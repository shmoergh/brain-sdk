#include "audio_processor_example.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace {

int16_t clamp_i16(int32_t value) {
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return static_cast<int16_t>(value);
}

}  // namespace

int16_t AudioProcessorExample::process_sample(int16_t in, const AudioProcessorFrame* frame, void* user_ctx) {
	EffectState* s = static_cast<EffectState*>(user_ctx);
	if (s == nullptr) return in;

	uint8_t cutoff = 64;
	if (frame != nullptr && frame->pot_count > 1) {
		cutoff = frame->pot_raw_u8[1];
	}

	s->lp += ((static_cast<int32_t>(in) - s->lp) * (4 + (cutoff >> 1))) >> 8;
	return clamp_i16(s->lp);
}

void AudioProcessorExample::init() {
	printf("\n--------\n");
	printf("Example: AudioProcessor lowpass (A-in -> A-out)\n");

	AudioProcessorConfig cfg{};
	cfg.sample_period_us = 23;
	cfg.enable_pot_mux = true;
	cfg.pot_count = 3;
	cfg.pot_settle_discard_samples = 2;
	cfg.pot_average_samples = 4;
	cfg.max_dma_drain_samples_per_tick = 64;

	if (!brain_init_succeeded(brain_.init_audio_processor(cfg, &AudioProcessorExample::process_sample, &state_))) {
		printf("[ERROR] init_audio_processor failed\n");
		return;
	}

	initialized_ = true;
	last_print_ms_ = to_ms_since_boot(get_absolute_time());
	printf("Running. POT2 controls cutoff.\n");
}

void AudioProcessorExample::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now = to_ms_since_boot(get_absolute_time());
	if ((now - last_print_ms_) >= 250) {
		last_print_ms_ = now;
		const AudioProcessorStats stats = brain_.audio_processor.get_stats();
		printf("\rTicks=%llu Overruns=%lu Pot1=%u Pot2=%u Pot3=%u      ",
			static_cast<unsigned long long>(stats.tick_count),
			static_cast<unsigned long>(stats.overrun_count),
			static_cast<unsigned>(brain_.audio_processor.get_pot_raw_u8(0)),
			static_cast<unsigned>(brain_.audio_processor.get_pot_raw_u8(1)),
			static_cast<unsigned>(brain_.audio_processor.get_pot_raw_u8(2)));
		fflush(stdout);
	}
}
