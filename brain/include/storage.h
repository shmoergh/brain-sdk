#pragma once

#include <cstddef>
#include <cstdint>

#include "storage-common.h"

#ifndef BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT
#define BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT 0
#endif

constexpr bool kAllowUnprotectedLayout =
	BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT != 0;

enum class StorageRegion : uint8_t {
	kAppData = 0,
	kCalibration,
};

struct CvCalibrationV1 {
	int16_t a_offset_lsb[10];
	int16_t b_offset_lsb[10];
};

bool is_layout_protected();
uint32_t region_offset(StorageRegion region);
size_t region_size(StorageRegion region);

StorageStatus read_region(
	StorageRegion region, uint32_t offset, void* out, size_t size);

StorageStatus write_region(
	StorageRegion region, uint32_t offset, const void* data, size_t size);

StorageStatus erase_region(StorageRegion region);

StorageStatus read_cv_calibration(CvCalibrationV1* out);
StorageStatus write_cv_calibration(const CvCalibrationV1* in);
StorageStatus clear_cv_calibration();

StorageStatus read_app_blob(void* out, size_t max_size, size_t* actual_size);
StorageStatus write_app_blob(const void* data, size_t size);
StorageStatus clear_app_blob();

namespace brain {
namespace storage {
using ::StorageRegion;
using ::CvCalibrationV1;
using ::StorageStatus;
constexpr bool kAllowUnprotectedLayout = ::kAllowUnprotectedLayout;
namespace layout = ::StorageLayout;

inline bool is_layout_protected() { return ::is_layout_protected(); }
inline uint32_t region_offset(StorageRegion region) { return ::region_offset(region); }
inline size_t region_size(StorageRegion region) { return ::region_size(region); }
inline StorageStatus read_region(StorageRegion region, uint32_t offset, void* out, size_t size) {
	return ::read_region(region, offset, out, size);
}
inline StorageStatus write_region(StorageRegion region, uint32_t offset, const void* data, size_t size) {
	return ::write_region(region, offset, data, size);
}
inline StorageStatus erase_region(StorageRegion region) { return ::erase_region(region); }
inline StorageStatus read_cv_calibration(CvCalibrationV1* out) { return ::read_cv_calibration(out); }
inline StorageStatus write_cv_calibration(const CvCalibrationV1* in) { return ::write_cv_calibration(in); }
inline StorageStatus clear_cv_calibration() { return ::clear_cv_calibration(); }
inline StorageStatus read_app_blob(void* out, size_t max_size, size_t* actual_size) {
	return ::read_app_blob(out, max_size, actual_size);
}
inline StorageStatus write_app_blob(const void* data, size_t size) { return ::write_app_blob(data, size); }
inline StorageStatus clear_app_blob() { return ::clear_app_blob(); }
}  // namespace storage
}  // namespace brain
