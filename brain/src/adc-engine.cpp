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
	// In audio mode this IRQ has a 7.67 µs deadline to read adc_buffer before
	// DMA starts overwriting it for the next cycle. Bump it to highest priority
	// (0 on Cortex-M) so USB / other IRQs cannot delay it past that window.
	dma_channel_set_irq0_enabled(dma_data_chan_, true);
	irq_set_exclusive_handler(DMA_IRQ_0, &AdcEngine::dma_irq_handler_static);
	irq_set_priority(DMA_IRQ_0, 0);
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
	snapshot = build_snapshot_inline();
	restore_interrupts(saved_irq);
	return snapshot;
}

AdcSnapshot AdcEngine::build_snapshot_inline() const {
	// Caller must hold disabled-IRQ (or be running inside the DMA IRQ itself,
	// which is the sole writer of the cached state).
	AdcSnapshot snapshot;
	snapshot.in1_raw = latest_in1_raw_;
	snapshot.in2_raw = latest_in2_raw_;
	for (uint8_t i = 0; i < kMaxPots; ++i) {
		snapshot.pot_raw[i] = latest_pot_raw_[i];
	}
	snapshot.total_samples = total_samples_;
	snapshot.pot_switch_count = pot_switch_count_;
	snapshot.pot_discard_count = pot_discard_count_;
	return snapshot;
}

bool AdcEngine::enable_audio_mode(uint32_t sample_period_us) {
	if (sample_period_us == 0) return false;
	if (!start()) return false;

	const uint32_t saved_irq = save_and_disable_interrupts();

	// Halt sampling and DMA so we can reconfigure cleanly without races.
	adc_run(false);
	// adc_run(false) only clears START_MANY; an in-flight conversion still
	// completes and advances AINSEL asynchronously. A conversion is at most
	// 96 ADC clock cycles (2 µs at 48 MHz). Wait a bit longer than that so
	// any in-flight conversion finishes before we re-seat AINSEL below.
	busy_wait_us(5);
	dma_channel_abort(dma_data_chan_);
	adc_fifo_drain();

	// Tune ADC clkdiv so each round-robin frame [POT, IN1, IN2] takes
	// sample_period_us. Per-sample period = sample_period_us / kSamplesPerFrame.
	// adc_set_clkdiv argument is "(48 MHz / target sample rate) - 1".
	const float per_sample_us =
		static_cast<float>(sample_period_us) / static_cast<float>(kSamplesPerFrame);
	float clkdiv = 48.0f * per_sample_us - 1.0f;
	if (clkdiv < 0.0f) clkdiv = 0.0f;
	adc_set_clkdiv(clkdiv);

	// Also abort the ctrl channel and clear any pending IRQ status to avoid
	// stale state from the just-aborted CV-mode cycle interfering with the
	// audio-mode restart.
	dma_channel_abort(dma_ctrl_chan_);
	dma_hw->ints0 = 1u << dma_data_chan_;

	// Reset the ADC round-robin pointer back to POT so each 3-sample audio
	// frame lays out as [POT, IN1, IN2] deterministically. Without this, the
	// pointer is wherever it stopped at adc_run(false), which can leave the
	// frame mis-aligned (e.g. [IN1, IN2, POT]) and the DSP would receive
	// IN2 instead of IN1.
	adc_select_input(kAdcChannelPot);

	// Shrink DMA transfer count to a single frame and re-arm write_addr.
	// trans_count is reloaded from the data channel's TRANS_COUNT register on
	// each ctrl chain restart, so a one-time write here applies to every cycle.
	dma_channel_set_trans_count(dma_data_chan_, kSamplesPerFrame, false);
	dma_channel_set_write_addr(dma_data_chan_, adc_buffer, false);

	audio_mode_enabled_ = true;

	// Restart the chain.
	dma_channel_start(dma_data_chan_);
	adc_run(true);

	restore_interrupts(saved_irq);
	return true;
}

void AdcEngine::disable_audio_mode() {
	if (!audio_mode_enabled_) return;

	const uint32_t saved_irq = save_and_disable_interrupts();

	adc_run(false);
	busy_wait_us(5);
	dma_channel_abort(dma_data_chan_);
	dma_channel_abort(dma_ctrl_chan_);
	adc_fifo_drain();
	dma_hw->ints0 = 1u << dma_data_chan_;
	adc_select_input(kAdcChannelPot);

	// Revert to full-speed ADC + 32-frame buffer.
	adc_set_clkdiv(0.0f);
	dma_channel_set_trans_count(dma_data_chan_, kSamplesPerBuffer, false);
	dma_channel_set_write_addr(dma_data_chan_, adc_buffer, false);

	audio_mode_enabled_ = false;
	audio_callback_ = nullptr;
	audio_ctx_ = nullptr;

	dma_channel_start(dma_data_chan_);
	adc_run(true);

	restore_interrupts(saved_irq);
}

void AdcEngine::set_audio_callback(AdcAudioCallback callback, void* ctx) {
	const uint32_t saved_irq = save_and_disable_interrupts();
	audio_callback_ = callback;
	audio_ctx_ = ctx;
	restore_interrupts(saved_irq);
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
	// Settling must cover at least one full DMA buffer period plus one sample
	// of margin. The IRQ processes a buffer (`kFramesPerBuffer` pot samples)
	// that was captured before the IRQ fired; when the state machine completes
	// an average mid-buffer and switches the mux, the remaining frames in the
	// SAME buffer were captured under the old mux setting. If the settle
	// window is shorter than the buffer, the first averaging sample for the
	// next pot can land on one of those stale frames — pot N's average then
	// gets contaminated with pot N-1's voltage. Clamping to `kFramesPerBuffer
	// + 1` guarantees the first averaging sample is always in a fresh buffer
	// captured after the mux GPIO change took effect.
	constexpr uint32_t kMinSettlingSamples = kFramesPerBuffer + 1;
	if (derived_settling < kMinSettlingSamples) derived_settling = kMinSettlingSamples;
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

void AdcEngine::run_pot_scanner_one_sample(uint16_t pot_sample) {
	// Caller is the DMA IRQ. Advances the pot settle/average state machine by
	// one POT sample. No-op if scanning hasn't been enabled.
	if (!pot_scanning_enabled_) return;

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

void AdcEngine::on_dma_irq() {
	if ((dma_hw->ints0 & (1u << dma_data_chan_)) == 0) {
		return;
	}
	dma_hw->ints0 = 1u << dma_data_chan_;  // clear (write-1-to-clear)

	if (audio_mode_enabled_) {
		// Audio mode: one frame per IRQ at sample rate (~43 kHz at 23 µs).
		// The ctrl channel has already re-armed data with trans_count = 3, so
		// the next frame is already being filled. We must finish before that
		// next frame completes (~23 µs).
		const uint16_t pot_sample = adc_buffer[0];
		const uint16_t in1_sample = adc_buffer[1];
		const uint16_t in2_sample = adc_buffer[2];

		latest_in1_raw_ = in1_sample;
		latest_in2_raw_ = in2_sample;
		total_samples_ += kSamplesPerFrame;

		run_pot_scanner_one_sample(pot_sample);

		if (audio_callback_ != nullptr) {
			audio_callback_(in1_sample, in2_sample, build_snapshot_inline(), audio_ctx_);
		}
		return;
	}

	// CV mode: walk the just-completed 32-frame buffer. We have ~192 µs of
	// margin (one full buffer period) before the writer overtakes us.
	for (uint32_t frame = 0; frame < kFramesPerBuffer; ++frame) {
		const uint16_t* samples = adc_buffer + (frame * kSamplesPerFrame);
		const uint16_t pot_sample = samples[0];
		const uint16_t in1_sample = samples[1];
		const uint16_t in2_sample = samples[2];

		latest_in1_raw_ = in1_sample;
		latest_in2_raw_ = in2_sample;
		total_samples_ += kSamplesPerFrame;

		run_pot_scanner_one_sample(pot_sample);
	}
}

}  // namespace brain::internal
