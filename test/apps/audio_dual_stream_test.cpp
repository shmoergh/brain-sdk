#include "audio_dual_stream_test.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"

namespace sandbox::apps {

void AudioDualStreamTest::process_dual(
	DualStreamSamples* samples,
	const AudioProcessorFrame* /*frame*/,
	void* user_ctx) {
	State* state = static_cast<State*>(user_ctx);
	if (state == nullptr || samples == nullptr) return;

	// Q0.8 gain per channel: 0 -> mute, 255 -> ~unity. No floating point.
	const int32_t gain_a = static_cast<int32_t>(state->volume_q8_a);
	const int32_t gain_b = static_cast<int32_t>(state->volume_q8_b);

	int32_t scaled_a = (static_cast<int32_t>(samples->in[kAudioStreamA]) * gain_a) >> 8;
	int32_t scaled_b = (static_cast<int32_t>(samples->in[kAudioStreamB]) * gain_b) >> 8;
	if (scaled_a > 32767) scaled_a = 32767;
	if (scaled_a < -32768) scaled_a = -32768;
	if (scaled_b > 32767) scaled_b = 32767;
	if (scaled_b < -32768) scaled_b = -32768;

	samples->out[kAudioStreamA] = static_cast<int16_t>(scaled_a);
	samples->out[kAudioStreamB] = static_cast<int16_t>(scaled_b);
}

void AudioDualStreamTest::init() {
	stdio_init_all();
	sleep_ms(800);

	printf("\n\r========================================\n");
	printf(" Audio dual-stream test\n");
	printf(" In1 -> Out1 (gain = Pot 1)\n");
	printf(" In2 -> Out2 (gain = Pot 2)\n");
	printf("========================================\n\n");

	BrainInitStatus pots_status = brain_.init_pots(create_default_pots_config(3, 8));
	if (!brain_init_succeeded(pots_status)) {
		printf("[ERROR] init_pots failed (status=%d)\n", static_cast<int>(pots_status));
		initialized_ = false;
		return;
	}

	AudioProcessorConfig audio_cfg{};
	audio_cfg.sample_period_us = 23;     // ~43.5 kHz
	audio_cfg.spi_baud_hz = 8000000;     // 8 MHz — needed in dual mode for two SPI writes/tick

	BrainInitStatus audio_status = brain_.init_audio_processor(
		audio_cfg, &AudioDualStreamTest::process_dual, &state_);
	if (!brain_init_succeeded(audio_status)) {
		printf("[ERROR] init_audio_processor (dual) failed (status=%d)\n",
			   static_cast<int>(audio_status));
		initialized_ = false;
		return;
	}

	state_.volume_q8_a = brain_.pots.get(0) & 0xFF;
	state_.volume_q8_b = brain_.pots.get(1) & 0xFF;

	printf("Init OK. Sample period=%lu us, SPI baud=%lu Hz.\n",
		   static_cast<unsigned long>(audio_cfg.sample_period_us),
		   static_cast<unsigned long>(audio_cfg.spi_baud_hz));
	printf("Turn Pot 1 to fade In1 -> Out1; Pot 2 to fade In2 -> Out2.\n");
	printf("Both pots should print 0..255; AP-Ovr / ADC-Ovr should stay at 0.\n\n");

	initialized_ = true;
}

void AudioDualStreamTest::update() {
	if (!initialized_) {
		sleep_ms(50);
		return;
	}

	state_.volume_q8_a = brain_.pots.get(0) & 0xFF;
	state_.volume_q8_b = brain_.pots.get(1) & 0xFF;

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) < 250000) {
		sleep_ms(1);
		return;
	}
	last_print_us_ = now_us;

	const AudioProcessorStats stats = brain_.audio_processor.get_stats();
	const AdcEngine::Stats adc_stats = AdcEngine::instance().get_stats();
	const uint16_t pot0 = brain_.pots.get(0);
	const uint16_t pot1 = brain_.pots.get(1);

	printf(
		"\rP1=%3u P2=%3u  Vol=[%3u,%3u]  Ticks=%llu  AP-Ovr=%lu  ADC-Ovr=%lu     ",
		static_cast<unsigned>(pot0),
		static_cast<unsigned>(pot1),
		static_cast<unsigned>(state_.volume_q8_a),
		static_cast<unsigned>(state_.volume_q8_b),
		static_cast<unsigned long long>(stats.tick_count),
		static_cast<unsigned long>(stats.overrun_count),
		static_cast<unsigned long>(adc_stats.overrun_count));
	fflush(stdout);

	sleep_ms(1);
}

}  // namespace sandbox::apps
