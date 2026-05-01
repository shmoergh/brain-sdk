#include "pot_cross_bleed_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace sandbox::apps {

namespace {

uint16_t min_u16(uint16_t a, uint16_t b) { return a < b ? a : b; }
uint16_t max_u16(uint16_t a, uint16_t b) { return a > b ? a : b; }

uint32_t elapsed_ms(absolute_time_t start) {
	return static_cast<uint32_t>(
		to_ms_since_boot(get_absolute_time()) - to_ms_since_boot(start));
}

}  // namespace

void PotCrossBleedTest::init() {
	stdio_init_all();
	printf("\033[2J\033[H");
	fflush(stdout);

	printf("\n----------------------------------------\n");
	printf(" Pot cross-bleed regression test\n");
	printf("----------------------------------------\n\n");

	BrainInitStatus status = brain_.init_pots(create_default_pots_config(3, 8));
	if (!brain_init_succeeded(status)) {
		printf("[ERROR] Failed to initialize pots.\n");
		setup_ok_ = false;
		initialized_ = true;
		phase_ = Phase::kDone;
		return;
	}

	setup_ok_ = true;
	initialized_ = true;
	start_phase(Phase::kPrepare);
}

void PotCrossBleedTest::update() {
	if (!initialized_ || !setup_ok_ || phase_ == Phase::kDone) {
		sleep_ms(5);
		return;
	}

	switch (phase_) {
		case Phase::kPrepare: {
			if (elapsed_ms(phase_start_) >= kPrepareMs) {
				start_phase(Phase::kMeasure);
			} else {
				sleep_ms(50);
			}
			break;
		}

		case Phase::kMeasure: {
			const uint16_t stable_raw = brain_.pots.get_raw(kStablePotIndex);
			const uint16_t mover_raw = brain_.pots.get_raw(kMoverPotIndex);

			stable_min_ = min_u16(stable_min_, stable_raw);
			stable_max_ = max_u16(stable_max_, stable_raw);
			mover_min_ = min_u16(mover_min_, mover_raw);
			mover_max_ = max_u16(mover_max_, mover_raw);
			++samples_;

			if (elapsed_ms(phase_start_) >= kMeasureMs) {
				start_phase(Phase::kSummary);
			} else {
				sleep_ms(2);
			}
			break;
		}

		case Phase::kSummary: {
			const uint16_t stable_range = static_cast<uint16_t>(stable_max_ - stable_min_);
			const uint16_t mover_range = static_cast<uint16_t>(mover_max_ - mover_min_);
			const bool pass = stable_range <= kPassRawRangeLsb;

			printf("\n========== Cross-bleed result ==========\n");
			printf("Samples taken:           %lu\n",
				   static_cast<unsigned long>(samples_));
			printf("Stable pot (pot %u) raw:  min=%u max=%u  range=%u (12-bit LSB)\n",
				   static_cast<unsigned>(kStablePotIndex),
				   static_cast<unsigned>(stable_min_),
				   static_cast<unsigned>(stable_max_),
				   static_cast<unsigned>(stable_range));
			printf("Mover  pot (pot %u) raw:  min=%u max=%u  range=%u (12-bit LSB)\n",
				   static_cast<unsigned>(kMoverPotIndex),
				   static_cast<unsigned>(mover_min_),
				   static_cast<unsigned>(mover_max_),
				   static_cast<unsigned>(mover_range));
			printf("Pass threshold (stable range): %u LSB\n",
				   static_cast<unsigned>(kPassRawRangeLsb));
			printf("Verdict: %s\n", pass ? "PASS — no cross-bleed detected" :
									 "FAIL — pot 0 is contaminated by pot 2");
			printf("========================================\n");
			fflush(stdout);
			phase_ = Phase::kDone;
			break;
		}

		case Phase::kDone:
			sleep_ms(50);
			break;
	}
}

void PotCrossBleedTest::start_phase(Phase phase) {
	phase_ = phase;
	phase_start_ = get_absolute_time();

	switch (phase_) {
		case Phase::kPrepare:
			printf("Setup window: %lu ms\n", static_cast<unsigned long>(kPrepareMs));
			printf("  - Set POT %u (stable)  to MIN (full CCW)\n",
				   static_cast<unsigned>(kStablePotIndex));
			printf("  - Set POT %u (mover)   to MAX (full CW)\n",
				   static_cast<unsigned>(kMoverPotIndex));
			printf("Hold both knobs still during the measurement phase.\n\n");
			fflush(stdout);
			break;

		case Phase::kMeasure:
			stable_min_ = 0xFFFF;
			stable_max_ = 0;
			mover_min_ = 0xFFFF;
			mover_max_ = 0;
			samples_ = 0;
			printf("[MEASURE] sampling pot %u for %lu ms...\n\n",
				   static_cast<unsigned>(kStablePotIndex),
				   static_cast<unsigned long>(kMeasureMs));
			fflush(stdout);
			break;

		case Phase::kSummary:
		case Phase::kDone:
			break;
	}
}

}  // namespace sandbox::apps
