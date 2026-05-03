// output-engine.cpp
// Single owner of the MCP4822 DAC. Streams interleaved (A, B) 16-bit frames into
// the SPI peripheral via three chained DMA channels paced by a hardware DMA
// timer. The PL022 in 16-bit Motorola frame format with CS routed through
// GPIO_FUNC_SPI toggles CS between frames automatically; MCP4822 latches its
// input register on each rising CS edge.
//
// Phase 3 changes the audio path from a single replicated frame per block to a
// per-channel sample ring filled by `write_audio_sample` and drained 16
// samples-per-block by the render IRQ. The render IRQ still runs at block
// rate (~2.7 kHz) regardless of mode, but in pure-Manual mode it does no
// useful work — current_block already holds the right hold values via
// `refill_current_block`'s manual path.

#include "output-engine.h"

#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/spi.h>
#include <hardware/sync.h>
#include <hardware/clocks.h>
#include <pico/stdlib.h>

namespace brain::internal {

namespace {

// Block layout: kBlockSizeFrames frames, interleaved A/B per audio sample.
//   block[0] = A_0, block[1] = B_0, block[2] = A_1, block[3] = B_1, ...
// kAudioBlockSamples (= 16) sample pairs per block.
constexpr uint32_t kBlockSizeFrames = OutputEngine::kAudioBlockSamples * 2;

// MCP4822 16-bit frame envelope (high nibble = config, low 12 bits = data):
//   bit 15: A/B  (0 = channel A, 1 = channel B)
//   bit 14: BUF  (0 = unbuffered)
//   bit 13: GAIN (0 = 2x gain, matches existing Brain analog chain)
//   bit 12: SHDN (1 = output active)
constexpr uint16_t kMcp4822FrameNibbleA = 0b0001;
constexpr uint16_t kMcp4822FrameNibbleB = 0b1001;

inline uint16_t make_frame(uint16_t base_nibble, uint16_t dac12) {
	return static_cast<uint16_t>((base_nibble << 12) | (dac12 & 0x0FFF));
}

// DMA buffers. Static storage so the DMA controller has stable addresses.
uint16_t streaming_block[kBlockSizeFrames];
uint16_t current_block[kBlockSizeFrames];

}  // namespace

OutputEngine& OutputEngine::instance() {
	static OutputEngine engine;
	return engine;
}

bool OutputEngine::start(const OutputEngineConfig& cfg) {
	if (running_) {
		return true;
	}

	cfg_ = cfg;
	spi_ = cfg.spi_instance;
	mcp_base_a_ = kMcp4822FrameNibbleA;
	mcp_base_b_ = kMcp4822FrameNibbleB;

	// SPI: 16-bit Motorola frames. CS routed through SPI peripheral so it
	// toggles between frames automatically (one rising edge per 16-bit frame).
	spi_init(spi_, cfg.spi_baud_hz);
	spi_set_format(spi_, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
	gpio_set_function(cfg.sck_gpio, GPIO_FUNC_SPI);
	gpio_set_function(cfg.tx_gpio, GPIO_FUNC_SPI);
	gpio_set_function(cfg.cs_gpio, GPIO_FUNC_SPI);

	// Pre-fill both buffers with current (zero) hold values so the very first
	// frames out are valid MCP4822 frames at 0V.
	hold_frame_a_ = make_frame(mcp_base_a_, 0);
	hold_frame_b_ = make_frame(mcp_base_b_, 0);
	for (uint32_t sample = 0; sample < kAudioBlockSamples; ++sample) {
		const uint32_t idx_a = sample * 2;
		const uint32_t idx_b = idx_a + 1;
		streaming_block[idx_a] = hold_frame_a_;
		streaming_block[idx_b] = hold_frame_b_;
		current_block[idx_a] = hold_frame_a_;
		current_block[idx_b] = hold_frame_b_;
	}

	// Claim DMA pacing timer.
	dma_pacing_timer_ = dma_claim_unused_timer(false);
	if (dma_pacing_timer_ < 0) {
		return false;
	}

	// Frame rate = 2 / sample_period_us (one frame per channel per sample period).
	// dma_timer fires at (X / Y) * clk_sys; pick X = 1 and Y = clk_sys / frame_rate_hz.
	const uint32_t clk_sys_hz = clock_get_hz(clk_sys);
	const uint64_t target_frame_rate_hz =
		(2ull * 1'000'000ull + (cfg.sample_period_us / 2)) / cfg.sample_period_us;
	uint32_t denominator = static_cast<uint32_t>(
		(static_cast<uint64_t>(clk_sys_hz) + (target_frame_rate_hz / 2)) /
		target_frame_rate_hz);
	if (denominator < 1) denominator = 1;
	if (denominator > 0xFFFF) denominator = 0xFFFF;
	dma_timer_set_fraction(
		dma_pacing_timer_,
		1u,
		static_cast<uint16_t>(denominator));

	// Claim the three DMA channels.
	dma_data_chan_ = dma_claim_unused_channel(false);
	if (dma_data_chan_ < 0) {
		dma_timer_unclaim(dma_pacing_timer_);
		dma_pacing_timer_ = -1;
		return false;
	}
	dma_ctrl_chan_ = dma_claim_unused_channel(false);
	if (dma_ctrl_chan_ < 0) {
		dma_channel_unclaim(dma_data_chan_);
		dma_data_chan_ = -1;
		dma_timer_unclaim(dma_pacing_timer_);
		dma_pacing_timer_ = -1;
		return false;
	}
	dma_render_chan_ = dma_claim_unused_channel(false);
	if (dma_render_chan_ < 0) {
		dma_channel_unclaim(dma_ctrl_chan_);
		dma_channel_unclaim(dma_data_chan_);
		dma_ctrl_chan_ = -1;
		dma_data_chan_ = -1;
		dma_timer_unclaim(dma_pacing_timer_);
		dma_pacing_timer_ = -1;
		return false;
	}

	// Stable pointer the ctrl channel reads from each cycle to re-seed data.read_addr.
	streaming_block_read_addr_ = streaming_block;

	// Render channel: copy current_block -> streaming_block, then chain to ctrl.
	dma_channel_config render_cfg = dma_channel_get_default_config(dma_render_chan_);
	channel_config_set_transfer_data_size(&render_cfg, DMA_SIZE_16);
	channel_config_set_read_increment(&render_cfg, true);
	channel_config_set_write_increment(&render_cfg, true);
	channel_config_set_chain_to(&render_cfg, dma_ctrl_chan_);
	dma_channel_configure(
		dma_render_chan_,
		&render_cfg,
		streaming_block,        // write addr
		current_block,          // read addr
		kBlockSizeFrames,
		false);

	// Ctrl channel: write streaming_block address into data.read_addr (one 32-bit
	// transfer), then chain to data.
	dma_channel_config ctrl_cfg = dma_channel_get_default_config(dma_ctrl_chan_);
	channel_config_set_transfer_data_size(&ctrl_cfg, DMA_SIZE_32);
	channel_config_set_read_increment(&ctrl_cfg, false);
	channel_config_set_write_increment(&ctrl_cfg, false);
	channel_config_set_chain_to(&ctrl_cfg, dma_data_chan_);
	dma_channel_configure(
		dma_ctrl_chan_,
		&ctrl_cfg,
		&dma_hw->ch[dma_data_chan_].read_addr,  // write addr: data channel's read_addr
		&streaming_block_read_addr_,            // read addr: pointer holding &streaming_block[0]
		1,
		false);

	// Data channel: stream streaming_block -> SPI DR, paced by DMA timer DREQ,
	// chain back to render.
	dma_channel_config data_cfg = dma_channel_get_default_config(dma_data_chan_);
	channel_config_set_transfer_data_size(&data_cfg, DMA_SIZE_16);
	channel_config_set_read_increment(&data_cfg, true);
	channel_config_set_write_increment(&data_cfg, false);
	channel_config_set_dreq(&data_cfg, dma_get_timer_dreq(dma_pacing_timer_));
	channel_config_set_chain_to(&data_cfg, dma_render_chan_);
	dma_channel_configure(
		dma_data_chan_,
		&data_cfg,
		&spi_get_hw(spi_)->dr,  // write addr: SPI data register
		streaming_block,        // read addr: streaming buffer
		kBlockSizeFrames,
		false);

	// Render IRQ on completion of render channel. Engine uses DMA_IRQ_1 to avoid
	// conflict with AdcEngine's DMA_IRQ_0.
	dma_channel_set_irq1_enabled(dma_render_chan_, true);
	irq_set_exclusive_handler(DMA_IRQ_1, &OutputEngine::dma_irq_handler_static);
	irq_set_enabled(DMA_IRQ_1, true);

	// Start the chain at the data channel: it consumes streaming_block (already
	// pre-filled with zero frames) at the timer rate. When done, chains to render
	// which copies current_block -> streaming_block, fires IRQ, chains to ctrl
	// which re-seeds data.read_addr, chains back to data.
	dma_channel_start(dma_data_chan_);

	running_ = true;
	return true;
}

bool OutputEngine::set_hold_value(AudioCvOutChannel channel, uint16_t dac12) {
	const uint32_t saved_irq = save_and_disable_interrupts();
	if (channel == AudioCvOutChannel::kChannelA) {
		if (owner_a_ == ChannelOwner::kAudio) {
			restore_interrupts(saved_irq);
			return false;
		}
		hold_frame_a_ = make_frame(mcp_base_a_, dac12);
	} else {
		if (owner_b_ == ChannelOwner::kAudio) {
			restore_interrupts(saved_irq);
			return false;
		}
		hold_frame_b_ = make_frame(mcp_base_b_, dac12);
	}
	restore_interrupts(saved_irq);
	return true;
}

bool OutputEngine::write_audio_sample(AudioCvOutChannel channel, uint16_t dac12) {
	if (channel == AudioCvOutChannel::kChannelA) {
		if (owner_a_ != ChannelOwner::kAudio) {
			return false;
		}
		const uint8_t head = audio_ring_a_head_;
		audio_ring_a_[head & (kAudioRingSamples - 1)] = make_frame(mcp_base_a_, dac12);
		audio_ring_a_head_ = static_cast<uint8_t>(head + 1);
	} else {
		if (owner_b_ != ChannelOwner::kAudio) {
			return false;
		}
		const uint8_t head = audio_ring_b_head_;
		audio_ring_b_[head & (kAudioRingSamples - 1)] = make_frame(mcp_base_b_, dac12);
		audio_ring_b_head_ = static_cast<uint8_t>(head + 1);
	}
	return true;
}

void OutputEngine::set_channel_owner(AudioCvOutChannel channel, ChannelOwner owner) {
	const uint32_t saved_irq = save_and_disable_interrupts();
	if (channel == AudioCvOutChannel::kChannelA) {
		if (owner == ChannelOwner::kAudio && owner_a_ != ChannelOwner::kAudio) {
			seed_ring_with_hold(channel);
		}
		owner_a_ = owner;
	} else {
		if (owner == ChannelOwner::kAudio && owner_b_ != ChannelOwner::kAudio) {
			seed_ring_with_hold(channel);
		}
		owner_b_ = owner;
	}
	restore_interrupts(saved_irq);
}

ChannelOwner OutputEngine::get_channel_owner(AudioCvOutChannel channel) const {
	const uint32_t saved_irq = save_and_disable_interrupts();
	const ChannelOwner owner =
		(channel == AudioCvOutChannel::kChannelA) ? owner_a_ : owner_b_;
	restore_interrupts(saved_irq);
	return owner;
}

OutputEngineSnapshot OutputEngine::get_snapshot() const {
	OutputEngineSnapshot snapshot;
	const uint32_t saved_irq = save_and_disable_interrupts();
	snapshot.hold_frame_a = hold_frame_a_;
	snapshot.hold_frame_b = hold_frame_b_;
	snapshot.owner_a = owner_a_;
	snapshot.owner_b = owner_b_;
	snapshot.total_blocks = total_blocks_;
	snapshot.total_frames = total_blocks_ * kBlockSizeFrames;
	snapshot.audio_underrun_a = audio_underrun_a_;
	snapshot.audio_underrun_b = audio_underrun_b_;
	restore_interrupts(saved_irq);
	return snapshot;
}

void OutputEngine::dma_irq_handler_static() {
	OutputEngine::instance().on_dma_irq();
}

void OutputEngine::on_dma_irq() {
	if ((dma_hw->ints1 & (1u << dma_render_chan_)) == 0) {
		return;
	}
	dma_hw->ints1 = 1u << dma_render_chan_;  // clear (write-1-to-clear)

	// Render just finished copying current_block -> streaming_block; ctrl will
	// chain to re-seed data.read_addr, and data starts streaming the new block.
	// Reset render's read/write addresses for the next cycle (they advanced to
	// the end of each buffer during the copy).
	dma_channel_set_read_addr(dma_render_chan_, current_block, false);
	dma_channel_set_write_addr(dma_render_chan_, streaming_block, false);

	// Refill current_block with the next batch of frames so it's ready when the
	// data channel finishes the current streaming_block.
	refill_current_block();
	++total_blocks_;
}

void OutputEngine::refill_current_block() {
	const bool a_audio = (owner_a_ == ChannelOwner::kAudio);
	const bool b_audio = (owner_b_ == ChannelOwner::kAudio);

	bool a_underrun_this_block = false;
	bool b_underrun_this_block = false;

	for (uint32_t sample = 0; sample < kAudioBlockSamples; ++sample) {
		uint16_t frame_a;
		if (a_audio) {
			if (audio_ring_a_tail_ != audio_ring_a_head_) {
				frame_a = audio_ring_a_[audio_ring_a_tail_ & (kAudioRingSamples - 1)];
				audio_ring_a_tail_ = static_cast<uint8_t>(audio_ring_a_tail_ + 1);
				last_consumed_a_ = frame_a;
			} else {
				frame_a = last_consumed_a_;
				a_underrun_this_block = true;
			}
		} else {
			frame_a = hold_frame_a_;
		}

		uint16_t frame_b;
		if (b_audio) {
			if (audio_ring_b_tail_ != audio_ring_b_head_) {
				frame_b = audio_ring_b_[audio_ring_b_tail_ & (kAudioRingSamples - 1)];
				audio_ring_b_tail_ = static_cast<uint8_t>(audio_ring_b_tail_ + 1);
				last_consumed_b_ = frame_b;
			} else {
				frame_b = last_consumed_b_;
				b_underrun_this_block = true;
			}
		} else {
			frame_b = hold_frame_b_;
		}

		const uint32_t idx_a = sample * 2;
		const uint32_t idx_b = idx_a + 1;
		current_block[idx_a] = frame_a;
		current_block[idx_b] = frame_b;
	}

	if (a_underrun_this_block) ++audio_underrun_a_;
	if (b_underrun_this_block) ++audio_underrun_b_;
}

void OutputEngine::seed_ring_with_hold(AudioCvOutChannel channel) {
	if (channel == AudioCvOutChannel::kChannelA) {
		audio_ring_a_head_ = 0;
		audio_ring_a_tail_ = 0;
		last_consumed_a_ = hold_frame_a_;
		for (uint32_t i = 0; i < kAudioBlockSamples; ++i) {
			audio_ring_a_[i & (kAudioRingSamples - 1)] = hold_frame_a_;
		}
		audio_ring_a_head_ = static_cast<uint8_t>(kAudioBlockSamples);
	} else {
		audio_ring_b_head_ = 0;
		audio_ring_b_tail_ = 0;
		last_consumed_b_ = hold_frame_b_;
		for (uint32_t i = 0; i < kAudioBlockSamples; ++i) {
			audio_ring_b_[i & (kAudioRingSamples - 1)] = hold_frame_b_;
		}
		audio_ring_b_head_ = static_cast<uint8_t>(kAudioBlockSamples);
	}
}

}  // namespace brain::internal
