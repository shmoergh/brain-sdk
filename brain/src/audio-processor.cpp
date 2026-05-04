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
	process_frame_fn_v2_ = nullptr;
	user_ctx_ = user_ctx;
	mode_v2_ = false;

	const EngineSetup setup{
		config_.sample_period_us,
		config_.enable_pot_mux,
		config_.pot_count,
		config_.pot_settle_discard_samples,
		config_.pot_average_samples,
		/*claim_channel_a=*/true,
		/*claim_channel_b=*/false,
	};
	return start_engines(setup);
}

BrainInitStatus AudioProcessor::init_v2(
	const AudioProcessorConfigV2& config,
	ProcessFrameFnV2 process_frame_fn,
	void* user_ctx) {
	if (initialized_) return BrainInitStatus::kAlreadyInitialized;
	if (process_frame_fn == nullptr) {
		fprintf(stderr, "AudioProcessor: v2 process callback is required\n");
		return BrainInitStatus::kFailed;
	}
	if (config.sample_rate_hz == 0) {
		fprintf(stderr, "AudioProcessor: v2 sample_rate_hz must be > 0\n");
		return BrainInitStatus::kFailed;
	}
	if (!config.claim_channel_a && !config.claim_channel_b) {
		fprintf(stderr, "AudioProcessor: v2 must claim at least one output channel\n");
		return BrainInitStatus::kFailed;
	}

	const uint32_t derived_period_us = 1'000'000u / config.sample_rate_hz;
	if (derived_period_us == 0) {
		fprintf(stderr, "AudioProcessor: v2 sample_rate_hz too high (period rounds to 0)\n");
		return BrainInitStatus::kFailed;
	}

	uint8_t pot_count = config.pot_count;
	if (pot_count > AudioProcessorFrame::kMaxPots) {
		pot_count = AudioProcessorFrame::kMaxPots;
	}
	bool enable_pot_mux = config.enable_pot_mux && (pot_count > 0);

	process_sample_fn_ = nullptr;
	process_frame_fn_v2_ = process_frame_fn;
	user_ctx_ = user_ctx;
	mode_v2_ = true;

	// Mirror v2 config into config_ for shared accessors (get_pot_raw_u8 etc.).
	config_ = AudioProcessorConfig{};
	config_.sample_period_us = derived_period_us;
	config_.enable_pot_mux = enable_pot_mux;
	config_.pot_count = pot_count;
	config_.pot_settle_discard_samples = config.pot_settle_discard_samples;
	config_.pot_average_samples =
		config.pot_average_samples == 0 ? 1 : config.pot_average_samples;

	const EngineSetup setup{
		derived_period_us,
		enable_pot_mux,
		pot_count,
		config.pot_settle_discard_samples,
		config_.pot_average_samples,
		config.claim_channel_a,
		config.claim_channel_b,
	};
	return start_engines(setup);
}

BrainInitStatus AudioProcessor::start_engines(const EngineSetup& s) {
	tick_count_ = 0;
	active_pot_count_ = s.enable_pot_mux ? s.pot_count : 0;

	// 1) Start OutputEngine (idempotent).
	brain::internal::OutputEngineConfig out_cfg;
	out_cfg.spi_instance = spi0;
	out_cfg.cs_gpio = kAudioCvOutCsPin;
	out_cfg.sck_gpio = kAudioCvOutSckPin;
	out_cfg.tx_gpio = kAudioCvOutTxPin;
	out_cfg.sample_period_us = s.sample_period_us;
	if (!brain::internal::OutputEngine::instance().start(out_cfg)) {
		fprintf(stderr, "AudioProcessor: OutputEngine::start failed\n");
		stop();
		return BrainInitStatus::kFailed;
	}

	// AudioProcessor writes signed samples around 0; drive coupling pins of
	// claimed channels high to select the bipolar -5..+5V range.
	if (s.claim_channel_a) {
		gpio_init(kAudioCvOutCouplingAPin);
		gpio_set_dir(kAudioCvOutCouplingAPin, GPIO_OUT);
		gpio_put(kAudioCvOutCouplingAPin, true);
	}
	if (s.claim_channel_b) {
		gpio_init(kAudioCvOutCouplingBPin);
		gpio_set_dir(kAudioCvOutCouplingBPin, GPIO_OUT);
		gpio_put(kAudioCvOutCouplingBPin, true);
	}

	// 2) Configure AdcEngine with optional pot scanning.
	if (s.enable_pot_mux) {
		PotsConfig pots_cfg = {};
		pots_cfg.simple = false;
		pots_cfg.adc_gpio = GPIO_BRAIN_POTMUX_ADC;
		pots_cfg.s0_gpio = GPIO_BRAIN_POTMUX_S0;
		pots_cfg.s1_gpio = GPIO_BRAIN_POTMUX_S1;
		pots_cfg.num_pots = s.pot_count;
		// channel_map is deprecated/ignored by AdcEngine; pot N maps to mux N.
		pots_cfg.output_resolution = 8;
		pots_cfg.settling_delay_us =
			static_cast<uint32_t>(s.pot_settle_discard_samples) *
			kAdcEnginePotSamplePeriodUs;
		pots_cfg.samples_per_read = s.pot_average_samples;
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
	if (!brain::internal::AdcEngine::instance().enable_audio_mode(s.sample_period_us)) {
		fprintf(stderr, "AudioProcessor: AdcEngine::enable_audio_mode failed\n");
		stop();
		return BrainInitStatus::kFailed;
	}
	audio_mode_started_ = true;

	// 4) Claim selected OutputEngine channels as audio-owned.
	if (s.claim_channel_a) {
		brain::internal::OutputEngine::instance().set_channel_owner(
			AudioCvOutChannel::kChannelA, brain::internal::ChannelOwner::kAudio);
		channel_a_claimed_ = true;
	}
	if (s.claim_channel_b) {
		brain::internal::OutputEngine::instance().set_channel_owner(
			AudioCvOutChannel::kChannelB, brain::internal::ChannelOwner::kAudio);
		channel_b_claimed_ = true;
	}

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
	// instances must not touch the shared engines owned by the running instance
	// when they go out of scope.
	if (!initialized_ && !audio_mode_started_ &&
	    !channel_a_claimed_ && !channel_b_claimed_) {
		return;
	}

	// Drop the callback first so no IRQ touches our state during teardown.
	brain::internal::AdcEngine::instance().set_audio_callback(nullptr, nullptr);

	if (channel_a_claimed_) {
		brain::internal::OutputEngine::instance().set_channel_owner(
			AudioCvOutChannel::kChannelA, brain::internal::ChannelOwner::kManual);
		channel_a_claimed_ = false;
	}
	if (channel_b_claimed_) {
		brain::internal::OutputEngine::instance().set_channel_owner(
			AudioCvOutChannel::kChannelB, brain::internal::ChannelOwner::kManual);
		channel_b_claimed_ = false;
	}

	if (audio_mode_started_) {
		brain::internal::AdcEngine::instance().disable_audio_mode();
		audio_mode_started_ = false;
	}

	process_sample_fn_ = nullptr;
	process_frame_fn_v2_ = nullptr;
	user_ctx_ = nullptr;
	mode_v2_ = false;
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

void AudioProcessor::on_adc_sample(uint16_t in1_raw, uint16_t in2_raw,
                                    const brain::internal::AdcSnapshot& snap) {
	const uint64_t tick = ++tick_count_;
	const int16_t input_a = adc_raw_to_audio_sample(in1_raw);

	if (mode_v2_) {
		AudioProcessorFrameV2 frame{};
		frame.tick = tick;
		frame.pot_count = active_pot_count_;
		for (uint8_t i = 0; i < AudioProcessorFrameV2::kMaxPots; ++i) {
			const uint32_t raw = snap.pot_raw[i] & kAdcMaxValue;
			frame.pot_raw_u8[i] = static_cast<uint8_t>(
				(raw * 255u + (kAdcMaxValue / 2)) / kAdcMaxValue);
		}
		const int16_t input_b = adc_raw_to_audio_sample(in2_raw);
		int16_t out_a = 0;
		int16_t out_b = 0;
		process_frame_fn_v2_(input_a, input_b, &frame, &out_a, &out_b, user_ctx_);
		if (channel_a_claimed_) {
			brain::internal::OutputEngine::instance().write_audio_sample(
				AudioCvOutChannel::kChannelA, audio_sample_to_dac_value(out_a));
		}
		if (channel_b_claimed_) {
			brain::internal::OutputEngine::instance().write_audio_sample(
				AudioCvOutChannel::kChannelB, audio_sample_to_dac_value(out_b));
		}
		return;
	}

	// Legacy single-channel path: IN1 -> ProcessSampleFn -> OUT A.
	AudioProcessorFrame frame{};
	frame.tick = tick;
	frame.pot_count = active_pot_count_;
	for (uint8_t i = 0; i < AudioProcessorFrame::kMaxPots; ++i) {
		const uint32_t raw = snap.pot_raw[i] & kAdcMaxValue;
		frame.pot_raw_u8[i] = static_cast<uint8_t>(
			(raw * 255u + (kAdcMaxValue / 2)) / kAdcMaxValue);
	}

	const int16_t input_sample = input_a;
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
