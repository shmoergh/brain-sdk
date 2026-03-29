#include "storage_test.h"

#include <pico/stdlib.h>
#include <stdio.h>

#include <cstdint>
#include <cstring>

#include "brain-storage/storage.h"

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

void print_result(const char* name, bool pass) {
	printf("[%s] %s\n", pass ? "PASS" : "FAIL", name);
}

bool calibration_equal(const brain::storage::CvCalibrationV1& a, const brain::storage::CvCalibrationV1& b) {
	return std::memcmp(&a, &b, sizeof(brain::storage::CvCalibrationV1)) == 0;
}

}  // namespace

void StorageTest::init() {
	stdio_init_all();
	sleep_ms(1200);

	printf("\n\r--------\n\r");
	printf("Brain Storage Test (Phase 2+3+4)\n");
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

	initialized_ = true;
}

void StorageTest::update() {
	if (!initialized_ || completed_) {
		sleep_ms(10);
		return;
	}

	bool overall_pass = true;

	uint8_t calibration_before[64] = {0};
	uint8_t calibration_after[64] = {0};
	uint8_t calibration_before_app_blob[64] = {0};
	uint8_t calibration_after_app_blob[64] = {0};
	uint8_t pattern[64] = {0};
	uint8_t app_readback[64] = {0};
	uint8_t app_blob_pattern[96] = {0};
	uint8_t app_blob_readback[96] = {0};
	uint8_t app_blob_oversize_guard = 0xA5;
	brain::storage::CvCalibrationV1 calibration_in{};
	brain::storage::CvCalibrationV1 calibration_out{};
	size_t app_blob_actual_size = 0;

	for (size_t i = 0; i < sizeof(pattern); i++) {
		pattern[i] = static_cast<uint8_t>(0xA0 + i);
	}
	for (size_t i = 0; i < sizeof(app_blob_pattern); i++) {
		app_blob_pattern[i] = static_cast<uint8_t>(0x31 + (i % 57));
	}

	brain::storage::StorageStatus status = brain::storage::read_region(
		brain::storage::StorageRegion::kCalibration,
		0,
		calibration_before,
		sizeof(calibration_before));
	bool step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Read calibration snapshot before app write", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::write_region(
		brain::storage::StorageRegion::kAppData,
		0,
		pattern,
		sizeof(pattern));
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Write 64-byte app pattern", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::read_region(
		brain::storage::StorageRegion::kAppData,
		0,
		app_readback,
		sizeof(app_readback));
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Read back app pattern", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	step_pass = (std::memcmp(pattern, app_readback, sizeof(pattern)) == 0);
	print_result("Verify app readback matches written pattern", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	status = brain::storage::read_region(
		brain::storage::StorageRegion::kCalibration,
		0,
		calibration_after,
		sizeof(calibration_after));
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Read calibration snapshot after app write", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	step_pass =
		(std::memcmp(calibration_before, calibration_after, sizeof(calibration_before)) == 0);
	print_result("Verify calibration region is unchanged", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	status = brain::storage::write_region(
		brain::storage::StorageRegion::kAppData,
		static_cast<uint32_t>(brain::storage::region_size(
			brain::storage::StorageRegion::kAppData) - 8),
		pattern,
		16);
	step_pass = (status == brain::storage::StorageStatus::kTooLarge);
	print_result("Reject out-of-bounds app write", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kTooLarge)\n", to_string(status));
		overall_pass = false;
	}

	for (int i = 0; i < 10; i++) {
		calibration_in.a_offset_lsb[i] = static_cast<int16_t>(i * 3 - 12);
		calibration_in.b_offset_lsb[i] = static_cast<int16_t>(30 - i * 2);
	}

	status = brain::storage::write_cv_calibration(&calibration_in);
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Write calibration record", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::read_cv_calibration(&calibration_out);
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Read calibration record", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	step_pass = calibration_equal(calibration_in, calibration_out);
	print_result("Verify calibration payload roundtrip", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	uint8_t calibration_corrupt_byte = 0;
	status = brain::storage::read_region(
		brain::storage::StorageRegion::kCalibration,
		8,
		&calibration_corrupt_byte,
		1);
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Read one calibration byte for corruption test", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	calibration_corrupt_byte ^= 0x5A;
	status = brain::storage::write_region(
		brain::storage::StorageRegion::kCalibration,
		8,
		&calibration_corrupt_byte,
		1);
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Corrupt one calibration byte", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::read_cv_calibration(&calibration_out);
	step_pass = (status == brain::storage::StorageStatus::kCorrupt);
	print_result("Detect corrupted calibration record", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kCorrupt)\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::clear_cv_calibration();
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Clear calibration sector", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::read_cv_calibration(&calibration_out);
	step_pass = (status == brain::storage::StorageStatus::kNotFound);
	print_result("Read calibration after clear returns not found", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kNotFound)\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::write_cv_calibration(&calibration_in);
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Re-write calibration baseline for app-blob isolation test", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::read_region(
		brain::storage::StorageRegion::kCalibration,
		0,
		calibration_before_app_blob,
		sizeof(calibration_before_app_blob));
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Read calibration snapshot before app blob ops", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::write_app_blob(app_blob_pattern, sizeof(app_blob_pattern));
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Write app blob record", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::read_app_blob(
		app_blob_readback,
		sizeof(app_blob_readback),
		&app_blob_actual_size);
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Read app blob record", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	step_pass = (app_blob_actual_size == sizeof(app_blob_pattern))
		&& (std::memcmp(
				app_blob_pattern,
				app_blob_readback,
				sizeof(app_blob_pattern)) == 0);
	print_result("Verify app blob payload roundtrip", step_pass);
	if (!step_pass) {
		printf("  actual_size=%u expected=%u\n",
			static_cast<unsigned>(app_blob_actual_size),
			static_cast<unsigned>(sizeof(app_blob_pattern)));
		overall_pass = false;
	}

	status = brain::storage::write_app_blob(
		&app_blob_oversize_guard,
		brain::storage::region_size(brain::storage::StorageRegion::kAppData));
	step_pass = (status == brain::storage::StorageStatus::kTooLarge);
	print_result("Reject oversize app blob write", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kTooLarge)\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::clear_app_blob();
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Clear app blob sector", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::read_app_blob(
		app_blob_readback,
		sizeof(app_blob_readback),
		&app_blob_actual_size);
	step_pass = (status == brain::storage::StorageStatus::kNotFound);
	print_result("Read app blob after clear returns not found", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kNotFound)\n", to_string(status));
		overall_pass = false;
	}

	status = brain::storage::read_region(
		brain::storage::StorageRegion::kCalibration,
		0,
		calibration_after_app_blob,
		sizeof(calibration_after_app_blob));
	step_pass = (status == brain::storage::StorageStatus::kOk);
	print_result("Read calibration snapshot after app blob ops", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	step_pass = (std::memcmp(
		calibration_before_app_blob,
		calibration_after_app_blob,
		sizeof(calibration_before_app_blob)) == 0);
	print_result("Verify calibration unchanged by app blob ops", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	printf("\nPhase 2+3+4 storage test result: %s\n",
		overall_pass ? "PASS" : "FAIL");
	printf("Execution complete. Power cycle or reset to run again.\n");

	completed_ = true;
}

}  // namespace sandbox::apps
