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

// Convenience aliases for less verbose region selection.
constexpr StorageRegion kStorageAppData = StorageRegion::kAppData;
constexpr StorageRegion kStorageCalibration = StorageRegion::kCalibration;

struct CvCalibrationV1 {
	int16_t a_offset_lsb[10];
	int16_t b_offset_lsb[10];
};

class Storage {
public:
	/**
	 * @brief Initializes `Storage` access policy for Brain reserved flash sectors.
	 * @param require_protected_layout `true` requires firmware image to end before app-data/calibration regions.
	 * If protection check fails, initialization fails. `false` allows running even on unprotected layout
	 * (subject to compile-time `BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT` for writes).
	 * @return `true` when layout policy is accepted; `false` when required protection is not satisfied.
	 */
	bool init(bool require_protected_layout = false);

	/**
	 * @brief Reports whether `Storage` has been initialized.
	 * @return `true` after successful `init(...)`.
	 */
	bool is_initialized() const;

	/**
	 * @brief Reports whether current firmware layout leaves reserved storage sectors untouched.
	 * @return `true` when application binary ends before the app-data region.
	 */
	bool is_layout_protected() const;

	/**
	 * @brief Returns the byte offset of the selected storage region.
	 * @param region Storage region selector:
	 * - `kStorageAppData`
	 * - `kStorageCalibration`
	 * @return Flash offset (from XIP base) where the selected region starts.
	 */
	uint32_t region_offset(StorageRegion region) const;

	/**
	 * @brief Returns the byte size of the selected storage region.
	 * @param region Storage region selector (`kStorageAppData` or `kStorageCalibration`).
	 * @return Region size in bytes.
	 */
	size_t region_size(StorageRegion region) const;

	/**
	 * @brief Reads raw bytes from a storage region.
	 * @param region Region to read (`kStorageAppData` or `kStorageCalibration`).
	 * @param offset Byte offset inside the selected region.
	 * @param out Destination buffer for read data.
	 * @param size Number of bytes to read.
	 * @return `kStorageStatusOk` on success, otherwise a specific error such as
	 * `kStorageStatusNotPermitted`, `kStorageStatusInvalidArgument`, `kStorageStatusOutOfBounds`,
	 * `kStorageStatusTooLarge`, or `kStorageStatusUnprotectedLayout`.
	 */
	StorageStatus read_region(StorageRegion region, uint32_t offset, void* out, size_t size) const;

	/**
	 * @brief Writes raw bytes into a storage region (sector rewrite under the hood).
	 * @param region Region to write (`kStorageAppData` or `kStorageCalibration`).
	 * @param offset Byte offset inside the selected region.
	 * @param data Source bytes to write.
	 * @param size Number of bytes to write.
	 * @return `kStorageStatusOk` on success, or an error such as `kStorageStatusNotPermitted`,
	 * `kStorageStatusUnprotectedLayout`, `kStorageStatusTooLarge`, `kStorageStatusInvalidArgument`,
	 * `kStorageStatusFlashError`, or `kStorageStatusTimeout`.
	 */
	StorageStatus write_region(StorageRegion region, uint32_t offset, const void* data, size_t size) const;

	/**
	 * @brief Erases the selected storage region in flash.
	 * @param region Region to erase (`kStorageAppData` or `kStorageCalibration`).
	 * @return `kStorageStatusOk` on success or a flash/layout permission error status.
	 */
	StorageStatus erase_region(StorageRegion region) const;

	/**
	 * @brief Reads and validates the stored CV calibration record.
	 * @param out Destination struct for calibration payload.
	 * @return `kStorageStatusOk` on success, `kStorageStatusNotFound` when no record exists,
	 * `kStorageStatusCorrupt` on bad magic/version/CRC, or readiness/access errors.
	 */
	StorageStatus read_cv_calibration(CvCalibrationV1* out) const;

	/**
	 * @brief Writes CV calibration as a framed record with CRC.
	 * @param in Calibration payload to persist.
	 * @return `kStorageStatusOk` on success or an argument/access/flash error status.
	 */
	StorageStatus write_cv_calibration(const CvCalibrationV1* in) const;

	/**
	 * @brief Erases the full calibration region.
	 * @return `kStorageStatusOk` on success or an access/flash error status.
	 */
	StorageStatus clear_cv_calibration() const;

	/**
	 * @brief Reads application blob record from app-data region.
	 * @param out Destination buffer for blob payload (can be `nullptr` only when payload size is zero).
	 * @param max_size Size of caller buffer `out`.
	 * @param actual_size Output size of decoded payload.
	 * @return `kStorageStatusOk` on success, `kStorageStatusNotFound` when region is empty,
	 * `kStorageStatusCorrupt` on bad header/CRC, `kStorageStatusTooLarge` when payload exceeds `max_size`,
	 * or readiness/access errors.
	 */
	StorageStatus read_app_blob(void* out, size_t max_size, size_t* actual_size) const;

	/**
	 * @brief Writes application blob record (header + payload + CRC) into app-data region.
	 * @param data Pointer to payload bytes.
	 * @param size Payload size in bytes.
	 * @return `kStorageStatusOk` on success, `kStorageStatusTooLarge` if payload exceeds region capacity,
	 * or argument/access/flash error status.
	 */
	StorageStatus write_app_blob(const void* data, size_t size) const;

	/**
	 * @brief Erases the entire app-data region.
	 * @return `kStorageStatusOk` on success or an access/flash error status.
	 */
	StorageStatus clear_app_blob() const;

private:
	StorageStatus check_ready_(bool write_operation) const;

	bool initialized_ = false;
	bool require_protected_layout_ = false;
};
