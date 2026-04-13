#include "pot_read_stability_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace sandbox::apps {

namespace {

uint16_t min_u16(uint16_t a, uint16_t b) {
	return a < b ? a : b;
}

uint16_t max_u16(uint16_t a, uint16_t b) {
	return a > b ? a : b;
}

uint32_t abs_delta_u16(uint16_t a, uint16_t b) {
	return (a >= b) ? static_cast<uint32_t>(a - b) : static_cast<uint32_t>(b - a);
}

}  // namespace

void PotReadStabilityTest::init() {
	stdio_init_all();
	printf("\033[2J\033[H");
	fflush(stdout);

	printf("\n--------\n");
	printf("Pot Read Stability Regression Test\n");
	printf("Goal: detect bleed on pot 1 while moving pot 3 via muxed ADC path.\n");
	printf("For every measurement phase: keep POT 1 still and move POT 3 quickly.\n\n");

	BrainInitStatus status = brain_.init_pots(create_default_pots_config(3, 8));
	if (!brain_init_succeeded(status)) {
		printf("[ERROR] Failed to initialize pots.\n");
		initialized_ = true;
		setup_ok_ = false;
		phase_ = Phase::kDone;
		return;
	}

	status = brain_.init_pot_multi();
	if (!brain_init_succeeded(status)) {
		printf("[ERROR] Failed to initialize pot multi utility.\n");
		initialized_ = true;
		setup_ok_ = false;
		phase_ = Phase::kDone;
		return;
	}

	register_multi_functions();

	reset_stats(pots_direct_, "Pots direct single-read (simple/aggressive)");
	reset_stats(pots_buffered_, "Pots buffered scan path");
	reset_stats(multi_direct_, "PotMultiFunction direct update_single()");
	reset_stats(multi_buffered_, "PotMultiFunction buffered update_buffered()");

	setup_ok_ = true;
	initialized_ = true;
	start_phase(Phase::kPreparePotsDirect);
}

void PotReadStabilityTest::update() {
	if (!initialized_ || !setup_ok_ || phase_ == Phase::kDone) {
		sleep_ms(kLoopSleepMs);
		return;
	}

	switch (phase_) {
		case Phase::kPreparePotsDirect:
			update_prepare("Phase 1/4: Pots direct single-read", Phase::kMeasurePotsDirect);
			break;
		case Phase::kMeasurePotsDirect:
			update_measure_pots(false, pots_direct_, Phase::kPreparePotsBuffered);
			break;
		case Phase::kPreparePotsBuffered:
			update_prepare("Phase 2/4: Pots buffered", Phase::kMeasurePotsBuffered);
			break;
		case Phase::kMeasurePotsBuffered:
			update_measure_pots(true, pots_buffered_, Phase::kPrepareMultiDirect);
			break;
		case Phase::kPrepareMultiDirect:
			update_prepare("Phase 3/4: PotMultiFunction direct update_single()", Phase::kMeasureMultiDirect);
			break;
		case Phase::kMeasureMultiDirect:
			update_measure_multi(false, multi_direct_, Phase::kPrepareMultiBuffered);
			break;
		case Phase::kPrepareMultiBuffered:
			update_prepare("Phase 4/4: PotMultiFunction buffered update_buffered()", Phase::kMeasureMultiBuffered);
			break;
		case Phase::kMeasureMultiBuffered:
			update_measure_multi(true, multi_buffered_, Phase::kSummary);
			break;
		case Phase::kSummary:
			print_summary();
			phase_ = Phase::kDone;
			break;
		case Phase::kDone:
			break;
	}

	sleep_ms(kLoopSleepMs);
}

void PotReadStabilityTest::start_phase(Phase phase) {
	phase_ = phase;
	phase_start_ = get_absolute_time();

	switch (phase_) {
		case Phase::kPreparePotsDirect:
		case Phase::kPreparePotsBuffered:
		case Phase::kPrepareMultiDirect:
		case Phase::kPrepareMultiBuffered:
			printf("\nPrepare window: %lu ms\n", static_cast<unsigned long>(kPrepareMs));
			printf("Keep POT 1 stable. Move POT 3 aggressively when measurement starts.\n");
			break;
		case Phase::kMeasurePotsDirect:
			reset_stats(pots_direct_, pots_direct_.label);
			brain_.pots.set_simple(true);
			printf("\n[MEASURE] %s\n", pots_direct_.label);
			break;
		case Phase::kMeasurePotsBuffered:
			reset_stats(pots_buffered_, pots_buffered_.label);
			brain_.pots.set_simple(false);
			printf("\n[MEASURE] %s\n", pots_buffered_.label);
			break;
		case Phase::kMeasureMultiDirect:
			reset_stats(multi_direct_, multi_direct_.label);
			brain_.pots.set_simple(true);
			printf("\n[MEASURE] %s\n", multi_direct_.label);
			break;
		case Phase::kMeasureMultiBuffered:
			reset_stats(multi_buffered_, multi_buffered_.label);
			brain_.pots.set_simple(false);
			printf("\n[MEASURE] %s\n", multi_buffered_.label);
			break;
		case Phase::kSummary:
		case Phase::kDone:
			break;
	}
}

void PotReadStabilityTest::update_prepare(const char* title, Phase next_phase) {
	if (elapsed_ms(phase_start_) == 0) {
		printf("\n%s\n", title);
	}

	if (elapsed_ms(phase_start_) >= kPrepareMs) {
		start_phase(next_phase);
	}
}

void PotReadStabilityTest::update_measure_pots(bool buffered, Stats& stats, Phase next_phase) {
	uint16_t stable_value = 0;
	uint16_t mover_value = 0;

	if (buffered) {
		brain_.pots.scan();
		stable_value = brain_.pots.get_buffered(kStablePotIndex);
		mover_value = brain_.pots.get_buffered(kMoverPotIndex);
	} else {
		stable_value = brain_.pots.get_single(kStablePotIndex);
		mover_value = brain_.pots.get_single(kMoverPotIndex);
	}

	feed_stats(stats, stable_value, mover_value);

	if (elapsed_ms(phase_start_) >= kMeasureMs) {
		print_phase_result(stats);
		start_phase(next_phase);
	}
}

void PotReadStabilityTest::update_measure_multi(bool buffered, Stats& stats, Phase next_phase) {
	if (buffered) {
		brain_.pot_multi.update_buffered(brain_.pots, true);
	} else {
		brain_.pot_multi.update_single(brain_.pots);
	}

	uint16_t stable_value = static_cast<uint16_t>(brain_.pot_multi.get_value(kStableFunctionId));
	uint16_t mover_value = static_cast<uint16_t>(brain_.pot_multi.get_value(kMoverFunctionId));
	feed_stats(stats, stable_value, mover_value);

	if (elapsed_ms(phase_start_) >= kMeasureMs) {
		print_phase_result(stats);
		start_phase(next_phase);
	}
}

void PotReadStabilityTest::reset_stats(Stats& stats, const char* label) {
	stats.label = label;
	stats.stable_min = 0xFFFF;
	stats.stable_max = 0;
	stats.mover_min = 0xFFFF;
	stats.mover_max = 0;
	stats.stable_delta_sum = 0;
	stats.mover_delta_sum = 0;
	stats.samples = 0;
	stats.last_stable = 0;
	stats.last_mover = 0;
	stats.has_last = false;
}

void PotReadStabilityTest::feed_stats(Stats& stats, uint16_t stable_value, uint16_t mover_value) {
	stats.stable_min = min_u16(stats.stable_min, stable_value);
	stats.stable_max = max_u16(stats.stable_max, stable_value);
	stats.mover_min = min_u16(stats.mover_min, mover_value);
	stats.mover_max = max_u16(stats.mover_max, mover_value);

	if (stats.has_last) {
		stats.stable_delta_sum += abs_delta_u16(stable_value, stats.last_stable);
		stats.mover_delta_sum += abs_delta_u16(mover_value, stats.last_mover);
	}

	stats.last_stable = stable_value;
	stats.last_mover = mover_value;
	stats.has_last = true;
	stats.samples++;
}

void PotReadStabilityTest::print_phase_result(const Stats& stats) const {
	const uint16_t stable_span = static_cast<uint16_t>(stats.stable_max - stats.stable_min);
	const uint16_t mover_span = static_cast<uint16_t>(stats.mover_max - stats.mover_min);
	const uint32_t coupling_pct_x10 = (mover_span == 0)
		? 0
		: (static_cast<uint32_t>(stable_span) * 1000u) / static_cast<uint32_t>(mover_span);

	printf("\nResult: %s\n", stats.label);
	printf("  Samples: %lu\n", static_cast<unsigned long>(stats.samples));
	printf("  Stable pot span (pot1): %u\n", static_cast<unsigned>(stable_span));
	printf("  Mover pot span  (pot3): %u\n", static_cast<unsigned>(mover_span));
	printf("  Coupling ratio (pot1_span/pot3_span): %lu.%lu%%\n",
		static_cast<unsigned long>(coupling_pct_x10 / 10u),
		static_cast<unsigned long>(coupling_pct_x10 % 10u));
}

void PotReadStabilityTest::print_summary() const {
	printf("\n======== Pot Stability Summary ========\n");
	print_phase_result(pots_direct_);
	print_phase_result(pots_buffered_);
	print_phase_result(multi_direct_);
	print_phase_result(multi_buffered_);
	printf("\nExpected: buffered paths should show lower stable-pot drift than direct paths.\n");
	printf("=======================================\n");
}

void PotReadStabilityTest::register_multi_functions() {
	PotFunctionConfig stable_cfg{};
	stable_cfg.function_id = kStableFunctionId;
	stable_cfg.pot_index = kStablePotIndex;
	stable_cfg.min_value = 0;
	stable_cfg.max_value = 255;
	stable_cfg.initial_value = 0;
	stable_cfg.mode = PotMode::kDirect;
	stable_cfg.pickup_hysteresis = 1;

	PotFunctionConfig mover_cfg{};
	mover_cfg.function_id = kMoverFunctionId;
	mover_cfg.pot_index = kMoverPotIndex;
	mover_cfg.min_value = 0;
	mover_cfg.max_value = 255;
	mover_cfg.initial_value = 0;
	mover_cfg.mode = PotMode::kDirect;
	mover_cfg.pickup_hysteresis = 1;

	const bool ok_a = brain_.pot_multi.register_function(stable_cfg);
	const bool ok_b = brain_.pot_multi.register_function(mover_cfg);

	if (!ok_a || !ok_b) {
		printf("[WARN] Failed to register one or more PotMultiFunction entries.\n");
	}

	uint8_t active[kMaxPots] = {255, 255, 255, 255};
	active[kStablePotIndex] = kStableFunctionId;
	active[kMoverPotIndex] = kMoverFunctionId;
	brain_.pot_multi.set_active_functions(active, kMaxPots);
}

uint32_t PotReadStabilityTest::elapsed_ms(absolute_time_t start) {
	return static_cast<uint32_t>(to_ms_since_boot(get_absolute_time()) - to_ms_since_boot(start));
}

}  // namespace sandbox::apps
