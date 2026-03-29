# Storage (`brain::storage`)

## Overview
`brain::storage` is the SDK-owned flash storage layer for persistent module data.

- Calibration data is device-level and should survive SDK-based UF2 updates.
- App blob data is firmware-level and isolated from calibration.
- Storage writes are blocked by default when linker flash reservation is not active.

## Flash Layout
RP2040 layout (8KB reserved):

- App blob sector: `FLASH_END - 8192 .. FLASH_END - 4097`
- Calibration sector: `FLASH_END - 4096 .. FLASH_END - 1`

RP2350 layout (12KB reserved):

- App blob sector: `FLASH_END - 12288 .. FLASH_END - 8193`
- Calibration sector: `FLASH_END - 8192 .. FLASH_END - 4097`
- Guard sector (unused by SDK data): `FLASH_END - 4096 .. FLASH_END - 1`

The RP2350 guard sector protects calibration from top-of-flash UF2 absolute-block writes.

Constants are available in `brain::storage::layout`:

- `kAppDataRegionOffsetBytes`
- `kAppDataRegionSizeBytes`
- `kCalibrationRegionOffsetBytes`
- `kCalibrationRegionSizeBytes`

## CMake / Linker Reservation
Enable reservation before `pico_sdk_init()`:

```cmake
include(cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()
```

Supported options:

- `BRAIN_STORAGE_ENABLE_FLASH_RESERVATION` (default `ON`)
- `BRAIN_STORAGE_RESERVED_FLASH_BYTES` (default `8192` on RP2040, `12288` on RP2350)
- `BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT` (default `OFF`)

If reservation is disabled and unsafe override is off, write/erase APIs return `kUnprotectedLayout`.

## Public APIs
Low-level region helpers:

- `read_region(...)`
- `write_region(...)`
- `erase_region(...)`
- `is_layout_protected()`

Calibration APIs:

- `read_cv_calibration(CvCalibrationV1* out)`
- `write_cv_calibration(const CvCalibrationV1* in)`
- `clear_cv_calibration()`

App blob APIs:

- `read_app_blob(void* out, size_t max_size, size_t* actual_size)`
- `write_app_blob(const void* data, size_t size)`
- `clear_app_blob()`

## Record Validation
Calibration and app blob records are versioned and CRC-protected:

- `magic`
- `version`
- `payload_size`
- payload
- `crc32`

Read behavior:

- erased/uninitialized record: `kNotFound`
- invalid magic/version/size/CRC: `kCorrupt`
- never crashes on invalid flash content

## Calibration Firmware Contract
Calibration payload type:

```cpp
struct CvCalibrationV1 {
	int16_t a_offset_lsb[10];
	int16_t b_offset_lsb[10];
};
```

Units and indexing:

- Values are signed DAC-LSB offsets.
- `a_offset_lsb[i]` is channel A offset at `(i + 1)V`.
- `b_offset_lsb[i]` is channel B offset at `(i + 1)V`.
- Index `0` maps to `1V`; index `9` maps to `10V`.
- `0V` anchor offset is always treated as `0` by calibrated output interpolation.

Expected write flow for external calibration firmware:

1. Fill a `CvCalibrationV1` payload in DAC-LSB units.
2. Call `write_cv_calibration(...)`.
3. Optionally verify with `read_cv_calibration(...)`.

Do not write raw flash directly from calibration firmware; use SDK APIs.

## App Blob Semantics
- Single global blob in one sector.
- Isolated from calibration sector by design.
- Persistence across firmware updates is allowed but not a hard product guarantee.

## Safety Notes
- Calibration safety depends on linker reservation being active in the flashed firmware.
- Flashing firmware that does not reserve sectors (or bypasses SDK APIs) can overwrite calibration.
