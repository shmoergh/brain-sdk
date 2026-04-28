#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

/**
 * @brief Minimal live pot monitor for hands-on verification.
 *
 * Initializes only `Pots` (no audio, no inputs) and prints continuously
 * updating values for each pot: raw 12-bit ADC, mapped 8-bit value, and
 * an ASCII bar graph. Tracks min/max observed per pot since startup so
 * a single sweep of each knob proves the full range works.
 *
 * Use this to confirm the SDK-side pot path works end-to-end on your
 * hardware, independent of any other component.
 */
class PotLiveMonitorTest {
public:
	void init();
	void update();

private:
	static constexpr uint8_t kNumPots = 3;
	static constexpr uint32_t kPrintIntervalUs = 100000;  // 10 Hz refresh

	Brain brain_{};
	bool initialized_ = false;
	bool first_print_ = true;

	uint16_t raw_min_[kNumPots] = {};
	uint16_t raw_max_[kNumPots] = {};
	uint8_t mapped_min_[kNumPots] = {};
	uint8_t mapped_max_[kNumPots] = {};

	uint32_t last_print_us_ = 0;
	uint32_t update_count_ = 0;
};

}  // namespace sandbox::apps
