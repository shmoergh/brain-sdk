// adc-engine.cpp
// Single owner of the ADC. Continuous round-robin (POT, IN1, IN2) sampling at
// full speed via a self-looping DMA chain: a tiny "ctrl" channel rewrites the
// data channel's write pointer every cycle, so the buffer wraps in hardware
// with zero CPU work. The IRQ only walks the just-filled buffer to update the
// IN1/IN2 caches and run the pot settle/average state machine. Inputs/Pots
// read snapshots without locking. Mirrors the OutputEngine ctrl-DMA pattern.

#include "adc-engine.h"

#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/irq.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>

#include "gpio-setup.h"

namespace brain::internal {

namespace {

// ADC channel numbers correspond to GPIO - 26.
//   GPIO 26 -> ADC channel 0 = POT mux output
//   GPIO 27 -> ADC channel 1 = IN1 (audio/CV input A)
//   GPIO 28 -> ADC channel 2 = IN2 (audio/CV input B)
constexpr uint8_t kAdcChannelPot = 0;
constexpr uint8_t kAdcChannelIn1 = 1;
constexpr uint8_t kAdcChannelIn2 = 2;

// Round-robin samples a fixed three-channel frame per cycle.
// Hardware cycles enabled bits in increasing order, starting from the channel
// passed to adc_select_input(). We start at POT, so each frame is laid out:
//   frame[0] = POT, frame[1] = IN1, frame[2] = IN2.
constexpr uint32_t kSamplesPerFrame = 3;

// One DMA buffer holds this many round-robin frames. With ~6 µs per frame
// (3 channels × ~2 µs/sample at full ADC speed), 32 frames = ~192 µs per IRQ.
// Comfortable IRQ rate (~5 kHz) with low per-IRQ work.
constexpr uint32_t kFramesPerBuffer = 32;
constexpr uint32_t kSamplesPerBuffer = kFramesPerBuffer * kSamplesPerFrame;

// Each round-robin cycle through (POT, IN1, IN2) takes one sample per channel.
// At full ADC speed (~2 µs/sample), the full frame period is ~6 µs, so the
// effective sampling cadence for any one channel — including POT — is ~6 µs.
constexpr uint32_t kPotSamplePeriodUs = kSamplesPerFrame * 2;

// Single DMA destination buffer. Static storage so the DMA controller has a
// stable address and the IRQ handler can index it directly.
uint16_t adc_buffer[kSamplesPerBuffer];

// Stable storage for the buffer's base address. The ctrl DMA channel reads
// from this pointer and writes it into the data channel's write_addr register
// once per loop, restarting the buffer fill in hardware.
uint16_t* const adc_buffer_base = adc_buffer;

}  // namespace

AdcEngine& AdcEngine::instance() {
	static AdcEngine engine;
	return engine;
}

bool AdcEngine::start() {
	if (running_) {
		return true;
	}

	// ADC init + GPIO claim for all three channels.
	adc_init();
	adc_gpio_init(GPIO_BRAIN_POTMUX_ADC);
	adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_A);
	adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_B);

	// Full-speed continuous sampling, FIFO with DMA pacing.
	adc_set_clkdiv(0.0f);
	adc_fifo_setup(
		true,	// enable FIFO
		true,	// DMA request
		1,		// DREQ asserted when ≥1 sample is present
		false,	// no ERR bit
		false);	// 12-bit samples (ERR bit disabled means full 12 bits)
	adc_fifo_drain();
	adc_set_round_robin(
		(1u << kAdcChannelPot) |
		(1u << kAdcChannelIn1) |
		(1u << kAdcChannelIn2));
	// Start at POT so frame layout is deterministic: [POT, IN1, IN2].
	adc_select_input(kAdcChannelPot);

	// Claim two DMA channels: data (streams ADC FIFO into the buffer) + ctrl
	// (rewrites the data channel's write pointer to close the loop in hardware).
	dma_data_chan_ = dma_claim_unused_channel(false);
	if (dma_data_chan_ < 0) {
		return false;
	}
	dma_ctrl_chan_ = dma_claim_unused_channel(false);
	if (dma_ctrl_chan_ < 0) {
		dma_channel_unclaim(dma_data_chan_);
		dma_data_chan_ = -1;
		return false;
	}

	// Data channel: ADC FIFO -> adc_buffer, paced by DREQ_ADC. On completion
	// it chains to the ctrl channel, which immediately rewinds write_addr and
	// chains back here. trans_count is reloaded automatically from its trigger
	// register on each chain re-arm, so we never have to write it again.
	dma_channel_config data_cfg = dma_channel_get_default_config(dma_data_chan_);
	channel_config_set_transfer_data_size(&data_cfg, DMA_SIZE_16);
	channel_config_set_read_increment(&data_cfg, false);
	channel_config_set_write_increment(&data_cfg, true);
	channel_config_set_dreq(&data_cfg, DREQ_ADC);
	channel_config_set_chain_to(&data_cfg, dma_ctrl_chan_);
	dma_channel_configure(
		dma_data_chan_,
		&data_cfg,
		adc_buffer,				// write addr (auto-incremented)
		&adc_hw->fifo,			// read addr (fixed)
		kSamplesPerBuffer,
		false);					// don't trigger yet

	// Ctrl channel: a single 32-bit transfer that writes adc_buffer's base
	// address into the data channel's write_addr register, then chains back
	// to the data channel which restarts. No DREQ — fires immediately when
	// chain-triggered. No IRQ — invisible to the CPU.
	dma_channel_config ctrl_cfg = dma_channel_get_default_config(dma_ctrl_chan_);
	channel_config_set_transfer_data_size(&ctrl_cfg, DMA_SIZE_32);
	channel_config_set_read_increment(&ctrl_cfg, false);
	channel_config_set_write_increment(&ctrl_cfg, false);
	channel_config_set_chain_to(&ctrl_cfg, dma_data_chan_);
	dma_channel_configure(
		dma_ctrl_chan_,
		&ctrl_cfg,
		&dma_hw->ch[dma_data_chan_].write_addr,	// write addr: data channel's write_addr register
		&adc_buffer_base,						// read addr: pointer holding adc_buffer
		1,
		false);

	// Only the data channel raises an IRQ; the ctrl channel is silent.
	dma_channel_set_irq0_enabled(dma_data_chan_, true);
	irq_set_exclusive_handler(DMA_IRQ_0, &AdcEngine::dma_irq_handler_static);
	irq_set_enabled(DMA_IRQ_0, true);

	// Start: trigger data channel, then ADC. Hardware loops forever after this.
	dma_channel_start(dma_data_chan_);
	adc_run(true);

	running_ = true;
	return true;
}

bool AdcEngine::enable_pots(const PotsConfig& pots_config) {
	if (!start()) {
		return false;
	}

	// Mux GPIOs as outputs; safe to init outside the IRQ-critical region.
	gpio_init(pots_config.s0_gpio);
	gpio_set_dir(pots_config.s0_gpio, GPIO_OUT);
	gpio_init(pots_config.s1_gpio);
	gpio_set_dir(pots_config.s1_gpio, GPIO_OUT);

	// Atomic swap of pot config + state-machine enable under disabled interrupts.
	const uint32_t saved_irq = save_and_disable_interrupts();
	mux_s0_gpio_ = pots_config.s0_gpio;
	mux_s1_gpio_ = pots_config.s1_gpio;
	apply_pot_scan_config(pots_config);
	reset_pot_state_machine();
	switch_mux_to(pot_current_index_);
	pot_scanning_enabled_ = true;
	restore_interrupts(saved_irq);

	return true;
}

void AdcEngine::reconfigure_pots(const PotsConfig& pots_config) {
	// Atomic swap under disabled interrupts so the IRQ never sees a partial config.
	const uint32_t saved_irq = save_and_disable_interrupts();
	mux_s0_gpio_ = pots_config.s0_gpio;
	mux_s1_gpio_ = pots_config.s1_gpio;
	apply_pot_scan_config(pots_config);
	reset_pot_state_machine();
	switch_mux_to(pot_current_index_);
	pot_scanning_enabled_ = true;
	restore_interrupts(saved_irq);
}

AdcSnapshot AdcEngine::get_snapshot() const {
	AdcSnapshot snapshot;
	const uint32_t saved_irq = save_and_disable_interrupts();
	snapshot.in1_raw = latest_in1_raw_;
	snapshot.in2_raw = latest_in2_raw_;
	for (uint8_t i = 0; i < kMaxPots; ++i) {
		snapshot.pot_raw[i] = latest_pot_raw_[i];
	}
	snapshot.total_samples = total_samples_;
	snapshot.pot_switch_count = pot_switch_count_;
	snapshot.pot_discard_count = pot_discard_count_;
	restore_interrupts(saved_irq);
	return snapshot;
}

void AdcEngine::apply_pot_scan_config(const PotsConfig& pots_config) {
	uint8_t count = pots_config.num_pots;
	if (count > kMaxPots) count = kMaxPots;
	if (count == 0) count = 1;
	pot_count_ = count;

	for (uint8_t i = 0; i < kMaxPots; ++i) {
		pot_channel_map_[i] = (i < count) ? pots_config.channel_map[i] : 0;
	}

	const uint32_t settling = (pots_config.settling_delay_us == 0)
		? kPotSamplePeriodUs
		: pots_config.settling_delay_us;
	uint32_t derived_settling = settling / kPotSamplePeriodUs;
	if (derived_settling < 1) derived_settling = 1;
	if (derived_settling > 0xFFFF) derived_settling = 0xFFFF;
	pot_settling_samples_ = static_cast<uint16_t>(derived_settling);

	uint16_t average = pots_config.samples_per_read;
	if (average == 0) average = 1;
	pot_average_samples_ = average;
}

void AdcEngine::reset_pot_state_machine() {
	pot_current_index_ = 0;
	pot_state_ = kPotStateSettling;
	pot_settling_samples_remaining_ = pot_settling_samples_;
	pot_average_samples_remaining_ = 0;
	pot_average_accumulator_ = 0;
}

void AdcEngine::switch_mux_to(uint8_t logical_pot_index) {
	const uint8_t mux_channel = pot_channel_map_[logical_pot_index] & 0x03;
	gpio_put(mux_s0_gpio_, mux_channel & 0x01);
	gpio_put(mux_s1_gpio_, (mux_channel >> 1) & 0x01);
}

void AdcEngine::dma_irq_handler_static() {
	AdcEngine::instance().on_dma_irq();
}

void AdcEngine::on_dma_irq() {
	if ((dma_hw->ints0 & (1u << dma_data_chan_)) == 0) {
		return;
	}
	dma_hw->ints0 = 1u << dma_data_chan_;  // clear (write-1-to-clear)

	// The ctrl channel has already rewound write_addr and re-armed the data
	// channel via chain_to, so the next buffer cycle is already in flight.
	// We have ~192 µs of margin (one full buffer period) to walk this buffer
	// before the writer overtakes us — well beyond per-IRQ cost.
	for (uint32_t frame = 0; frame < kFramesPerBuffer; ++frame) {
		const uint16_t* samples = adc_buffer + (frame * kSamplesPerFrame);
		const uint16_t pot_sample = samples[0];
		const uint16_t in1_sample = samples[1];
		const uint16_t in2_sample = samples[2];

		latest_in1_raw_ = in1_sample;
		latest_in2_raw_ = in2_sample;
		total_samples_ += kSamplesPerFrame;

		// Pot state machine only runs when pot scanning has been enabled via
		// enable_pots(). Without it, the POT slot is sampled (for deterministic
		// frame layout) but discarded.
		if (!pot_scanning_enabled_) {
			continue;
		}

		// Pot scanner state machine.
		if (pot_state_ == kPotStateSettling) {
			pot_discard_count_++;
			if (pot_settling_samples_remaining_ > 0) {
				pot_settling_samples_remaining_--;
			}
			if (pot_settling_samples_remaining_ == 0) {
				pot_state_ = kPotStateAveraging;
				pot_average_samples_remaining_ = pot_average_samples_;
				pot_average_accumulator_ = 0;
			}
			// Note: the sample that triggers the settling->averaging transition
			// is intentionally discarded. The first averaged sample is the next
			// pot sample to arrive, guaranteeing a full settle window.
		} else {
			pot_average_accumulator_ += pot_sample;
			if (pot_average_samples_remaining_ > 0) {
				pot_average_samples_remaining_--;
			}
			if (pot_average_samples_remaining_ == 0) {
				const uint16_t averaged = static_cast<uint16_t>(
					pot_average_accumulator_ / pot_average_samples_);
				latest_pot_raw_[pot_current_index_] = averaged;

				// Advance to next logical pot, switch external mux, restart settling.
				pot_current_index_ = (pot_current_index_ + 1) % pot_count_;
				switch_mux_to(pot_current_index_);
				pot_switch_count_++;

				pot_state_ = kPotStateSettling;
				pot_settling_samples_remaining_ = pot_settling_samples_;
			}
		}
	}
}

}  // namespace brain::internal
