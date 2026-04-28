#include "audio_volume_test.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"

namespace sandbox::apps {

int16_t AudioVolumeTest::process_sample(
	int16_t input_sample,
	const AudioProcessorFrame* /*frame*/,
	void* user_ctx) {
	State* state = static_cast<State*>(user_ctx);
	if (state == nullptr) return input_sample;

	// Pot 1 (index 0) is mapped 0..255; treat as Q0.8 gain (0 -> mute, 255 -> ~unity).
	const int32_t gain_q8 = static_cast<int32_t>(state->volume_q8);
	const int32_t scaled = (static_cast<int32_t>(input_sample) * gain_q8) >> 8;
	if (scaled > 32767) return 32767;
	if (scaled < -32768) return -32768;
	return static_cast<int16_t>(scaled);
}

void AudioVolumeTest::init() {
	stdio_init_all();
	sleep_ms(800);

	printf("\n\r========================================\n");
	printf(" Audio volume test\n");
	printf(" Input 1 -> Output 1, gain = Pot 1\n");
	printf("========================================\n\n");

	// Pots first so AudioProcessor sees them via Brain auto-wiring.
	BrainInitStatus pots_status = brain_.init_pots(create_default_pots_config(3, 8));
	if (!brain_init_succeeded(pots_status)) {
		printf("[ERROR] init_pots failed (status=%d)\n", static_cast<int>(pots_status));
		initialized_ = false;
		return;
	}

	AudioProcessorConfig audio_cfg{};
	audio_cfg.sample_period_us = 23;  // ~43.5 kHz
	audio_cfg.spi_baud_hz = 4000000;  // 4 MHz keeps DAC SPI well under tick budget

	BrainInitStatus audio_status = brain_.init_audio_processor(
		audio_cfg, &AudioVolumeTest::process_sample, &state_);
	if (!brain_init_succeeded(audio_status)) {
		printf("[ERROR] init_audio_processor failed (status=%d)\n", static_cast<int>(audio_status));
		initialized_ = false;
		return;
	}

	state_.volume_q8 = brain_.pots.get(0) & 0xFF;

	printf("Init OK. Sample period=%lu us, SPI baud=%lu Hz.\n",
		   static_cast<unsigned long>(audio_cfg.sample_period_us),
		   static_cast<unsigned long>(audio_cfg.spi_baud_hz));
	printf("Turn Pot 1 to fade Input 1 -> Output 1.\n");
	printf("Pot 1 should print 0..255; Overruns should stay at 0.\n\n");

	initialized_ = true;
}

void AudioVolumeTest::update() {
	if (!initialized_) {
		sleep_ms(50);
		return;
	}

	// Pull the latest pot value into the State the audio ISR reads.
	state_.volume_q8 = brain_.pots.get(0) & 0xFF;

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) < 250000) {
		sleep_ms(1);
		return;
	}
	last_print_us_ = now_us;

	const AudioProcessorStats stats = brain_.audio_processor.get_stats();
	const AdcEngine::Stats adc_stats = AdcEngine::instance().get_stats();
	const uint16_t pot0 = brain_.pots.get(0);

	printf(
		"\rPot1=%3u  Vol=%3u/255  Ticks=%llu  AP-Ovr=%lu  ADC-Ovr=%lu  Drains=%llu      ",
		static_cast<unsigned>(pot0),
		static_cast<unsigned>(state_.volume_q8),
		static_cast<unsigned long long>(stats.tick_count),
		static_cast<unsigned long>(stats.overrun_count),
		static_cast<unsigned long>(adc_stats.overrun_count),
		static_cast<unsigned long long>(adc_stats.drain_count));
	fflush(stdout);

	sleep_ms(1);
}

}  // namespace sandbox::apps
