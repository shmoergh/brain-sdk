#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

// Stress test for OutputEngine + Storage coexistence during flash writes.
//
// Continuously sweeps CV OUT A/B using manual hold updates while writing a
// small blob to flash every few seconds. The test checks write/read roundtrip
// integrity and reports output hold DAC codes each print tick so scope-based
// verification is easy:
//  - Scope OUT A/OUT B: should keep moving through the sweep
//  - Scope DAC CS/SCK/TX: stream should stay active across writes
//  - Firmware should never freeze
class OutputsStorageStressTest {
public:
	void init();
	void update();

private:
	bool do_flash_write(uint32_t counter);
	void update_output_sweep();

	Brain brain_{};
	bool initialized_ = false;
	uint32_t last_print_us_ = 0;
	uint32_t last_flash_us_ = 0;
	uint32_t last_sweep_step_us_ = 0;
	uint32_t flash_interval_us_ = 3'000'000;   // one flash write every 3 s
	uint32_t sweep_step_interval_us_ = 2'000;  // 500 Hz output updates
	uint32_t flash_write_count_ = 0;
	uint32_t failure_count_ = 0;
	int32_t out_a_millivolts_ = 0;
	int32_t out_b_millivolts_ = 10'000;
	bool sweep_up_ = true;
	uint16_t hold_before_flash_[2] = {0, 0};
	uint16_t hold_after_flash_[2] = {0, 0};
};

}  // namespace sandbox::apps
