#include "audio_volume_test.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"

namespace sandbox::apps {

int16_t AudioVolumeTest::process_sample(
	int16_t input_sample,
	const AudioProcessorFrame* /*frame*/,
	void* user_ctx) {
	// DIAGNOSTIC MODE: raw passthrough plus ISR-side discontinuity counters.
	State* state = static_cast<State*>(user_ctx);
	if (state == nullptr) return input_sample;

	const int32_t delta = static_cast<int32_t>(input_sample) - static_cast<int32_t>(state->last_input_sample);
	const uint32_t abs_delta = static_cast<uint32_t>(delta < 0 ? -delta : delta);
	if (abs_delta > state->max_abs_delta) {
		state->max_abs_delta = abs_delta;
	}
	// Threshold tuned to catch click-like discontinuities, not normal waveform slopes.
	if (abs_delta > 12000u) {
		++state->spike_count;
	}
	state->last_input_sample = input_sample;
	++state->sample_count;
	return input_sample;
}

void AudioVolumeTest::init() {
	stdio_init_all();
	sleep_ms(800);

	printf("\n\r========================================\n");
	printf(" Audio volume test\n");
	printf(" Input 1 -> Output 1, gain = Pot 1\n");
	printf("========================================\n\n");

	AudioProcessorConfig audio_cfg{};
	audio_cfg.sample_period_us = 23;
	audio_cfg.spi_baud_hz = 8000000;

	BrainInitStatus audio_status = brain_.init_audio_processor(
		audio_cfg, &AudioVolumeTest::process_sample, &state_);
	if (!brain_init_succeeded(audio_status)) {
		printf("[ERROR] init_audio_processor failed (status=%d)\n", static_cast<int>(audio_status));
		initialized_ = false;
		return;
	}

	printf("Init OK. Sample period=%lu us, SPI baud=%lu Hz.\n",
		   static_cast<unsigned long>(audio_cfg.sample_period_us),
		   static_cast<unsigned long>(audio_cfg.spi_baud_hz));
	printf("Raw passthrough with ADC discontinuity diagnostics.\n\n");

	initialized_ = true;
}

void AudioVolumeTest::update() {
	if (!initialized_) {
		sleep_ms(50);
		return;
	}

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) < 1000000) {
		sleep_ms(1);
		return;
	}
	last_print_us_ = now_us;

	const uint32_t samples = state_.sample_count;
	const uint32_t spikes = state_.spike_count;
	const uint32_t max_abs_delta = state_.max_abs_delta;
	const AdcEngine::Stats adc_stats = AdcEngine::instance().get_stats();

	printf("ADC path diag: samples=%lu spikes=%lu max_abs_delta=%lu ADC-Err=%lu\n",
		static_cast<unsigned long>(samples),
		static_cast<unsigned long>(spikes),
		static_cast<unsigned long>(max_abs_delta),
		static_cast<unsigned long>(adc_stats.conversion_error_count));
	fflush(stdout);

	sleep_ms(1);
}

}  // namespace sandbox::apps
