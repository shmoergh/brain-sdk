#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

// Stress test for AdcEngine::pause_for_flash / resume_after_flash.
//
// Pots + Storage running together: continuously read all 3 pots (which goes
// through the shared AdcEngine), and periodically write a small blob to flash
// via Storage. Each flash write internally pauses the AdcEngine, runs the
// flash op (~10–40 ms with IRQs disabled by the bootrom), and resumes the
// engine. Without the pause/resume protection, the engine would wedge after a
// few flash writes; with it, pot reads keep updating cleanly forever.
//
// The test prints a periodic line showing pot values, flash write count,
// and a "frozen?" indicator (compares pot values against what they were before
// the most recent flash write — if all three are identical, that's a hint
// something might be wrong, but turning a pot during the test refutes a
// false positive).
class PotsStorageStressTest {
public:
	void init();
	void update();

private:
	bool do_flash_write(uint32_t counter);

	Brain brain_{};
	bool initialized_ = false;
	uint32_t last_print_us_ = 0;
	uint32_t last_flash_us_ = 0;
	uint32_t flash_interval_us_ = 3'000'000;  // one flash write every 3 s
	uint32_t flash_write_count_ = 0;
	uint32_t flash_failure_count_ = 0;
	uint16_t pots_before_flash_[3] = {0, 0, 0};
	uint16_t pots_after_flash_[3] = {0, 0, 0};
};

}  // namespace sandbox::apps
