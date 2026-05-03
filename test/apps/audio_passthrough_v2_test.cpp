#include "audio_passthrough_v2_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "output-engine.h"

namespace sandbox::apps {

void AudioPassthroughV2Test::process_frame(
	int16_t in1,
	int16_t in2,
	const AudioProcessorFrameV2* /*frame*/,
	int16_t* out_a,
	int16_t* out_b,
	void* /*user_ctx*/) {
	// Stereo passthrough: IN1 → OUT A, IN2 → OUT B. No DSP.
	*out_a = in1;
	*out_b = in2;
}

void AudioPassthroughV2Test::init() {
	stdio_init_all();

	printf("\n\r--------\n\r");
	printf("Audio passthrough V2 (stereo)\n");
	printf("IN1 -> OUT A, IN2 -> OUT B. No pots, no DSP.\n");
	printf("Sample rate: 43500 Hz\n");

	AudioProcessorConfigV2 config{};
	config.sample_rate_hz = 43500;
	config.enable_pot_mux = false;
	config.pot_count = 0;
	config.claim_channel_a = true;
	config.claim_channel_b = true;

	BrainInitStatus status = brain_.init_audio_processor_v2(
		config, &AudioPassthroughV2Test::process_frame, nullptr);
	if (!brain_init_succeeded(status)) {
		printf("[ERROR] init_audio_processor_v2 failed (status=%d)\n",
			static_cast<int>(status));
		initialized_ = false;
		return;
	}

	printf("Stereo passthrough running.\n");
	initialized_ = true;
}

void AudioPassthroughV2Test::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) >= 250000) {
		last_print_us_ = now_us;

		const AudioProcessorStats stats = brain_.audio_processor.get_stats();
		const auto out_snap = brain::internal::OutputEngine::instance().get_snapshot();
		printf("\n\rTicks=%llu UnderrunsA=%lu OverflowsA=%lu UnderrunsB=%lu OverflowsB=%lu      ",
			static_cast<unsigned long long>(stats.tick_count),
			static_cast<unsigned long>(out_snap.audio_underrun_a),
			static_cast<unsigned long>(out_snap.audio_overflow_a),
			static_cast<unsigned long>(out_snap.audio_underrun_b),
			static_cast<unsigned long>(out_snap.audio_overflow_b));
		fflush(stdout);
	}

	sleep_ms(1);
}

}  // namespace sandbox::apps
