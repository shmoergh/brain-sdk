# Storage

## Overview
Brain storage reserves flash sectors for:
- calibration data
- app blob data

Storage is exposed as a component class (`Storage`) and via `Brain` as `brain.storage`.

## Include
```cpp
#include "brain/include/storage.h"
```

With `Brain`:
```cpp
#define BRAIN_USE_STORAGE 1
#include "brain/brain.h"
```

## Quick Start
```cpp
Storage storage;
if (!storage.init(true)) {
	// storage layout is not protected
}

CvCalibrationV1 cal{};
StorageStatus status = storage.read_cv_calibration(&cal);
```

## Layout Constants
Layout constants live in `StorageLayout`:
- `kAppDataRegionOffsetBytes`
- `kAppDataRegionSizeBytes`
- `kCalibrationRegionOffsetBytes`
- `kCalibrationRegionSizeBytes`

## CMake Reservation
Call before `pico_sdk_init()`:
```cmake
include(cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()
```

## Storage Class API
Lifecycle:
- `bool init(bool require_protected_layout = false)`
- `bool is_initialized() const`

Low level:
- `bool is_layout_protected() const`
- `uint32_t region_offset(StorageRegion) const`
- `size_t region_size(StorageRegion) const`
- `StorageStatus read_region(...) const`
- `StorageStatus write_region(...) const`
- `StorageStatus erase_region(...) const`

Calibration:
- `StorageStatus read_cv_calibration(...) const`
- `StorageStatus write_cv_calibration(...) const`
- `StorageStatus clear_cv_calibration() const`

App blob:
- `StorageStatus read_app_blob(...) const`
- `StorageStatus write_app_blob(...) const`
- `StorageStatus clear_app_blob() const`

## Calibration Payload
```cpp
struct CvCalibrationV1 {
	int16_t a_offset_lsb[10];
	int16_t b_offset_lsb[10];
};
```

This format is unchanged and remains compatible with existing saved calibration data.
