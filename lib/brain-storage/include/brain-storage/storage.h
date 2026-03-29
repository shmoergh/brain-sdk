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

}  // namespace brain::storage
