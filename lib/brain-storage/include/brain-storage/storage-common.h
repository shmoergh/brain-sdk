#pragma once

#include <hardware/flash.h>

#include <cstdint>

namespace brain::storage {

namespace layout {

constexpr uint32_t kFlashSectorSizeBytes = FLASH_SECTOR_SIZE;
constexpr uint32_t kFlashPageSizeBytes = FLASH_PAGE_SIZE;

static_assert(kFlashSectorSizeBytes == 4096,
	"Brain storage assumes 4KB flash erase sectors.");
static_assert(kFlashPageSizeBytes == 256,
	"Brain storage assumes 256B flash program pages.");
static_assert((kFlashSectorSizeBytes % kFlashPageSizeBytes) == 0,
	"Brain storage requires sector size to be a multiple of page size.");

constexpr uint32_t kCalibrationRegionSizeBytes = kFlashSectorSizeBytes;
constexpr uint32_t kAppDataRegionSizeBytes = kFlashSectorSizeBytes;
constexpr uint32_t kReservedRegionSizeBytes =
	kCalibrationRegionSizeBytes + kAppDataRegionSizeBytes;

static_assert((kCalibrationRegionSizeBytes % kFlashSectorSizeBytes) == 0,
	"Calibration region must be sector aligned.");
static_assert((kAppDataRegionSizeBytes % kFlashSectorSizeBytes) == 0,
	"App-data region must be sector aligned.");

constexpr uint32_t kCalibrationRegionOffsetBytes =
	PICO_FLASH_SIZE_BYTES - kCalibrationRegionSizeBytes;
constexpr uint32_t kAppDataRegionOffsetBytes =
	PICO_FLASH_SIZE_BYTES - kReservedRegionSizeBytes;

}  // namespace layout

enum class StorageStatus : uint8_t {
	kOk = 0,
	kInvalidArgument,
	kNotFound,
	kCorrupt,
	kOutOfBounds,
	kTooLarge,
	kUnprotectedLayout,
	kFlashError,
	kTimeout,
	kNotPermitted,
};

}  // namespace brain::storage
