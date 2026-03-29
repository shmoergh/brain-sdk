#include "brain-storage/storage.h"

#include <hardware/flash.h>
#include <pico/flash.h>
#include <pico/platform.h>

#include <cstdint>
#include <cstring>

namespace brain::storage {

extern "C" uint8_t __flash_binary_end;

namespace {

struct FlashProgramRequest {
	uint32_t offset_bytes;
	const uint8_t* data;
	size_t size;
};

void flash_program_sector_callback(void* param) {
	auto* request = static_cast<FlashProgramRequest*>(param);
	flash_range_erase(request->offset_bytes, request->size);
	flash_range_program(request->offset_bytes, request->data, request->size);
}

StorageStatus map_flash_safe_execute_result(int result) {
	switch (result) {
		case PICO_OK:
			return StorageStatus::kOk;
		case PICO_ERROR_TIMEOUT:
			return StorageStatus::kTimeout;
		case PICO_ERROR_NOT_PERMITTED:
			return StorageStatus::kNotPermitted;
		default:
			return StorageStatus::kFlashError;
	}
}

StorageStatus validate_access(
	StorageRegion region, uint32_t offset, size_t size, bool require_write_protection) {
	size_t size_limit = region_size(region);
	if (size == 0) {
		return StorageStatus::kInvalidArgument;
	}
	if (offset >= size_limit) {
		return StorageStatus::kOutOfBounds;
	}
	if (size > size_limit || static_cast<size_t>(offset) + size > size_limit) {
		return StorageStatus::kTooLarge;
	}
	if (require_write_protection && !kAllowUnprotectedLayout && !is_layout_protected()) {
		return StorageStatus::kUnprotectedLayout;
	}
	return StorageStatus::kOk;
}

}  // namespace

bool is_layout_protected() {
	uintptr_t flash_binary_end_address =
		reinterpret_cast<uintptr_t>(&__flash_binary_end);
	if (flash_binary_end_address < XIP_BASE) {
		return false;
	}

	uint32_t flash_binary_end_offset =
		static_cast<uint32_t>(flash_binary_end_address - XIP_BASE);
	return flash_binary_end_offset <= layout::kAppDataRegionOffsetBytes;
}

uint32_t region_offset(StorageRegion region) {
	switch (region) {
		case StorageRegion::kAppData:
			return layout::kAppDataRegionOffsetBytes;
		case StorageRegion::kCalibration:
			return layout::kCalibrationRegionOffsetBytes;
		default:
			return layout::kAppDataRegionOffsetBytes;
	}
}

size_t region_size(StorageRegion region) {
	switch (region) {
		case StorageRegion::kAppData:
			return layout::kAppDataRegionSizeBytes;
		case StorageRegion::kCalibration:
			return layout::kCalibrationRegionSizeBytes;
		default:
			return 0;
	}
}

StorageStatus read_region(
	StorageRegion region, uint32_t offset, void* out, size_t size) {
	if (!out) {
		return StorageStatus::kInvalidArgument;
	}

	StorageStatus validation = validate_access(region, offset, size, false);
	if (validation != StorageStatus::kOk) {
		return validation;
	}

	const uintptr_t source_address =
		XIP_BASE + region_offset(region) + offset;
	std::memcpy(out, reinterpret_cast<const void*>(source_address), size);
	return StorageStatus::kOk;
}

StorageStatus write_region(
	StorageRegion region, uint32_t offset, const void* data, size_t size) {
	if (!data) {
		return StorageStatus::kInvalidArgument;
	}

	StorageStatus validation = validate_access(region, offset, size, true);
	if (validation != StorageStatus::kOk) {
		return validation;
	}

	const uint32_t region_offset_bytes = region_offset(region);
	const size_t region_size_bytes = region_size(region);

	uint8_t sector_buffer[layout::kFlashSectorSizeBytes];
	std::memcpy(
		sector_buffer,
		reinterpret_cast<const void*>(XIP_BASE + region_offset_bytes),
		region_size_bytes);

	std::memcpy(sector_buffer + offset, data, size);

	FlashProgramRequest request{
		.offset_bytes = region_offset_bytes,
		.data = sector_buffer,
		.size = region_size_bytes,
	};

	int flash_result = flash_safe_execute(
		flash_program_sector_callback, &request, 500);
	return map_flash_safe_execute_result(flash_result);
}

StorageStatus erase_region(StorageRegion region) {
	if (!kAllowUnprotectedLayout && !is_layout_protected()) {
		return StorageStatus::kUnprotectedLayout;
	}

	uint8_t blank_sector[layout::kFlashSectorSizeBytes];
	std::memset(blank_sector, 0xFF, sizeof(blank_sector));

	FlashProgramRequest request{
		.offset_bytes = region_offset(region),
		.data = blank_sector,
		.size = region_size(region),
	};

	int flash_result = flash_safe_execute(
		flash_program_sector_callback, &request, 500);
	return map_flash_safe_execute_result(flash_result);
}

}  // namespace brain::storage
