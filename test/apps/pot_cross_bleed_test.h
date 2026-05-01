#pragma once

#include <cstdint>

#include "brain.h"
#include "pico/time.h"

namespace sandbox::apps {

/**
 * @brief Cross-bleed regression test for `Pots`.
 *
 * The pre-2.1 `Pots` flipped its mux GPIO synchronously when an averaged
 * read completed, but the AdcEngine drains samples in batches — so multiple
 * pre-flip samples (still on the OLD mux state) would be processed AFTER
 * the flip and incorrectly attributed to the NEW pot. With the default
 * `samples_per_read = 6`, a single stale sample bleeding through produces
 * a `prev_pot_value / 6` contamination term in the new pot's reading.
 *
 * This test exercises that scenario: the user sets pot 0 (the "stable"
 * pot) to a known extreme (min) and pot 2 (the "mover") to the opposite
 * extreme (max), and the test rapidly samples pot 0 for several seconds.
 * If cross-bleed is occurring, pot 0's raw value will visibly bounce
 * between its true value and the contamination term. The test reports
 * the observed min/max range and PASSES only if the range is well below
 * any plausible bleed contribution.
 */
class PotCrossBleedTest {
public:
	void init();
	void update();

private:
	enum class Phase {
		kPrepare,
		kMeasure,
		kSummary,
		kDone,
	};

	void start_phase(Phase phase);

	static constexpr uint32_t kPrepareMs = 4000;
	static constexpr uint32_t kMeasureMs = 4000;
	static constexpr uint8_t kStablePotIndex = 0;
	static constexpr uint8_t kMoverPotIndex = 2;
	// Raw 12-bit ADC threshold below which pot 0 is considered stable
	// against pot 2's swing. Real analog jitter is a handful of LSB at
	// rest; the bleed term in the bug was ~`pot2_max / samples_per_read`
	// — well above 100. Threshold of 100 LSB cleanly separates the two.
	static constexpr uint16_t kPassRawRangeLsb = 100;

	Brain brain_{};
	bool initialized_ = false;
	bool setup_ok_ = false;
	Phase phase_ = Phase::kDone;
	absolute_time_t phase_start_{};

	uint16_t stable_min_ = 0xFFFF;
	uint16_t stable_max_ = 0;
	uint16_t mover_min_ = 0xFFFF;
	uint16_t mover_max_ = 0;
	uint32_t samples_ = 0;
};

}  // namespace sandbox::apps
