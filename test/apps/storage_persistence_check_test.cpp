#include "storage_persistence_check_test.h"

#include <pico/stdlib.h>
#include <stdio.h>

#include "storage.h"

namespace sandbox::apps {

namespace {

const char* to_string(brain::storage::StorageStatus status) {
	switch (status) {
		case brain::storage::StorageStatus::kOk:
			return "kOk";
		case brain::storage::StorageStatus::kInvalidArgument:
			return "kInvalidArgument";
		case brain::storage::StorageStatus::kNotFound:
			return "kNotFound";
		case brain::storage::StorageStatus::kCorrupt:
			return "kCorrupt";
		case brain::storage::StorageStatus::kOutOfBounds:
			return "kOutOfBounds";
		case brain::storage::StorageStatus::kTooLarge:
			return "kTooLarge";
		case brain::storage::StorageStatus::kUnprotectedLayout:
			return "kUnprotectedLayout";
		case brain::storage::StorageStatus::kFlashError:
			return "kFlashError";
		case brain::storage::StorageStatus::kTimeout:
			return "kTimeout";
		case brain::storage::StorageStatus::kNotPermitted:
			return "kNotPermitted";
		default:
			return "unknown";
	}
}

bool matches_storage_test_seed(const brain::storage::CvCalibrationV1& calibration) {
	for (int i = 0; i < 10; i++) {
		if (calibration.a_offset_lsb[i] != static_cast<int16_t>(-10 * (i + 1))) {
			return false;
		}
		if (calibration.b_offset_lsb[i] != static_cast<int16_t>(5 * (i + 1))) {
			return false;
		}
	}
	return true;
}

void print_offsets(const char* name, const int16_t* values) {
	printf("%s:", name);
	for (int i = 0; i < 10; i++) {
		printf(" %d", static_cast<int>(values[i]));
	}
	printf("\n");
}

}  // namespace

void StoragePersistenceCheckTest::init() {
	stdio_init_all();
	sleep_ms(1200);

	printf("\n\r--------\n\r");
	printf("Brain Storage Persistence Check\n");
	printf("Layout protected: %s\n",
		brain::storage::is_layout_protected() ? "yes" : "no");
	printf("Unsafe override compiled: %s\n",
		brain::storage::kAllowUnprotectedLayout ? "yes" : "no");
	printf("App region offset/size: %u / %u\n",
		static_cast<unsigned>(
			brain::storage::region_offset(brain::storage::StorageRegion::kAppData)),
		static_cast<unsigned>(
			brain::storage::region_size(brain::storage::StorageRegion::kAppData)));
	printf("Cal region offset/size: %u / %u\n",
		static_cast<unsigned>(
			brain::storage::region_offset(brain::storage::StorageRegion::kCalibration)),
		static_cast<unsigned>(
			brain::storage::region_size(brain::storage::StorageRegion::kCalibration)));
	printf("Guard region offset/size: %u / %u\n",
		static_cast<unsigned>(brain::storage::layout::kGuardRegionOffsetBytes),
		static_cast<unsigned>(brain::storage::layout::kGuardRegionSizeBytes));

	initialized_ = true;
}

void StoragePersistenceCheckTest::update() {
	if (!initialized_ || completed_) {
		sleep_ms(10);
		return;
	}

	brain::storage::CvCalibrationV1 calibration{};
	brain::storage::StorageStatus status =
		brain::storage::read_cv_calibration(&calibration);
	uint8_t app_blob[64] = {0};
	size_t app_blob_size = 0;
	brain::storage::StorageStatus app_blob_status =
		brain::storage::read_app_blob(app_blob, sizeof(app_blob), &app_blob_size);

	printf("read_cv_calibration status: %s\n", to_string(status));
	if (status == brain::storage::StorageStatus::kOk) {
		print_offsets("a_offset_lsb[1V..10V]", calibration.a_offset_lsb);
		print_offsets("b_offset_lsb[1V..10V]", calibration.b_offset_lsb);

		if (matches_storage_test_seed(calibration)) {
			printf("[PASS] Calibration matches StorageTest seed pattern.\n");
		} else {
			printf("[INFO] Calibration is valid but does not match StorageTest seed pattern.\n");
		}
	} else if (status == brain::storage::StorageStatus::kNotFound) {
		printf("[FAIL] No calibration record found.\n");
	} else if (status == brain::storage::StorageStatus::kCorrupt) {
		printf("[FAIL] Calibration record is corrupt.\n");
	} else {
		printf("[FAIL] Unexpected calibration read status.\n");
	}

	printf("read_app_blob status: %s (size=%u)\n",
		to_string(app_blob_status),
		static_cast<unsigned>(app_blob_size));
	if (app_blob_status == brain::storage::StorageStatus::kOk) {
		printf("[INFO] App blob exists after firmware update.\n");
	} else if (app_blob_status == brain::storage::StorageStatus::kNotFound) {
		printf("[INFO] App blob not found after firmware update (acceptable).\n");
	} else {
		printf("[WARN] App blob read returned unexpected status.\n");
	}

	printf("Execution complete. Power cycle or reset to run again.\n");
	completed_ = true;
}

}  // namespace sandbox::apps
