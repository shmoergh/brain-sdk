#include "storage_test.h"

#include <pico/stdlib.h>
#include <stdio.h>

#include <cstdint>
#include <cstring>

#include "outputs.h"
#include "storage.h"

namespace sandbox::apps {

namespace {

const char* to_string(StorageStatus status) {
	switch (status) {
		case StorageStatus::kOk:
			return "kOk";
		case StorageStatus::kInvalidArgument:
			return "kInvalidArgument";
		case StorageStatus::kNotFound:
			return "kNotFound";
		case StorageStatus::kCorrupt:
			return "kCorrupt";
		case StorageStatus::kOutOfBounds:
			return "kOutOfBounds";
		case StorageStatus::kTooLarge:
			return "kTooLarge";
		case StorageStatus::kUnprotectedLayout:
			return "kUnprotectedLayout";
		case StorageStatus::kFlashError:
			return "kFlashError";
		case StorageStatus::kTimeout:
			return "kTimeout";
		case StorageStatus::kNotPermitted:
			return "kNotPermitted";
		default:
			return "unknown";
	}
}

void print_result(const char* name, bool pass) {
	printf("[%s] %s\n", pass ? "PASS" : "FAIL", name);
}

bool calibration_equal(const CvCalibrationV1& a, const CvCalibrationV1& b) {
	return std::memcmp(&a, &b, sizeof(CvCalibrationV1)) == 0;
}

int16_t legacy_round_to_int16(float value) {
	if (value >= 0.0f) {
		return static_cast<int16_t>(value + 0.5f);
	}
	return static_cast<int16_t>(value - 0.5f);
}

int32_t legacy_voltage_to_dac_from_millivolts(int32_t millivolts) {
	float voltage = static_cast<float>(millivolts) / 1000.0f;
	float normalized = voltage / 10.0f;
	uint16_t dac_value = static_cast<uint16_t>(normalized * 4095.0f + 0.5f);
	return (dac_value > 4095) ? 4095 : dac_value;
}

int16_t legacy_interpolated_offset_lsb(const int16_t* offsets, int32_t millivolts) {
	float clamped_voltage = static_cast<float>(millivolts) / 1000.0f;

	if (clamped_voltage <= 0.0f) {
		return 0;
	}
	if (clamped_voltage < 1.0f) {
		return legacy_round_to_int16(static_cast<float>(offsets[0]) * clamped_voltage);
	}
	if (clamped_voltage >= 10.0f) {
		return offsets[9];
	}

	const int lower_whole_volt = static_cast<int>(clamped_voltage);
	const int lower_idx = lower_whole_volt - 1;
	const int upper_idx = lower_idx + 1;
	const float t = clamped_voltage - static_cast<float>(lower_whole_volt);

	const float lower_offset = static_cast<float>(offsets[lower_idx]);
	const float upper_offset = static_cast<float>(offsets[upper_idx]);
	return legacy_round_to_int16(lower_offset + (upper_offset - lower_offset) * t);
}

int32_t legacy_expected_calibrated_dac_channel_a(int32_t requested_millivolts, bool bipolar_range,
	const CvCalibrationV1& calibration) {
	int32_t dac_domain_millivolts = requested_millivolts;
	if (bipolar_range) {
		dac_domain_millivolts += 5000;
	}
	if (dac_domain_millivolts < 0) dac_domain_millivolts = 0;
	if (dac_domain_millivolts > 10000) dac_domain_millivolts = 10000;

	int32_t raw_dac = legacy_voltage_to_dac_from_millivolts(dac_domain_millivolts);
	int32_t with_offset = raw_dac + legacy_interpolated_offset_lsb(calibration.a_offset_lsb, dac_domain_millivolts);
	if (with_offset < 0) return 0;
	if (with_offset > 4095) return 4095;
	return with_offset;
}

int16_t expected_channel_a_offset(int32_t millivolts) {
	if (millivolts <= 0) {
		return 0;
	}
	if (millivolts < 1000) {
		return -5;	// between 0 and -10 at 500mV for test case
	}
	if (millivolts == 1000) {
		return -10;
	}
	if (millivolts == 5500) {
		return -55;
	}
	if (millivolts >= 10000) {
		return -100;
	}
	return 0;
}

}  // namespace

void StorageTest::init() {
	stdio_init_all();
	sleep_ms(1200);

	printf("\n\r--------\n\r");
	printf("Brain Storage Test (Phase 2+3+4+5)\n");
	printf("Layout protected: %s\n",
		is_layout_protected() ? "yes" : "no");
	printf("Unsafe override compiled: %s\n",
		kAllowUnprotectedLayout ? "yes" : "no");
	printf("App region offset/size: %u / %u\n",
		static_cast<unsigned>(
			region_offset(StorageRegion::kAppData)),
		static_cast<unsigned>(
			region_size(StorageRegion::kAppData)));
	printf("Cal region offset/size: %u / %u\n",
		static_cast<unsigned>(
			region_offset(StorageRegion::kCalibration)),
		static_cast<unsigned>(
			region_size(StorageRegion::kCalibration)));
	printf("Guard region offset/size: %u / %u\n",
		static_cast<unsigned>(StorageLayout::kGuardRegionOffsetBytes),
		static_cast<unsigned>(StorageLayout::kGuardRegionSizeBytes));

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
	CvCalibrationV1 calibration_in{};
	CvCalibrationV1 calibration_out{};
	AudioCvOut dac{};
	size_t app_blob_actual_size = 0;

	for (size_t i = 0; i < sizeof(pattern); i++) {
		pattern[i] = static_cast<uint8_t>(0xA0 + i);
	}
	for (size_t i = 0; i < sizeof(app_blob_pattern); i++) {
		app_blob_pattern[i] = static_cast<uint8_t>(0x31 + (i % 57));
	}

	StorageStatus status = read_region(
		StorageRegion::kCalibration,
		0,
		calibration_before,
		sizeof(calibration_before));
	bool step_pass = (status == StorageStatus::kOk);
	print_result("Read calibration snapshot before app write", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = write_region(
		StorageRegion::kAppData,
		0,
		pattern,
		sizeof(pattern));
	step_pass = (status == StorageStatus::kOk);
	print_result("Write 64-byte app pattern", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = read_region(
		StorageRegion::kAppData,
		0,
		app_readback,
		sizeof(app_readback));
	step_pass = (status == StorageStatus::kOk);
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

	status = read_region(
		StorageRegion::kCalibration,
		0,
		calibration_after,
		sizeof(calibration_after));
	step_pass = (status == StorageStatus::kOk);
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

	status = write_region(
		StorageRegion::kAppData,
		static_cast<uint32_t>(region_size(
			StorageRegion::kAppData) - 8),
		pattern,
		16);
	step_pass = (status == StorageStatus::kTooLarge);
	print_result("Reject out-of-bounds app write", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kTooLarge)\n", to_string(status));
		overall_pass = false;
	}

	for (int i = 0; i < 10; i++) {
		calibration_in.a_offset_lsb[i] = static_cast<int16_t>(i * 3 - 12);
		calibration_in.b_offset_lsb[i] = static_cast<int16_t>(30 - i * 2);
	}

	status = write_cv_calibration(&calibration_in);
	step_pass = (status == StorageStatus::kOk);
	print_result("Write calibration record", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = read_cv_calibration(&calibration_out);
	step_pass = (status == StorageStatus::kOk);
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
	status = read_region(
		StorageRegion::kCalibration,
		8,
		&calibration_corrupt_byte,
		1);
	step_pass = (status == StorageStatus::kOk);
	print_result("Read one calibration byte for corruption test", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	calibration_corrupt_byte ^= 0x5A;
	status = write_region(
		StorageRegion::kCalibration,
		8,
		&calibration_corrupt_byte,
		1);
	step_pass = (status == StorageStatus::kOk);
	print_result("Corrupt one calibration byte", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = read_cv_calibration(&calibration_out);
	step_pass = (status == StorageStatus::kCorrupt);
	print_result("Detect corrupted calibration record", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kCorrupt)\n", to_string(status));
		overall_pass = false;
	}

	status = clear_cv_calibration();
	step_pass = (status == StorageStatus::kOk);
	print_result("Clear calibration sector", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = read_cv_calibration(&calibration_out);
	step_pass = (status == StorageStatus::kNotFound);
	print_result("Read calibration after clear returns not found", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kNotFound)\n", to_string(status));
		overall_pass = false;
	}

	status = write_cv_calibration(&calibration_in);
	step_pass = (status == StorageStatus::kOk);
	print_result("Re-write calibration baseline for app-blob isolation test", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = read_region(
		StorageRegion::kCalibration,
		0,
		calibration_before_app_blob,
		sizeof(calibration_before_app_blob));
	step_pass = (status == StorageStatus::kOk);
	print_result("Read calibration snapshot before app blob ops", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = write_app_blob(app_blob_pattern, sizeof(app_blob_pattern));
	step_pass = (status == StorageStatus::kOk);
	print_result("Write app blob record", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = read_app_blob(
		app_blob_readback,
		sizeof(app_blob_readback),
		&app_blob_actual_size);
	step_pass = (status == StorageStatus::kOk);
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

	status = write_app_blob(
		&app_blob_oversize_guard,
		region_size(StorageRegion::kAppData));
	step_pass = (status == StorageStatus::kTooLarge);
	print_result("Reject oversize app blob write", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kTooLarge)\n", to_string(status));
		overall_pass = false;
	}

	status = clear_app_blob();
	step_pass = (status == StorageStatus::kOk);
	print_result("Clear app blob sector", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	status = read_app_blob(
		app_blob_readback,
		sizeof(app_blob_readback),
		&app_blob_actual_size);
	step_pass = (status == StorageStatus::kNotFound);
	print_result("Read app blob after clear returns not found", step_pass);
	if (!step_pass) {
		printf("  status=%s (expected kNotFound)\n", to_string(status));
		overall_pass = false;
	}

	status = read_region(
		StorageRegion::kCalibration,
		0,
		calibration_after_app_blob,
		sizeof(calibration_after_app_blob));
	step_pass = (status == StorageStatus::kOk);
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

	step_pass = dac.init();
	print_result("Initialize AudioCvOut for calibration math tests", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	dac.clear_calibration();
	step_pass = !dac.has_calibration();
	print_result("Clear in-memory calibration state", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	const int32_t phase5_test_millivolts[] = {0, 500, 1000, 5500, 10000};
	bool zero_cal_match = true;
	for (int32_t millivolts : phase5_test_millivolts) {
		dac.set_voltage_millivolts(AudioCvOutChannel::kChannelA, millivolts);
		uint16_t raw_dac = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
		dac.set_voltage_calibrated_millivolts(AudioCvOutChannel::kChannelA, millivolts);
		uint16_t calibrated_dac = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
		if (raw_dac != calibrated_dac) {
			zero_cal_match = false;
			printf("  mismatch at %ldmV raw=%u calibrated=%u\n",
				static_cast<long>(millivolts),
				static_cast<unsigned>(raw_dac),
				static_cast<unsigned>(calibrated_dac));
		}
	}
	print_result("Zero-calibration calibrated output equals raw output", zero_cal_match);
	if (!zero_cal_match) {
		overall_pass = false;
	}

	for (int i = 0; i < 10; i++) {
		calibration_in.a_offset_lsb[i] = static_cast<int16_t>(-10 * (i + 1));
		calibration_in.b_offset_lsb[i] = static_cast<int16_t>(5 * (i + 1));
	}
	dac.set_calibration(calibration_in);
	step_pass = dac.has_calibration();
	print_result("Load synthetic in-memory calibration", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	bool synthetic_delta_match = true;
	for (int32_t millivolts : phase5_test_millivolts) {
		dac.set_voltage_millivolts(AudioCvOutChannel::kChannelA, millivolts);
		int32_t raw_dac = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
		dac.set_voltage_calibrated_millivolts(AudioCvOutChannel::kChannelA, millivolts);
		int32_t calibrated_dac = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
		int32_t expected_delta = expected_channel_a_offset(millivolts);
		if ((calibrated_dac - raw_dac) != expected_delta) {
			synthetic_delta_match = false;
			printf("  delta mismatch at %ldmV raw=%ld calibrated=%ld delta=%ld expected=%ld\n",
				static_cast<long>(millivolts),
				static_cast<long>(raw_dac),
				static_cast<long>(calibrated_dac),
				static_cast<long>(calibrated_dac - raw_dac),
				static_cast<long>(expected_delta));
		}
	}
	print_result("Synthetic calibration produces expected DAC deltas", synthetic_delta_match);
	if (!synthetic_delta_match) {
		overall_pass = false;
	}

	dac.set_voltage_calibrated_millivolts(AudioCvOutChannel::kChannelA, -1000);
	int32_t clamped_low_dac = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
	dac.set_voltage_calibrated_millivolts(AudioCvOutChannel::kChannelA, 11000);
	int32_t clamped_high_dac = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);

	step_pass = (clamped_low_dac == 0) && (clamped_high_dac == 3995);
	print_result("Out-of-range calibrated input clamps safely", step_pass);
	if (!step_pass) {
		printf("  clamped_low=%ld (expected 0), clamped_high=%ld (expected 3995)\n",
			static_cast<long>(clamped_low_dac),
			static_cast<long>(clamped_high_dac));
		overall_pass = false;
	}

	status = write_cv_calibration(&calibration_in);
	step_pass = (status == StorageStatus::kOk);
	print_result("Write calibration for load-from-flash API test", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	dac.clear_calibration();
	step_pass = !dac.has_calibration();
	print_result("Verify calibration cleared before flash load", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	step_pass = dac.load_calibration_from_flash();
	print_result("Load calibration from flash into AudioCvOut", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	dac.set_voltage_millivolts(AudioCvOutChannel::kChannelA, 1000);
	int32_t flash_load_raw_1v = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
	dac.set_voltage_calibrated_millivolts(AudioCvOutChannel::kChannelA, 1000);
	int32_t flash_load_cal_1v = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
	step_pass = ((flash_load_cal_1v - flash_load_raw_1v) == -10);
	print_result("Flash-loaded calibration is applied at 1V", step_pass);
	if (!step_pass) {
		printf("  raw=%ld calibrated=%ld delta=%ld expected=-10\n",
			static_cast<long>(flash_load_raw_1v),
			static_cast<long>(flash_load_cal_1v),
			static_cast<long>(flash_load_cal_1v - flash_load_raw_1v));
		overall_pass = false;
	}

	CvCalibrationV1 legacy_compat_cal{};
	for (int i = 0; i < 10; ++i) {
		legacy_compat_cal.a_offset_lsb[i] = static_cast<int16_t>((i % 2 == 0) ? (12 - i * 2) : (-9 - i * 3));
		legacy_compat_cal.b_offset_lsb[i] = static_cast<int16_t>((i % 2 == 0) ? (-7 + i) : (6 - i * 2));
	}

	status = write_cv_calibration(&legacy_compat_cal);
	step_pass = (status == StorageStatus::kOk);
	print_result("Write legacy-style calibration payload", step_pass);
	if (!step_pass) {
		printf("  status=%s\n", to_string(status));
		overall_pass = false;
	}

	step_pass = dac.load_calibration_from_flash();
	print_result("Load legacy-style calibration payload from flash", step_pass);
	if (!step_pass) {
		overall_pass = false;
	}

	const int32_t legacy_unipolar_points_mv[] = {0, 500, 1000, 2500, 5500, 7500, 10000};
	bool legacy_unipolar_match = true;
	dac.set_output_range(AudioCvOutChannel::kChannelA, AudioCvOutRange::kRange0To10V);
	for (int32_t mv : legacy_unipolar_points_mv) {
		dac.set_voltage_calibrated_millivolts(AudioCvOutChannel::kChannelA, mv);
		int32_t actual_dac = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
		int32_t expected_dac =
			legacy_expected_calibrated_dac_channel_a(mv, false, legacy_compat_cal);
		if (actual_dac != expected_dac) {
			legacy_unipolar_match = false;
			printf(
				"  legacy unipolar mismatch at %ldmV actual=%ld expected=%ld\n",
				static_cast<long>(mv),
				static_cast<long>(actual_dac),
				static_cast<long>(expected_dac));
		}
	}
	print_result("Legacy calibration compatibility (0..10V range)", legacy_unipolar_match);
	if (!legacy_unipolar_match) {
		overall_pass = false;
	}

	const int32_t legacy_bipolar_points_mv[] = {-5000, -2500, 0, 2500, 5000};
	bool legacy_bipolar_match = true;
	dac.set_output_range(AudioCvOutChannel::kChannelA, AudioCvOutRange::kRangeMinus5To5V);
	for (int32_t mv : legacy_bipolar_points_mv) {
		dac.set_voltage_calibrated_millivolts(AudioCvOutChannel::kChannelA, mv);
		int32_t actual_dac = dac.get_last_dac_value(AudioCvOutChannel::kChannelA);
		int32_t expected_dac =
			legacy_expected_calibrated_dac_channel_a(mv, true, legacy_compat_cal);
		if (actual_dac != expected_dac) {
			legacy_bipolar_match = false;
			printf(
				"  legacy bipolar mismatch at %ldmV actual=%ld expected=%ld\n",
				static_cast<long>(mv),
				static_cast<long>(actual_dac),
				static_cast<long>(expected_dac));
		}
	}
	print_result("Legacy calibration compatibility (-5..5V range)", legacy_bipolar_match);
	if (!legacy_bipolar_match) {
		overall_pass = false;
	}

	printf("\nPhase 2+3+4+5 storage test result: %s\n",
		overall_pass ? "PASS" : "FAIL");
	printf("Execution complete. Power cycle or reset to run again.\n");

	completed_ = true;
}

}  // namespace sandbox::apps
