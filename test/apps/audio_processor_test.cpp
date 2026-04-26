#include "audio_processor_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace {

int16_t clamp_i16(int32_t value) {
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return static_cast<int16_t>(value);
}

void print_check_result(const char* label, bool pass) {
	printf("[%-4s] %s\n", pass ? "PASS" : "FAIL", label);
}

}  // namespace

namespace sandbox::apps {

int16_t AudioProcessorTest::process_sample(
	int16_t input_sample,
	const AudioProcessorFrame* frame,
	void* user_ctx) {
	EffectState* state = static_cast<EffectState*>(user_ctx);
	if (state == nullptr) {
		return input_sample;
	}

	uint8_t drive = 128;
	uint8_t cutoff = 64;
	uint8_t mix = 128;
	if (frame != nullptr) {
		if (frame->pot_count > 0) drive = frame->pot_raw_u8[0];
		if (frame->pot_count > 1) cutoff = frame->pot_raw_u8[1];
		if (frame->pot_count > 2) mix = frame->pot_raw_u8[2];
	}

	const int32_t driven =
		(static_cast<int32_t>(input_sample) * static_cast<int32_t>(128 + drive)) >> 7;
	const int32_t filter_coeff = 4 + (cutoff >> 1);	// 4..131
	state->lowpass_state += ((driven - state->lowpass_state) * filter_coeff) >> 8;

	const int32_t wet = state->lowpass_state;
	const int32_t out =
		((wet * static_cast<int32_t>(mix)) +
			(driven * static_cast<int32_t>(255 - mix))) >>
		8;
	return clamp_i16(out);
}

void AudioProcessorTest::init() {
	stdio_init_all();

	printf("\n\r--------\n\r");
	printf("AudioProcessor Test (A-in -> A-out)\n");
	printf("Effect: driven lowpass blend\n");
	printf("  POT1: drive\n");
	printf("  POT2: filter cutoff\n");
	printf("  POT3: wet/dry mix\n");
	printf("Sample period: 23us (~43.48kHz)\n");

	AudioProcessorConfig config{};
	config.sample_period_us = 23;
	config.enable_pot_mux = true;
	config.pot_count = 3;
	config.pot_settle_discard_samples = 2;
	config.pot_average_samples = 4;
	config.max_dma_drain_samples_per_tick = 64;

	BrainInitStatus status = brain_.init_audio_processor(config, &AudioProcessorTest::process_sample, &effect_state_);
	if (!brain_init_succeeded(status)) {
		printf("[ERROR] Failed to initialize AudioProcessor\n");
		initialized_ = false;
		return;
	}

	printf("AudioProcessor running.\n");
	run_guardrail_checks(config);
	initialized_ = true;
}

void AudioProcessorTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) >= 250000) {
		last_print_us_ = now_us;

		const AudioProcessorStats stats = brain_.audio_processor.get_stats();
		const uint16_t pot0 = brain_.audio_processor.get_pot_raw_u8(0);
		const uint16_t pot1 = brain_.audio_processor.get_pot_raw_u8(1);
		const uint16_t pot2 = brain_.audio_processor.get_pot_raw_u8(2);

		printf(
			"\rTicks=%llu Overruns=%lu Pot=[%3u %3u %3u]                              ",
			static_cast<unsigned long long>(stats.tick_count),
			static_cast<unsigned long>(stats.overrun_count),
			static_cast<unsigned>(pot0),
			static_cast<unsigned>(pot1),
			static_cast<unsigned>(pot2));
		fflush(stdout);
	}

	sleep_ms(1);
}

bool AudioProcessorTest::run_guardrail_checks(const AudioProcessorConfig& config) {
	printf("\n\nConcurrent-init checks (must all PASS — AdcEngine allows mixing):\n");

	const bool inputs_after_audio_ok =
		brain_init_succeeded(brain_.init_inputs());
	const bool pots_after_audio_ok =
		brain_init_succeeded(brain_.init_pots());
	const bool pot_multi_after_audio_ok =
		brain_init_succeeded(brain_.init_pot_multi());

	print_check_result("audio->inputs ok", inputs_after_audio_ok);
	print_check_result("audio->pots ok", pots_after_audio_ok);
	print_check_result("audio->pot_multi ok", pot_multi_after_audio_ok);

	EffectState scratch_state_1{};
	Brain pots_first{};
	const bool pots_first_ok = brain_init_succeeded(pots_first.init_pots());
	const bool audio_after_pots_ok = brain_init_succeeded(pots_first.init_audio_processor(
		config, &AudioProcessorTest::process_sample, &scratch_state_1));
	print_check_result("pots->audio ok", pots_first_ok && audio_after_pots_ok);

	EffectState scratch_state_2{};
	Brain inputs_first{};
	const bool inputs_first_ok = brain_init_succeeded(inputs_first.init_inputs());
	const bool audio_after_inputs_ok = brain_init_succeeded(inputs_first.init_audio_processor(
		config, &AudioProcessorTest::process_sample, &scratch_state_2));
	print_check_result("inputs->audio ok", inputs_first_ok && audio_after_inputs_ok);

	EffectState scratch_state_3{};
	Brain pot_multi_first{};
	const bool pot_multi_first_ok = brain_init_succeeded(pot_multi_first.init_pot_multi());
	const bool audio_after_pot_multi_ok = brain_init_succeeded(pot_multi_first.init_audio_processor(
		config, &AudioProcessorTest::process_sample, &scratch_state_3));
	print_check_result("pot_multi->audio ok", pot_multi_first_ok && audio_after_pot_multi_ok);

	return inputs_after_audio_ok && pots_after_audio_ok && pot_multi_after_audio_ok &&
		pots_first_ok && audio_after_pots_ok &&
		inputs_first_ok && audio_after_inputs_ok &&
		pot_multi_first_ok && audio_after_pot_multi_ok;
}

}  // namespace sandbox::apps
