#pragma once

#include <cstdint>

namespace sandbox::apps {

/**
 * @brief Manual test app for verifying OutputEngine streams interleaved A/B
 * frames at deterministic cadence with hardware-toggled CS.
 *
 * Initializes OutputEngine directly (not through Outputs — that lands in
 * Slice 2). Drives a slow alternating ramp on both channels from the main loop
 * via `set_hold_value` and prints snapshot frame counters / measured frame rate
 * once per second. The operator scopes both DAC outputs and verifies clean,
 * jitter-free output and CS rising edges between frames.
 */
class OutputEngineTimingTest {
public:
	void init();
	void update();

private:
	bool initialized_ = false;
	uint32_t last_print_us_ = 0;
	uint32_t last_step_us_ = 0;
	uint16_t ramp_value_ = 0;
	bool ramp_up_ = true;
	uint64_t last_total_frames_ = 0;
};

}  // namespace sandbox::apps
