#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

/**
 * @brief Manual hardware regression test for pot bleed/read stability.
 *
 * The test compares direct single-read paths against buffered scan paths for
 * both Pots and PotMultiFunction while the user keeps pot 1 stable and moves
 * pot 3 aggressively.
 */
class PotReadStabilityTest {
public:
	void init();
	void update();

private:
	enum class Phase : uint8_t {
		kPreparePotsDirect,
		kMeasurePotsDirect,
		kPreparePotsBuffered,
		kMeasurePotsBuffered,
		kPrepareMultiDirect,
		kMeasureMultiDirect,
		kPrepareMultiBuffered,
		kMeasureMultiBuffered,
		kSummary,
		kDone
	};

	struct Stats {
		const char* label = "";
		uint16_t stable_min = 0;
		uint16_t stable_max = 0;
		uint16_t mover_min = 0;
		uint16_t mover_max = 0;
		uint32_t stable_delta_sum = 0;
		uint32_t mover_delta_sum = 0;
		uint32_t samples = 0;
		uint16_t last_stable = 0;
		uint16_t last_mover = 0;
		bool has_last = false;
	};

	static constexpr uint8_t kStablePotIndex = 0;  // Pot 1
	static constexpr uint8_t kMoverPotIndex = 2;   // Pot 3
	static constexpr uint8_t kStableFunctionId = 1;
	static constexpr uint8_t kMoverFunctionId = 3;

	static constexpr uint32_t kPrepareMs = 3000;
	static constexpr uint32_t kMeasureMs = 8000;
	static constexpr uint32_t kLoopSleepMs = 1;

	Brain brain_;
	bool initialized_ = false;
	bool setup_ok_ = false;
	Phase phase_ = Phase::kPreparePotsDirect;
	absolute_time_t phase_start_{};

	Stats pots_direct_{};
	Stats pots_buffered_{};
	Stats multi_direct_{};
	Stats multi_buffered_{};

	void start_phase(Phase phase);
	void update_prepare(const char* title, Phase next_phase);
	void update_measure_pots(bool buffered, Stats& stats, Phase next_phase);
	void update_measure_multi(bool buffered, Stats& stats, Phase next_phase);
	void reset_stats(Stats& stats, const char* label);
	void feed_stats(Stats& stats, uint16_t stable_value, uint16_t mover_value);
	void print_phase_result(const Stats& stats) const;
	void print_summary() const;
	void register_multi_functions();
	static uint32_t elapsed_ms(absolute_time_t start);
};

}  // namespace sandbox::apps
