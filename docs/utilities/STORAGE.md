# Storage Utility

`Storage` provides persistent data storage for Brain firmware, persisting data in flash memory. It is intended to:

- save app-level data blobs (settings, presets, state)
- save CV calibration payloads
- low-level region read/write/erase operations when needed

## Storage model
Storage is split into reserved regions, exposed through `StorageRegion`:

- `StorageRegion::kAppData` (app blob)
- `StorageRegion::kCalibration`

Most apps should use the high-level helpers (`read_app_blob`, `write_app_blob`, calibration helpers) instead of accessing the raw regions.

## Example

This example shows a simple persistence workflow using `Storage` through the `Brain` wrapper: initialize storage once, write a small settings struct as an app blob, then read it back and check the returned size/status. It demonstrates the intended high-level usage where your firmware treats storage as a typed state container rather than doing raw flash operations directly. The key point is that read/write calls return status codes, so the app can handle failures explicitly instead of assuming persistence always succeeds.


```cpp
#define BRAIN_USE_STORAGE 1
#include "brain/brain.h"

#include <pico/stdlib.h>
#include <stdio.h>

Brain brain;

// Define what you want to save
struct AppSettings {
	uint32_t magic;
	uint8_t mode;
	uint8_t brightness;
};

int main() {
	stdio_init_all();

	BrainInitStatus status = brain.init_storage();
	if (!brain_init_succeeded(status)) return 1;

	AppSettings settings{0x42524149u, 2, 120};
	StorageStatus wr = brain.storage.write_app_blob(&settings, sizeof(settings));
	(void)wr;

	AppSettings loaded{};
	size_t actual_size = 0;
	StorageStatus rd = brain.storage.read_app_blob(&loaded, sizeof(loaded), &actual_size);
	(void)rd;
	(void)actual_size;

	while (true) {
		sleep_ms(100);
	}
}
```

### Using `magic`

`magic` is a fixed marker value you write into stored data, then check after reading it back.

Basic flow:

1. Choose a constant, e.g. `0x42524149`.
2. When saving settings, always write that value into `settings.magic`.
3. When loading settings, verify `loaded.magic == expected_magic`.
4. If it doesn’t match, treat data as invalid/old/uninitialized and fall back to defaults.

Why it helps:
- Detects random/erased/corrupted data.
- Detects layout/version mismatch when struct format changes.

Typical pattern also checks size:

```cpp
if (status != StorageStatus::kOk ||
	actual_size != sizeof(Settings) ||
	loaded.magic != kSettingsMagic) {
	loaded = default_settings();
}
```

So `magic` is just a quick “is this really my settings blob?” guard.


## Storage API

### Initialization and readiness
- `bool init(bool require_protected_layout = false)`
  Initializes storage subsystem and validates layout/availability.
- `bool is_initialized() const`
  Returns whether storage has been initialized.
- `bool is_layout_protected() const`
  Returns whether protected flash layout is active.

### Region metadata/introspection
- `uint32_t region_offset(StorageRegion region) const`
  Returns byte offset of a region.
- `size_t region_size(StorageRegion region) const`
  Returns capacity of a region in bytes.

### Generic region I/O
- `StorageStatus read_region(StorageRegion region, uint32_t offset, void* out, size_t size) const`
  Reads raw bytes from region.
- `StorageStatus write_region(StorageRegion region, uint32_t offset, const void* data, size_t size) const`
  Writes raw bytes into region.
- `StorageStatus erase_region(StorageRegion region) const`
  Erases entire region.

### Calibration helpers
- `StorageStatus read_cv_calibration(CvCalibrationV1* out) const`
  Reads calibration payload.
- `StorageStatus write_cv_calibration(const CvCalibrationV1* in) const`
  Writes calibration payload.
- `StorageStatus clear_cv_calibration() const`
  Clears calibration data.

### App-blob helpers
- `StorageStatus read_app_blob(void* out, size_t max_size, size_t* actual_size) const`
  Reads app blob payload, returns actual size.
- `StorageStatus write_app_blob(const void* data, size_t size) const`
  Writes app blob payload.
- `StorageStatus clear_app_blob() const`
  Clears app blob payload.


## Low-level region I/O example
```cpp
#include "brain/include/storage.h"

Storage storage;

int main() {
	if (!storage.init(true)) return 1;

	uint8_t bytes[16] = {1,2,3,4};
	StorageStatus wr = storage.write_region(StorageRegion::kAppData, 0, bytes, sizeof(bytes));
	(void)wr;

	uint8_t out[16] = {};
	StorageStatus rd = storage.read_region(StorageRegion::kAppData, 0, out, sizeof(out));
	(void)rd;
}
```

## Calibration payload example
```cpp
#include "brain/include/storage.h"

Storage storage;

int main() {
	if (!storage.init(true)) return 1;

	CvCalibrationV1 cal{};
	for (int i = 0; i < 10; ++i) {
		cal.a_offset_lsb[i] = 0;
		cal.b_offset_lsb[i] = 0;
	}

	storage.write_cv_calibration(&cal);

	CvCalibrationV1 loaded{};
	storage.read_cv_calibration(&loaded);
}
```

## Status/result types used by the API

- `StorageRegion`
  Region selector (`kAppData`, `kCalibration`).

- `CvCalibrationV1`
  Calibration payload structure (`a_offset_lsb[10]`, `b_offset_lsb[10]`).

- `StorageStatus`
  Return status type for storage operations (defined in `storage-common.h`).

## API usage notes
- Always check returned `StorageStatus` from read/write/erase calls.
- If your firmware uses Brain SDK as a dependency, ensure flash reservation setup is present in CMake before `pico_sdk_init()`.
- Use high-level blob/calibration helpers unless you specifically need raw region access.