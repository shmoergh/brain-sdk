// audio-processor.cpp
// Phase 3: AudioProcessor is a thin client over the shared AdcEngine + OutputEngine.
// On init() it switches the AdcEngine into audio mode at the requested
// sample_period_us, claims OutputEngine channel A as kAudio, and registers
// a per-sample callback that runs at the ADC IRQ rate (~43 kHz at default 23 µs).
// The callback runs the user's DSP and pushes the result into channel A's
// audio ring; OutputEngine's render IRQ drains the ring 16 samples per block.

#include "audio-processor.h"

#include <cstdio>

#include <pico/stdlib.h>
#include <hardware/sync.h>

#include "adc-engine.h"
#include "constants.h"
#include "gpio-setup.h"
#include "output-engine.h"
#include "outputs.h"
#include "pots-core.h"

namespace {

uint16_t clamp_u16(int32_t value, uint16_t min_value, uint16_t max_value) {
	if (value < static_cast<int32_t>(min_value)) return min_value;
	if (value > static_cast<int32_t>(max_value)) return max_value;
	return static_cast<uint16_t>(value);
}

constexpr uint16_t kDacMaxValue = 4095;

// AdcEngine converts PotsConfig.settling_delay_us into a discard-sample count
// using a fixed reference period (kPotSamplePeriodUs in adc-engine.cpp). Use
// the same period here so that an AudioProcessorConfig of N discard samples
// produces exactly N discard samples in the engine, regardless of mode.
constexpr uint32_t kAdcEnginePotSamplePeriodUs = 6;

}  // namespace

namespace brain::utils {

AudioProcessor::~AudioProcessor() {
	stop();
}

BrainInitStatus AudioProcessor::init(
	const AudioProcessorConfig& config,
	ProcessSampleFn process_sample_fn,
	void* user_ctx) {
	if (initialized_) return BrainInitStatus::kAlreadyInitialized;
	if (process_sample_fn == nullptr) {
		fprintf(stderr, "AudioProcessor: process callback is required\n");
		return BrainInitStatus::kFailed;
	}
	if (config.sample_period_us == 0) {
		fprintf(stderr, "AudioProcessor: sample_period_us must be > 0\n");
		return BrainInitStatus::kFailed;
	}

	config_ = config;
	if (config_.pot_count > AudioProcessorFrame::kMaxPots) {
		config_.pot_count = AudioProcessorFrame::kMaxPots;
	}
	if (config_.pot_count == 0) {
		config_.enable_pot_mux = false;
	}
	if (config_.pot_average_samples == 0) {
		config_.pot_average_samples = 1;
	}

	process_sample_fn_ = process_sample_fn;
	user_ctx_ = user_ctx;
	tick_count_ = 0;
	active_pot_count_ = config_.enable_pot_mux ? config_.pot_count : 0;

	// 1) Start OutputEngine (idempotent).
	brain::internal::OutputEngineConfig out_cfg;
	out_cfg.spi_instance = spi0;
	out_cfg.cs_gpio = kAudioCvOutCsPin;
	out_cfg.sck_gpio = kAudioCvOutSckPin;
	out_cfg.tx_gpio = kAudioCvOutTxPin;
	out_cfg.sample_period_us = config_.sample_period_us;
	if (!brain::internal::OutputEngine::instance().start(out_cfg)) {
		fprintf(stderr, "AudioProcessor: OutputEngine::start failed\n");
		stop();
		return BrainInitStatus::kFailed;
	}

	// AudioProcessor writes signed samples around 0; drive coupling pin A high
	// to select the bipolar -5..+5V range (matches legacy behavior).
	gpio_init(kAudioCvOutCouplingAPin);
	gpio_set_dir(kAudioCvOutCouplingAPin, GPIO_OUT);
	gpio_put(kAudioCvOutCouplingAPin, true);

	// 2) Configure AdcEngine with optional pot scanning.
	if (config_.enable_pot_mux) {
		PotsConfig pots_cfg = {};
		pots_cfg.simple = false;
		pots_cfg.adc_gpio = GPIO_BRAIN_POTMUX_ADC;
		pots_cfg.s0_gpio = GPIO_BRAIN_POTMUX_S0;
		pots_cfg.s1_gpio = GPIO_BRAIN_POTMUX_S1;
		pots_cfg.num_pots = config_.pot_count;
		for (uint8_t i = 0; i < kMaxPots; ++i) {
			pots_cfg.channel_map[i] = (i < config_.pot_count) ? i : 0;
		}
		pots_cfg.output_resolution = 8;
		pots_cfg.settling_delay_us =
			static_cast<uint32_t>(config_.pot_settle_discard_samples) *
			kAdcEnginePotSamplePeriodUs;
		pots_cfg.samples_per_read = config_.pot_average_samples;
		pots_cfg.change_threshold = 1;
		if (!brain::internal::AdcEngine::instance().enable_pots(pots_cfg)) {
			fprintf(stderr, "AudioProcessor: AdcEngine::enable_pots failed\n");
			stop();
			return BrainInitStatus::kFailed;
		}
	} else {
		if (!brain::internal::AdcEngine::instance().start()) {
			fprintf(stderr, "AudioProcessor: AdcEngine::start failed\n");
			stop();
			return BrainInitStatus::kFailed;
		}
	}

	// 3) Switch AdcEngine into audio mode (1-frame DMA, sample-rate IRQ).
	if (!brain::internal::AdcEngine::instance().enable_audio_mode(config_.sample_period_us)) {
		fprintf(stderr, "AudioProcessor: AdcEngine::enable_audio_mode failed\n");
		stop();
		return BrainInitStatus::kFailed;
	}
	audio_mode_started_ = true;

	// 4) Claim OutputEngine channel A as audio-owned. set_hold_value writes
	// from Outputs are now refused on channel A until stop() releases it.
	brain::internal::OutputEngine::instance().set_channel_owner(
		AudioCvOutChannel::kChannelA, brain::internal::ChannelOwner::kAudio);
	channel_a_claimed_ = true;

	// 5) Capture baseline underrun count so get_stats() reports overruns
	// accumulated during this run only.
	initial_underrun_a_ =
		brain::internal::OutputEngine::instance().get_snapshot().audio_underrun_a;

	// 6) Register the per-sample callback. Order matters: do this last so the
	// callback only fires once everything else is ready.
	brain::internal::AdcEngine::instance().set_audio_callback(
		&AudioProcessor::on_adc_sample_static, this);

	initialized_ = true;
	return BrainInitStatus::kOk;
}

void AudioProcessor::stop() {
	// No-op if this instance never claimed shared engine resources. Critical
	// because each `Brain` aggregates an `AudioProcessor` and unrelated `Brain`
	// instances (e.g. those created by guardrail tests) must not touch the
	// shared AdcEngine/OutputEngine state owned by the running instance when
	// they go out of scope.
	if (!initialized_ && !audio_mode_started_ && !channel_a_claimed_) {
		return;
	}

	// Drop the callback first so no IRQ touches our state during teardown.
	brain::internal::AdcEngine::instance().set_audio_callback(nullptr, nullptr);

	if (channel_a_claimed_) {
		brain::internal::OutputEngine::instance().set_channel_owner(
			AudioCvOutChannel::kChannelA, brain::internal::ChannelOwner::kManual);
		channel_a_claimed_ = false;
	}

	if (audio_mode_started_) {
		brain::internal::AdcEngine::instance().disable_audio_mode();
		audio_mode_started_ = false;
	}

	process_sample_fn_ = nullptr;
	user_ctx_ = nullptr;
	initialized_ = false;
}

bool AudioProcessor::is_initialized() const {
	return initialized_;
}

AudioProcessorStats AudioProcessor::get_stats() const {
	AudioProcessorStats stats{};

	const uint32_t irq_state = save_and_disable_interrupts();
	stats.tick_count = tick_count_;
	restore_interrupts(irq_state);

	const auto adc_snap = brain::internal::AdcEngine::instance().get_snapshot();
	stats.pot_mux_switch_count = adc_snap.pot_switch_count;
	stats.pot_settle_discard_count = adc_snap.pot_discard_count;

	const auto out_snap = brain::internal::OutputEngine::instance().get_snapshot();
	const uint32_t current_underrun = out_snap.audio_underrun_a;
	stats.overrun_count = (current_underrun >= initial_underrun_a_)
		? (current_underrun - initial_underrun_a_)
		: current_underrun;

	return stats;
}

uint16_t AudioProcessor::get_pot_raw_u8(uint8_t index) const {
	if (index >= AudioProcessorFrame::kMaxPots) return 0;

	const auto snap = brain::internal::AdcEngine::instance().get_snapshot();
	const uint32_t raw = snap.pot_raw[index] & kAdcMaxValue;
	return static_cast<uint16_t>(
		(raw * 255u + (kAdcMaxValue / 2)) / kAdcMaxValue);
}

void AudioProcessor::on_adc_sample_static(uint16_t in1_raw, uint16_t in2_raw,
                                           const brain::internal::AdcSnapshot& snap,
                                           void* ctx) {
	auto* self = static_cast<AudioProcessor*>(ctx);
	if (self == nullptr || !self->initialized_) return;
	self->on_adc_sample(in1_raw, in2_raw, snap);
}

void AudioProcessor::on_adc_sample(uint16_t in1_raw, uint16_t /*in2_raw*/,
                                    const brain::internal::AdcSnapshot& snap) {
	// Build the per-tick frame for the user callback.
	AudioProcessorFrame frame{};
	frame.tick = ++tick_count_;
	frame.pot_count = active_pot_count_;
	for (uint8_t i = 0; i < AudioProcessorFrame::kMaxPots; ++i) {
		const uint32_t raw = snap.pot_raw[i] & kAdcMaxValue;
		frame.pot_raw_u8[i] = static_cast<uint8_t>(
			(raw * 255u + (kAdcMaxValue / 2)) / kAdcMaxValue);
	}

	const int16_t input_sample = adc_raw_to_audio_sample(in1_raw);
	int16_t output_sample = input_sample;
	if (process_sample_fn_ != nullptr) {
		output_sample = process_sample_fn_(input_sample, &frame, user_ctx_);
	}

	const uint16_t dac_value = audio_sample_to_dac_value(output_sample);
	brain::internal::OutputEngine::instance().write_audio_sample(
		AudioCvOutChannel::kChannelA, dac_value);
}

int16_t AudioProcessor::adc_raw_to_audio_sample(uint16_t raw) {
	const int32_t centered = static_cast<int32_t>(raw & kAdcMaxValue) - 2048;
	return static_cast<int16_t>(centered << 4);
}

uint16_t AudioProcessor::audio_sample_to_dac_value(int16_t sample) {
	const int32_t unipolar = static_cast<int32_t>(sample) + 32768;
	const uint16_t clamped = clamp_u16(unipolar, 0, 65535);
	return clamp_u16(clamped >> 4, 0, kDacMaxValue);
}

}  // namespace brain::utils
