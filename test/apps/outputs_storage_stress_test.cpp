#include "outputs_storage_stress_test.h"

#include <stdio.h>
#include <cstring>

#include "pico/stdlib.h"

#include "output-engine.h"
#include "storage.h"

namespace sandbox::apps {

namespace {

constexpr int32_t kOutputMinMv = 0;
constexpr int32_t kOutputMaxMv = 10'000;
constexpr int32_t kOutputStepMv = 40;

}  // namespace

void OutputsStorageStressTest::init() {
	stdio_init_all();
	sleep_ms(800);

	printf("\n\r--------\n\r");
	printf("Outputs + Storage stress test\n");
	printf("Continuously sweeps OUT A/B while writing to flash every ~3s.\n");
	printf("Verifies output DMA stream stays alive while storage writes occur:\n");
	printf("  - outputs should keep moving after every flash write\n");
	printf("  - firmware should never freeze\n");
	printf("If possible, scope OUT A/OUT B and DAC CS for continuity.\n\n");

	const BrainInitStatus outputs_status = brain_.init_outputs();
	if (!brain_init_succeeded(outputs_status)) {
		printf("[ERROR] init_outputs failed (%d)\n", static_cast<int>(outputs_status));
		initialized_ = false;
		return;
	}

	const BrainInitStatus storage_status = brain_.init_storage();
	if (!brain_init_succeeded(storage_status)) {
		printf("[ERROR] init_storage failed (%d)\n", static_cast<int>(storage_status));
		initialized_ = false;
		return;
	}

	if (!brain_.outputs.set_voltage_millivolts(kOutputsChannelA, out_a_millivolts_) ||
	    !brain_.outputs.set_voltage_millivolts(kOutputsChannelB, out_b_millivolts_)) {
		printf("[ERROR] initial set_voltage_millivolts failed\n");
		initialized_ = false;
		return;
	}

	const uint32_t now = to_us_since_boot(get_absolute_time());
	last_print_us_ = now;
	last_flash_us_ = now;
	last_sweep_step_us_ = now;

	printf("Running. (one flash write every 3 s)\n");
	initialized_ = true;
}

bool OutputsStorageStressTest::do_flash_write(uint32_t counter) {
	const auto before = brain::internal::OutputEngine::instance().get_snapshot();
	hold_before_flash_[0] = static_cast<uint16_t>(before.hold_frame_a & 0x0FFF);
	hold_before_flash_[1] = static_cast<uint16_t>(before.hold_frame_b & 0x0FFF);

	struct Blob {
		uint32_t counter;
		int32_t out_a_mv;
		int32_t out_b_mv;
		uint16_t hold_a_dac;
		uint16_t hold_b_dac;
		uint16_t tag;
	};
	Blob blob{};
	blob.counter = counter;
	blob.out_a_mv = out_a_millivolts_;
	blob.out_b_mv = out_b_millivolts_;
	blob.hold_a_dac = hold_before_flash_[0];
	blob.hold_b_dac = hold_before_flash_[1];
	blob.tag = 0xBEEF;

	const StorageStatus write_status = brain_.storage.write_app_blob(&blob, sizeof(blob));
	if (write_status != StorageStatus::kOk) {
		++failure_count_;
		printf("\n[ERROR] write_app_blob failed (status=%d) writes=%lu\n",
			static_cast<int>(write_status),
			static_cast<unsigned long>(flash_write_count_));
		return false;
	}

	Blob readback{};
	size_t actual_size = 0;
	const StorageStatus read_status = brain_.storage.read_app_blob(
		&readback, sizeof(readback), &actual_size);
	if (read_status != StorageStatus::kOk) {
		++failure_count_;
		printf("\n[ERROR] read_app_blob failed (status=%d)\n", static_cast<int>(read_status));
		return false;
	}
	if (actual_size != sizeof(Blob) || std::memcmp(&blob, &readback, sizeof(Blob)) != 0) {
		++failure_count_;
		printf("\n[ERROR] blob roundtrip mismatch\n");
		return false;
	}

	const auto after = brain::internal::OutputEngine::instance().get_snapshot();
	hold_after_flash_[0] = static_cast<uint16_t>(after.hold_frame_a & 0x0FFF);
	hold_after_flash_[1] = static_cast<uint16_t>(after.hold_frame_b & 0x0FFF);

	++flash_write_count_;
	return true;
}

void OutputsStorageStressTest::update_output_sweep() {
	if (sweep_up_) {
		out_a_millivolts_ += kOutputStepMv;
		if (out_a_millivolts_ >= kOutputMaxMv) {
			out_a_millivolts_ = kOutputMaxMv;
			sweep_up_ = false;
		}
	} else {
		out_a_millivolts_ -= kOutputStepMv;
		if (out_a_millivolts_ <= kOutputMinMv) {
			out_a_millivolts_ = kOutputMinMv;
			sweep_up_ = true;
		}
	}
	out_b_millivolts_ = kOutputMaxMv - out_a_millivolts_;

	const bool ok_a = brain_.outputs.set_voltage_millivolts(kOutputsChannelA, out_a_millivolts_);
	const bool ok_b = brain_.outputs.set_voltage_millivolts(kOutputsChannelB, out_b_millivolts_);
	if (!ok_a || !ok_b) {
		++failure_count_;
		printf("\n[ERROR] set_voltage failed (A=%d B=%d)\n", static_cast<int>(ok_a), static_cast<int>(ok_b));
	}
}

void OutputsStorageStressTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now = to_us_since_boot(get_absolute_time());

	if ((now - last_sweep_step_us_) >= sweep_step_interval_us_) {
		last_sweep_step_us_ = now;
		update_output_sweep();
	}

	if ((now - last_flash_us_) >= flash_interval_us_) {
		last_flash_us_ = now;
		do_flash_write(flash_write_count_ + 1);
	}

	if ((now - last_print_us_) >= 250'000) {
		last_print_us_ = now;
		const auto snap = brain::internal::OutputEngine::instance().get_snapshot();
		const bool stale_hint =
			(hold_before_flash_[0] == hold_after_flash_[0]) &&
			(hold_before_flash_[1] == hold_after_flash_[1]) &&
			flash_write_count_ > 0;

		printf("Writes=%lu Failures=%lu OutmV=[%5ld %5ld] HoldDAC=[%4u %4u] Engine=%s %s\n",
			static_cast<unsigned long>(flash_write_count_),
			static_cast<unsigned long>(failure_count_),
			static_cast<long>(out_a_millivolts_),
			static_cast<long>(out_b_millivolts_),
			static_cast<unsigned>(snap.hold_frame_a & 0x0FFF),
			static_cast<unsigned>(snap.hold_frame_b & 0x0FFF),
			brain::internal::OutputEngine::instance().is_running() ? "RUN" : "STOP",
			stale_hint ? "(post-flash hold same as pre)" : "");
		fflush(stdout);
	}

	sleep_ms(1);
}

}  // namespace sandbox::apps
