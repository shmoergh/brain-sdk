#include "output_engine_timing_test.h"

#include <stdio.h>

#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include "gpio-setup.h"
#include "output-engine.h"

namespace sandbox::apps {

namespace {

constexpr uint32_t kRampStepUs = 1000;       // 1 kHz update from main loop
constexpr uint16_t kRampStep = 32;           // ~128 ms ramp top-to-bottom
constexpr uint32_t kPrintIntervalUs = 1'000'000;

void init_coupling_pins_for_unipolar() {
	gpio_init(GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A);
	gpio_set_dir(GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A, GPIO_OUT);
	gpio_put(GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A, 0);
	gpio_init(GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B);
	gpio_set_dir(GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B, GPIO_OUT);
	gpio_put(GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B, 0);
}

}  // namespace

void OutputEngineTimingTest::init() {
	stdio_init_all();
	sleep_ms(200);

	printf("\n\r--------\n\r");
	printf("OutputEngine timing test (Slice 1)\n");
	printf("Drives a slow ramp on A (rising) and B (falling). Scope both DAC\n");
	printf("outputs and CS — expect clean, jitter-free 16-bit frames with CS\n");
	printf("toggling between every frame. Frame rate printed once per second.\n\n");

	init_coupling_pins_for_unipolar();

	brain::internal::OutputEngineConfig cfg;
	cfg.spi_instance = spi0;
	cfg.cs_gpio = GPIO_BRAIN_AUDIO_CV_OUT_CS;
	cfg.sck_gpio = GPIO_BRAIN_AUDIO_CV_OUT_SCK;
	cfg.tx_gpio = GPIO_BRAIN_AUDIO_CV_OUT_TX;
	cfg.spi_baud_hz = 20'000'000;
	cfg.sample_period_us = 23;

	initialized_ = brain::internal::OutputEngine::instance().start(cfg);
	if (!initialized_) {
		printf("[ERROR] OutputEngine::start failed\n");
		return;
	}

	// Seed both channels with mid-scale so the operator sees something even
	// before the ramp updater kicks in.
	brain::internal::OutputEngine::instance().set_hold_value(
		AudioCvOutChannel::kChannelA, 0x800);
	brain::internal::OutputEngine::instance().set_hold_value(
		AudioCvOutChannel::kChannelB, 0x800);

	printf("[OK] Engine started. Streaming at sample_period_us=%lu (frame rate ~= %lu Hz)\n",
		static_cast<unsigned long>(cfg.sample_period_us),
		static_cast<unsigned long>(2'000'000ull / cfg.sample_period_us));
}

void OutputEngineTimingTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now_us = to_us_since_boot(get_absolute_time());

	// Step the ramp.
	if ((now_us - last_step_us_) >= kRampStepUs) {
		last_step_us_ = now_us;
		if (ramp_up_) {
			if (ramp_value_ + kRampStep >= 0x0FFF) {
				ramp_value_ = 0x0FFF;
				ramp_up_ = false;
			} else {
				ramp_value_ = static_cast<uint16_t>(ramp_value_ + kRampStep);
			}
		} else {
			if (ramp_value_ < kRampStep) {
				ramp_value_ = 0;
				ramp_up_ = true;
			} else {
				ramp_value_ = static_cast<uint16_t>(ramp_value_ - kRampStep);
			}
		}

		const uint16_t inv = static_cast<uint16_t>(0x0FFF - ramp_value_);
		brain::internal::OutputEngine::instance().set_hold_value(
			AudioCvOutChannel::kChannelA, ramp_value_);
		brain::internal::OutputEngine::instance().set_hold_value(
			AudioCvOutChannel::kChannelB, inv);
	}

	// Print stats once per second.
	if ((now_us - last_print_us_) < kPrintIntervalUs) {
		return;
	}
	const uint32_t elapsed_us = now_us - last_print_us_;
	last_print_us_ = now_us;

	const auto snapshot = brain::internal::OutputEngine::instance().get_snapshot();
	const uint64_t delta_frames = snapshot.total_frames - last_total_frames_;
	last_total_frames_ = snapshot.total_frames;

	const uint64_t measured_frame_rate_hz =
		(delta_frames * 1'000'000ull) / (elapsed_us == 0 ? 1 : elapsed_us);

	printf("\rblocks=%llu frames=%llu rate=%lluHz A=0x%03X B=0x%03X underruns=A%lu/B%lu    ",
		static_cast<unsigned long long>(snapshot.total_blocks),
		static_cast<unsigned long long>(snapshot.total_frames),
		static_cast<unsigned long long>(measured_frame_rate_hz),
		static_cast<unsigned>(snapshot.hold_frame_a & 0x0FFF),
		static_cast<unsigned>(snapshot.hold_frame_b & 0x0FFF),
		static_cast<unsigned long>(snapshot.audio_underrun_a),
		static_cast<unsigned long>(snapshot.audio_underrun_b));
	fflush(stdout);
}

}  // namespace sandbox::apps
