#include "pots_storage_stress_test.h"

#include <stdio.h>
#include <cstring>

#include "pico/stdlib.h"

#include "storage.h"

namespace sandbox::apps {

void PotsStorageStressTest::init() {
	stdio_init_all();
	sleep_ms(800);

	printf("\n\r--------\n\r");
	printf("Pots + Storage stress test\n");
	printf("Continuously reads pots while writing to flash every ~1s.\n");
	printf("Verifies AdcEngine::pause_for_flash / resume_after_flash works:\n");
	printf("  - pot values should keep updating after every flash write\n");
	printf("  - firmware should never freeze, no matter how many writes\n");
	printf("Turn the pots during the test to confirm they stay responsive.\n\n");

	if (!brain_init_succeeded(brain_.init_pots())) {
		printf("[ERROR] init_pots failed\n");
		initialized_ = false;
		return;
	}
	if (!brain_init_succeeded(brain_.init_storage())) {
		printf("[ERROR] init_storage failed\n");
		initialized_ = false;
		return;
	}

	const uint32_t now = to_us_since_boot(get_absolute_time());
	last_print_us_ = now;
	last_flash_us_ = now;

	printf("Running. (one flash write every 3 s)\n");
	initialized_ = true;
}

bool PotsStorageStressTest::do_flash_write(uint32_t counter) {
	// Snapshot pots before the flash op so we can compare after.
	pots_before_flash_[0] = brain_.pots.get_raw(0);
	pots_before_flash_[1] = brain_.pots.get_raw(1);
	pots_before_flash_[2] = brain_.pots.get_raw(2);

	// Build a small blob: counter + pot snapshot + a tag.
	struct Blob {
		uint32_t counter;
		uint16_t pots[3];
		uint16_t tag;  // 0xCAFE so we can recognise our blob on readback
	};
	Blob blob;
	blob.counter = counter;
	blob.pots[0] = pots_before_flash_[0];
	blob.pots[1] = pots_before_flash_[1];
	blob.pots[2] = pots_before_flash_[2];
	blob.tag = 0xCAFE;

	const StorageStatus status = brain_.storage.write_app_blob(&blob, sizeof(blob));
	if (status != StorageStatus::kOk) {
		++flash_failure_count_;
		printf("\n[ERROR] write_app_blob failed (status=%d) write_count=%lu\n",
			static_cast<int>(status), static_cast<unsigned long>(flash_write_count_));
		return false;
	}

	// Read back to verify the write committed correctly.
	Blob readback;
	size_t actual_size = 0;
	const StorageStatus read_status = brain_.storage.read_app_blob(
		&readback, sizeof(readback), &actual_size);
	if (read_status != StorageStatus::kOk) {
		++flash_failure_count_;
		printf("\n[ERROR] read_app_blob failed (status=%d)\n", static_cast<int>(read_status));
		return false;
	}
	if (actual_size != sizeof(Blob) ||
	    std::memcmp(&blob, &readback, sizeof(Blob)) != 0) {
		++flash_failure_count_;
		printf("\n[ERROR] blob roundtrip mismatch\n");
		return false;
	}

	// Snapshot pots after the flash op for the periodic print.
	pots_after_flash_[0] = brain_.pots.get_raw(0);
	pots_after_flash_[1] = brain_.pots.get_raw(1);
	pots_after_flash_[2] = brain_.pots.get_raw(2);

	++flash_write_count_;
	return true;
}

void PotsStorageStressTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now = to_us_since_boot(get_absolute_time());

	// Run the flash write on schedule.
	if ((now - last_flash_us_) >= flash_interval_us_) {
		last_flash_us_ = now;
		do_flash_write(flash_write_count_ + 1);
	}

	// Print stats roughly every 250 ms.
	if ((now - last_print_us_) >= 250'000) {
		last_print_us_ = now;

		const uint16_t p0 = brain_.pots.get_raw(0);
		const uint16_t p1 = brain_.pots.get_raw(1);
		const uint16_t p2 = brain_.pots.get_raw(2);

		// "Stale?" hint: did the post-flash snapshot match the pre-flash
		// snapshot exactly? Equality across all 3 means pots didn't visibly
		// move during the flash op. False positive if you held all knobs
		// still — turn them and re-check.
		const bool stale_hint =
			(pots_before_flash_[0] == pots_after_flash_[0]) &&
			(pots_before_flash_[1] == pots_after_flash_[1]) &&
			(pots_before_flash_[2] == pots_after_flash_[2]) &&
			flash_write_count_ > 0;

		printf("Writes=%lu Failures=%lu Pot=[%4u %4u %4u] %s\n",
			static_cast<unsigned long>(flash_write_count_),
			static_cast<unsigned long>(flash_failure_count_),
			static_cast<unsigned>(p0),
			static_cast<unsigned>(p1),
			static_cast<unsigned>(p2),
			stale_hint ? "(post-flash same as pre, turn pots to verify)" : "");
		fflush(stdout);
	}

	sleep_ms(1);
}

}  // namespace sandbox::apps
