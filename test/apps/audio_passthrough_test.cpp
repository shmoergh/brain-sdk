#include "audio_passthrough_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "output-engine.h"

namespace sandbox::apps {

int16_t AudioPassthroughTest::process_sample(
	int16_t input_sample,
	const AudioProcessorFrame* /*frame*/,
	void* /*user_ctx*/) {
	// Simplest possible DSP: pass IN1 straight to OUT A, no filtering, no pots.
	return input_sample;
}

void AudioPassthroughTest::init() {
	stdio_init_all();

	printf("\n\r--------\n\r");
	printf("Audio passthrough test (IN1 -> OUT A direct)\n");
	printf("No pots, no DSP, no guardrails. Pure passthrough.\n");
	printf("Sample period: 23us (~43.48kHz)\n");

	AudioProcessorConfig config{};
	config.sample_period_us = 23;
	config.enable_pot_mux = false;  // skip pot scanner entirely
	config.pot_count = 0;

	BrainInitStatus status = brain_.init_audio_processor(
		config, &AudioPassthroughTest::process_sample, nullptr);
	if (!brain_init_succeeded(status)) {
		printf("[ERROR] init_audio_processor failed (status=%d)\n", static_cast<int>(status));
		initialized_ = false;
		return;
	}

	printf("Audio passthrough running. Patch IN1 -> hear it on OUT A.\n");
	initialized_ = true;
}

void AudioPassthroughTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) >= 250000) {
		last_print_us_ = now_us;

		const AudioProcessorStats stats = brain_.audio_processor.get_stats();
		const auto out_snap = brain::internal::OutputEngine::instance().get_snapshot();
		printf("\n\rTicks=%llu Underruns=%lu Overflows=%lu      ",
			static_cast<unsigned long long>(stats.tick_count),
			static_cast<unsigned long>(out_snap.audio_underrun_a),
			static_cast<unsigned long>(out_snap.audio_overflow_a));
		fflush(stdout);
	}

	sleep_ms(1);
}

}  // namespace sandbox::apps
