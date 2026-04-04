# Storage

## Overview
Brain storage reserves flash sectors for:
- calibration data
- app blob data

Public APIs are now in the global namespace (no `brain::storage` qualifier).

## Include
```cpp
#include "brain/include/storage.h"
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

## API Groups
Low level:
- `is_layout_protected()`
- `read_region(...)`
- `write_region(...)`
- `erase_region(...)`

Calibration:
- `read_cv_calibration(...)`
- `write_cv_calibration(...)`
- `clear_cv_calibration()`

App blob:
- `read_app_blob(...)`
- `write_app_blob(...)`
- `clear_app_blob()`

## Calibration Payload
```cpp
struct CvCalibrationV1 {
	int16_t a_offset_lsb[10];
	int16_t b_offset_lsb[10];
};
```

This format is unchanged and remains compatible with existing saved calibration data.
