#include "storage.h"

#include <hardware/flash.h>
#include <pico/flash.h>
#include <pico/platform.h>

#include <cstdint>
#include <cstring>

extern "C" uint8_t __flash_binary_end;

namespace {

constexpr uint32_t kCalibrationMagic = 0x4C414342;	// "BCAL"
constexpr uint32_t kCalibrationVersion = 1;
constexpr uint32_t kAppBlobMagic = 0x50415042;		// "BPAP"
constexpr uint32_t kAppBlobVersion = 1;

struct CalibrationRecordV1 {
	uint32_t magic;
	uint32_t version;
	uint32_t payload_size;
	CvCalibrationV1 payload;
	uint32_t crc32;
};

struct AppBlobRecordHeaderV1 {
	uint32_t magic;
	uint32_t version;
	uint32_t payload_size;
};

static_assert(sizeof(CvCalibrationV1) == 40,
	"CvCalibrationV1 must remain 40 bytes for storage format compatibility.");
static_assert(sizeof(CalibrationRecordV1) < StorageLayout::kCalibrationRegionSizeBytes,
	"Calibration record must fit in one calibration sector.");
static_assert(sizeof(AppBlobRecordHeaderV1) + sizeof(uint32_t) <
	StorageLayout::kAppDataRegionSizeBytes,
	"App blob record framing must fit in one app-data sector.");

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

uint32_t crc32_calculate(const uint8_t* data, size_t length) {
	uint32_t crc = 0xFFFFFFFFu;
	for (size_t i = 0; i < length; i++) {
		crc ^= data[i];
		for (int bit = 0; bit < 8; bit++) {
			uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1u)));
			crc = (crc >> 1u) ^ (0xEDB88320u & mask);
		}
	}
	return ~crc;
}

constexpr size_t app_blob_max_payload_size() {
	return StorageLayout::kAppDataRegionSizeBytes
		- sizeof(AppBlobRecordHeaderV1)
		- sizeof(uint32_t);
}

bool layout_protected_impl() {
	uintptr_t flash_binary_end_address = reinterpret_cast<uintptr_t>(&__flash_binary_end);
	if (flash_binary_end_address < XIP_BASE) {
		return false;
	}

	uint32_t flash_binary_end_offset = static_cast<uint32_t>(flash_binary_end_address - XIP_BASE);
	return flash_binary_end_offset <= StorageLayout::kAppDataRegionOffsetBytes;
}

uint32_t region_offset_impl(StorageRegion region) {
	switch (region) {
		case StorageRegion::kAppData:
			return StorageLayout::kAppDataRegionOffsetBytes;
		case StorageRegion::kCalibration:
			return StorageLayout::kCalibrationRegionOffsetBytes;
		default:
			return StorageLayout::kAppDataRegionOffsetBytes;
	}
}

size_t region_size_impl(StorageRegion region) {
	switch (region) {
		case StorageRegion::kAppData:
			return StorageLayout::kAppDataRegionSizeBytes;
		case StorageRegion::kCalibration:
			return StorageLayout::kCalibrationRegionSizeBytes;
		default:
			return 0;
	}
}

StorageStatus validate_access(
	StorageRegion region,
	uint32_t offset,
	size_t size,
	bool require_write_protection,
	bool require_protected_layout) {
	size_t size_limit = region_size_impl(region);
	if (size == 0) {
		return StorageStatus::kInvalidArgument;
	}
	if (offset >= size_limit) {
		return StorageStatus::kOutOfBounds;
	}
	if (size > size_limit || static_cast<size_t>(offset) + size > size_limit) {
		return StorageStatus::kTooLarge;
	}

	const bool unprotected_layout = !layout_protected_impl();
	if (require_protected_layout && unprotected_layout) {
		return StorageStatus::kUnprotectedLayout;
	}
	if (require_write_protection && !kAllowUnprotectedLayout && unprotected_layout) {
		return StorageStatus::kUnprotectedLayout;
	}

	return StorageStatus::kOk;
}

StorageStatus read_region_impl(
	StorageRegion region,
	uint32_t offset,
	void* out,
	size_t size,
	bool require_protected_layout) {
	if (!out) {
		return StorageStatus::kInvalidArgument;
	}

	StorageStatus validation = validate_access(region, offset, size, false, require_protected_layout);
	if (validation != StorageStatus::kOk) {
		return validation;
	}

	const uintptr_t source_address = XIP_BASE + region_offset_impl(region) + offset;
	std::memcpy(out, reinterpret_cast<const void*>(source_address), size);
	return StorageStatus::kOk;
}

StorageStatus write_region_impl(
	StorageRegion region,
	uint32_t offset,
	const void* data,
	size_t size,
	bool require_protected_layout) {
	if (!data) {
		return StorageStatus::kInvalidArgument;
	}

	StorageStatus validation = validate_access(region, offset, size, true, require_protected_layout);
	if (validation != StorageStatus::kOk) {
		return validation;
	}

	const uint32_t region_offset_bytes = region_offset_impl(region);
	const size_t region_size_bytes = region_size_impl(region);

	uint8_t sector_buffer[StorageLayout::kFlashSectorSizeBytes];
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

	const int flash_result = flash_safe_execute(flash_program_sector_callback, &request, 500);
	return map_flash_safe_execute_result(flash_result);
}

StorageStatus erase_region_impl(StorageRegion region, bool require_protected_layout) {
	const bool unprotected_layout = !layout_protected_impl();
	if ((require_protected_layout || !kAllowUnprotectedLayout) && unprotected_layout) {
		return StorageStatus::kUnprotectedLayout;
	}

	uint8_t blank_sector[StorageLayout::kFlashSectorSizeBytes];
	std::memset(blank_sector, 0xFF, sizeof(blank_sector));

	FlashProgramRequest request{
		.offset_bytes = region_offset_impl(region),
		.data = blank_sector,
		.size = region_size_impl(region),
	};

	const int flash_result = flash_safe_execute(flash_program_sector_callback, &request, 500);
	return map_flash_safe_execute_result(flash_result);
}

}  // namespace

bool Storage::init(bool require_protected_layout) {
	if (initialized_) {
		if (require_protected_layout) {
			require_protected_layout_ = true;
		}
		if (require_protected_layout_ && !layout_protected_impl()) {
			return false;
		}
		return true;
	}

	require_protected_layout_ = require_protected_layout;
	if (require_protected_layout_ && !layout_protected_impl()) {
		return false;
	}
	initialized_ = true;
	return true;
}

bool Storage::is_initialized() const {
	return initialized_;
}

bool Storage::is_layout_protected() const {
	return layout_protected_impl();
}

uint32_t Storage::region_offset(StorageRegion region) const {
	return region_offset_impl(region);
}

size_t Storage::region_size(StorageRegion region) const {
	return region_size_impl(region);
}

StorageStatus Storage::check_ready_(bool write_operation) const {
	(void)write_operation;
	if (!initialized_) {
		return StorageStatus::kNotPermitted;
	}

	if (!require_protected_layout_) {
		return StorageStatus::kOk;
	}

	if (!layout_protected_impl()) {
		return StorageStatus::kUnprotectedLayout;
	}

	return StorageStatus::kOk;
}

StorageStatus Storage::read_region(StorageRegion region, uint32_t offset, void* out, size_t size) const {
	StorageStatus ready = check_ready_(false);
	if (ready != StorageStatus::kOk) {
		return ready;
	}

	return read_region_impl(region, offset, out, size, require_protected_layout_);
}

StorageStatus Storage::write_region(
	StorageRegion region,
	uint32_t offset,
	const void* data,
	size_t size) const {
	StorageStatus ready = check_ready_(true);
	if (ready != StorageStatus::kOk) {
		return ready;
	}

	return write_region_impl(region, offset, data, size, require_protected_layout_);
}

StorageStatus Storage::erase_region(StorageRegion region) const {
	StorageStatus ready = check_ready_(true);
	if (ready != StorageStatus::kOk) {
		return ready;
	}

	return erase_region_impl(region, require_protected_layout_);
}

StorageStatus Storage::read_cv_calibration(CvCalibrationV1* out) const {
	StorageStatus ready = check_ready_(false);
	if (ready != StorageStatus::kOk) {
		return ready;
	}
	if (!out) {
		return StorageStatus::kInvalidArgument;
	}

	CalibrationRecordV1 record{};
	StorageStatus read_status =
		read_region_impl(StorageRegion::kCalibration, 0, &record, sizeof(record), require_protected_layout_);
	if (read_status != StorageStatus::kOk) {
		return read_status;
	}

	if (record.magic == 0xFFFFFFFFu) {
		return StorageStatus::kNotFound;
	}
	if (record.magic != kCalibrationMagic) {
		return StorageStatus::kCorrupt;
	}
	if (record.version != kCalibrationVersion) {
		return StorageStatus::kCorrupt;
	}
	if (record.payload_size != sizeof(CvCalibrationV1)) {
		return StorageStatus::kCorrupt;
	}

	const uint32_t computed_crc = crc32_calculate(
		reinterpret_cast<const uint8_t*>(&record),
		sizeof(record) - sizeof(record.crc32));
	if (computed_crc != record.crc32) {
		return StorageStatus::kCorrupt;
	}

	*out = record.payload;
	return StorageStatus::kOk;
}

StorageStatus Storage::write_cv_calibration(const CvCalibrationV1* in) const {
	StorageStatus ready = check_ready_(true);
	if (ready != StorageStatus::kOk) {
		return ready;
	}
	if (!in) {
		return StorageStatus::kInvalidArgument;
	}

	CalibrationRecordV1 record{};
	record.magic = kCalibrationMagic;
	record.version = kCalibrationVersion;
	record.payload_size = sizeof(CvCalibrationV1);
	record.payload = *in;
	record.crc32 = crc32_calculate(
		reinterpret_cast<const uint8_t*>(&record),
		sizeof(record) - sizeof(record.crc32));

	return write_region_impl(
		StorageRegion::kCalibration,
		0,
		&record,
		sizeof(record),
		require_protected_layout_);
}

StorageStatus Storage::clear_cv_calibration() const {
	StorageStatus ready = check_ready_(true);
	if (ready != StorageStatus::kOk) {
		return ready;
	}
	return erase_region_impl(StorageRegion::kCalibration, require_protected_layout_);
}

StorageStatus Storage::read_app_blob(void* out, size_t max_size, size_t* actual_size) const {
	StorageStatus ready = check_ready_(false);
	if (ready != StorageStatus::kOk) {
		return ready;
	}
	if (!actual_size) {
		return StorageStatus::kInvalidArgument;
	}
	*actual_size = 0;

	uint8_t sector_buffer[StorageLayout::kAppDataRegionSizeBytes];
	StorageStatus read_status = read_region_impl(
		StorageRegion::kAppData,
		0,
		sector_buffer,
		sizeof(sector_buffer),
		require_protected_layout_);
	if (read_status != StorageStatus::kOk) {
		return read_status;
	}

	AppBlobRecordHeaderV1 header{};
	std::memcpy(&header, sector_buffer, sizeof(header));

	if (header.magic == 0xFFFFFFFFu) {
		return StorageStatus::kNotFound;
	}
	if (header.magic != kAppBlobMagic) {
		return StorageStatus::kCorrupt;
	}
	if (header.version != kAppBlobVersion) {
		return StorageStatus::kCorrupt;
	}
	if (header.payload_size > app_blob_max_payload_size()) {
		return StorageStatus::kCorrupt;
	}

	const size_t crc_offset = sizeof(header) + header.payload_size;
	uint32_t stored_crc = 0;
	std::memcpy(&stored_crc, sector_buffer + crc_offset, sizeof(stored_crc));

	const uint32_t computed_crc = crc32_calculate(sector_buffer, sizeof(header) + header.payload_size);
	if (stored_crc != computed_crc) {
		return StorageStatus::kCorrupt;
	}

	if (header.payload_size > max_size) {
		return StorageStatus::kTooLarge;
	}
	if (header.payload_size > 0 && !out) {
		return StorageStatus::kInvalidArgument;
	}

	if (header.payload_size > 0) {
		std::memcpy(out, sector_buffer + sizeof(header), header.payload_size);
	}
	*actual_size = header.payload_size;
	return StorageStatus::kOk;
}

StorageStatus Storage::write_app_blob(const void* data, size_t size) const {
	StorageStatus ready = check_ready_(true);
	if (ready != StorageStatus::kOk) {
		return ready;
	}
	if (!data) {
		return StorageStatus::kInvalidArgument;
	}
	if (size > app_blob_max_payload_size()) {
		return StorageStatus::kTooLarge;
	}

	uint8_t sector_buffer[StorageLayout::kAppDataRegionSizeBytes];
	std::memset(sector_buffer, 0xFF, sizeof(sector_buffer));

	AppBlobRecordHeaderV1 header{};
	header.magic = kAppBlobMagic;
	header.version = kAppBlobVersion;
	header.payload_size = static_cast<uint32_t>(size);
	std::memcpy(sector_buffer, &header, sizeof(header));
	std::memcpy(sector_buffer + sizeof(header), data, size);

	const uint32_t crc = crc32_calculate(sector_buffer, sizeof(header) + size);
	std::memcpy(sector_buffer + sizeof(header) + size, &crc, sizeof(crc));

	return write_region_impl(
		StorageRegion::kAppData,
		0,
		sector_buffer,
		sizeof(sector_buffer),
		require_protected_layout_);
}

StorageStatus Storage::clear_app_blob() const {
	StorageStatus ready = check_ready_(true);
	if (ready != StorageStatus::kOk) {
		return ready;
	}
	return erase_region_impl(StorageRegion::kAppData, require_protected_layout_);
}
