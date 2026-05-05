// output-engine.cpp
// Zero-IRQ, zero-chain ring DMA topology.
//
// A single DMA channel reads 16-bit MCP4822 frames from a 128-byte ring buffer
// directly into the SPI data register. Read-side ring DMA wraps the read
// pointer in hardware every 128 bytes, so the channel runs autonomously with
// trans_count=0xFFFFFFFF (~14 hours). No render channel, no ctrl channel,
// no IRQ, no chain overhead. Frame rate is exactly the dma_timer's configured
// rate, locked to the ADC's audio rate (both derive from the system crystal).
//
// Buffer layout: 64 frames = 32 stereo pairs. buffer[2k] = A frame at pair k,
// buffer[2k+1] = B frame at pair k.
//
// Manual mode: hold value replicated into every channel-A or channel-B slot.
// Audio mode: each write_audio_sample call rewrites all 32 of the channel's
// slots with the new sample (~0.5 µs). The DMA reads each slot at most once
// per ring rotation (~11.5 µs at default rate), and writer rate matches DMA
// rate, so every distinct sample reaches the DAC.

#include "output-engine.h"

#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>
#include <hardware/sync.h>
#include <hardware/clocks.h>
#include <pico/stdlib.h>

namespace brain::internal {

namespace {

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

// 64 frames × 2 bytes = 128 bytes. DMA read-side ring with size_bits=7 wraps
// every 128 bytes, so the buffer's base address must be 128-byte aligned.
alignas(128) uint16_t streaming_buffer[64];
static_assert(sizeof(streaming_buffer) == 128, "ring DMA expects 128-byte buffer");

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

	// Initial buffer state: every A slot holds 0V hold frame, every B slot too.
	hold_frame_a_ = make_frame(mcp_base_a_, 0);
	hold_frame_b_ = make_frame(mcp_base_b_, 0);
	for (uint32_t k = 0; k < kStreamPairs; ++k) {
		streaming_buffer[k * 2] = hold_frame_a_;
		streaming_buffer[k * 2 + 1] = hold_frame_b_;
	}

	// Claim DMA pacing timer.
	dma_pacing_timer_ = dma_claim_unused_timer(false);
	if (dma_pacing_timer_ < 0) {
		return false;
	}

	// dma_timer fires at clk_sys * X / Y Hz. We want one DAC frame every
	// sample_period_us / 2 microseconds (each audio sample emits two frames:
	// one A, one B), i.e. a frame rate of 2_000_000 / sample_period_us Hz.
	// Setting X = 2 and Y = clk_sys * sample_period_us / 1_000_000 gives that
	// exactly: rate = clk_sys * 2 / (clk_sys * sample_period_us / 1_000_000)
	//             = 2_000_000 / sample_period_us.
	// Example: at clk_sys = 125 MHz, sample_period_us = 23 → Y = 2875,
	// frame rate = 125e6 * 2 / 2875 ≈ 86956 Hz, per-channel rate ≈ 43478 Hz.
	// The ADC's audio-mode round-robin runs at the same per-channel rate,
	// derived from the same crystal-PLL chain, so input and output stay
	// sample-locked with no long-term drift.
	const uint32_t clk_sys_hz = clock_get_hz(clk_sys);
	uint64_t denominator_64 =
		(static_cast<uint64_t>(clk_sys_hz) * cfg.sample_period_us + 500'000ull) /
		1'000'000ull;
	if (denominator_64 < 2) denominator_64 = 2;
	if (denominator_64 > 0xFFFF) denominator_64 = 0xFFFF;
	dma_timer_set_fraction(
		dma_pacing_timer_,
		2u,
		static_cast<uint16_t>(denominator_64));

	// Claim the single DMA channel.
	dma_data_chan_ = dma_claim_unused_channel(false);
	if (dma_data_chan_ < 0) {
		dma_timer_unclaim(dma_pacing_timer_);
		dma_pacing_timer_ = -1;
		return false;
	}

	// Data channel: 16-bit transfers from streaming_buffer -> SPI DR, paced by
	// DMA timer DREQ, with read-side ring wrap every 128 bytes (kRingSizeBits = 7).
	// trans_count = 0xFFFFFFFF so the channel runs autonomously for ~14 hours.
	dma_channel_config data_cfg = dma_channel_get_default_config(dma_data_chan_);
	channel_config_set_transfer_data_size(&data_cfg, DMA_SIZE_16);
	channel_config_set_read_increment(&data_cfg, true);
	channel_config_set_write_increment(&data_cfg, false);
	channel_config_set_dreq(&data_cfg, dma_get_timer_dreq(dma_pacing_timer_));
	channel_config_set_ring(&data_cfg, false /* ring on read */, kRingSizeBits);
	dma_channel_configure(
		dma_data_chan_,
		&data_cfg,
		&spi_get_hw(spi_)->dr,  // write addr: SPI data register (fixed)
		streaming_buffer,       // read addr: ring buffer
		0xFFFFFFFFu,            // ~14 hours of audio at 87 kHz before requiring re-arm
		false);

	// On RP2350, writing 0xFFFFFFFF to TRANS_COUNT sets the MODE field to 0xF
	// (ENDLESS) — the channel performs an endless DREQ-paced sequence with no
	// completion IRQ, no chain, no count decrement. Streams forever in hardware.
	// On RP2040 (no MODE field), 0xFFFFFFFF is just a 4-billion-transfer count;
	// the channel halts after ~14 hours of audio. If RP2040 long-run support is
	// needed, add an IRQ-based rearm here gated to RP2040 — but for normal use
	// either platform runs cleanly with no per-block IRQs.
	start_time_us_ = to_us_since_boot(get_absolute_time());
	dma_channel_start(dma_data_chan_);

	running_ = true;
	return true;
}

void OutputEngine::fill_channel_a_with(uint16_t frame) {
	for (uint32_t k = 0; k < kStreamPairs; ++k) {
		streaming_buffer[k * 2] = frame;
	}
}

void OutputEngine::fill_channel_b_with(uint16_t frame) {
	for (uint32_t k = 0; k < kStreamPairs; ++k) {
		streaming_buffer[k * 2 + 1] = frame;
	}
}

bool OutputEngine::set_hold_value(AudioCvOutChannel channel, uint16_t dac12) {
	const uint32_t saved_irq = save_and_disable_interrupts();
	if (channel == AudioCvOutChannel::kChannelA) {
		if (owner_a_ == ChannelOwner::kAudio) {
			restore_interrupts(saved_irq);
			return false;
		}
		hold_frame_a_ = make_frame(mcp_base_a_, dac12);
		fill_channel_a_with(hold_frame_a_);
	} else {
		if (owner_b_ == ChannelOwner::kAudio) {
			restore_interrupts(saved_irq);
			return false;
		}
		hold_frame_b_ = make_frame(mcp_base_b_, dac12);
		fill_channel_b_with(hold_frame_b_);
	}
	restore_interrupts(saved_irq);
	return true;
}

bool OutputEngine::write_audio_sample(AudioCvOutChannel channel, uint16_t dac12) {
	if (channel == AudioCvOutChannel::kChannelA) {
		if (owner_a_ != ChannelOwner::kAudio) {
			return false;
		}
		// Replicate the new value into every channel-A slot. Works for any
		// writer rate: at audio rate (~43 kHz, matching DMA), each push writes
		// 32 slots between successive DMA reads — the next read picks up the
		// new value, sample-by-sample. At slow rates (e.g. 5 kHz CV/control),
		// the value persists in all slots until the next push — clean stepped
		// output. Loop is ~32 × 16-bit stores ≈ 0.5 µs at 125/150 MHz.
		fill_channel_a_with(make_frame(mcp_base_a_, dac12));
	} else {
		if (owner_b_ != ChannelOwner::kAudio) {
			return false;
		}
		fill_channel_b_with(make_frame(mcp_base_b_, dac12));
	}
	return true;
}

void OutputEngine::set_channel_owner(AudioCvOutChannel channel, ChannelOwner owner) {
	const uint32_t saved_irq = save_and_disable_interrupts();
	if (channel == AudioCvOutChannel::kChannelA) {
		if (owner == ChannelOwner::kManual && owner_a_ == ChannelOwner::kAudio) {
			// Audio→Manual: refresh slots with the held value so the channel
			// snaps back to its last-set voltage cleanly.
			fill_channel_a_with(hold_frame_a_);
		}
		// Manual→Audio: nothing to do. The buffer already holds hold_frame_a in
		// every A slot (from manual mode); audio output continues at that value
		// until the writer's first `write_audio_sample` overwrites all slots.
		owner_a_ = owner;
	} else {
		if (owner == ChannelOwner::kManual && owner_b_ == ChannelOwner::kAudio) {
			fill_channel_b_with(hold_frame_b_);
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
	snapshot.audio_underrun_a = audio_underrun_a_;
	snapshot.audio_underrun_b = audio_underrun_b_;
	snapshot.audio_overflow_a = audio_overflow_a_;
	snapshot.audio_overflow_b = audio_overflow_b_;
	const bool running = running_;
	const uint64_t start_us = start_time_us_;
	const uint32_t period_us = cfg_.sample_period_us;
	restore_interrupts(saved_irq);

	if (running && period_us > 0) {
		const uint64_t now_us = to_us_since_boot(get_absolute_time());
		const uint64_t elapsed_us = now_us - start_us;
		// frames per second = 2_000_000 / sample_period_us.
		// total_frames = elapsed_us * 2 / sample_period_us.
		snapshot.total_frames = (elapsed_us * 2u + period_us / 2u) / period_us;
		snapshot.total_blocks = snapshot.total_frames / (kStreamPairs * 2);
	}

	return snapshot;
}

}  // namespace brain::internal
