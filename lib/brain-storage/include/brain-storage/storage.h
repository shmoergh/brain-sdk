#pragma once

#include <cstddef>
#include <cstdint>

#include "brain-storage/storage-common.h"

namespace brain::storage {

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

// Returns true when the firmware binary ends before the reserved app-data
// sector starts, meaning storage regions are linker-protected.
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

}  // namespace brain::storage
