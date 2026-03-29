# Storage + Calibration Release Notes

## Summary
This release adds SDK-managed persistent flash storage with calibration protection and calibrated CV output APIs.

Primary goal achieved:
- calibration now behaves as a device-level property for SDK-based UF2 workflows.

## Added Modules and APIs

### `brain::storage`
- reserved-sector flash layout management
- bounded region read/write/erase APIs
- calibration persistence APIs:
  - `read_cv_calibration(...)`
  - `write_cv_calibration(...)`
  - `clear_cv_calibration()`
- app blob APIs:
  - `read_app_blob(...)`
  - `write_app_blob(...)`
  - `clear_app_blob()`
- linker protection check:
  - `is_layout_protected()`

### `brain::io::AudioCvOut`
Added non-breaking calibrated output APIs:
- `set_voltage_calibrated(...)`
- `set_calibration(...)`
- `clear_calibration()`
- `load_calibration_from_flash()`
- `has_calibration()`

Existing `set_voltage(...)` behavior remains unchanged.

### `brain::utils::MidiToCV`
Added opt-in calibrated path:
- `enable_calibrated_output(...)`
- `disable_calibrated_output()`
- `set_cv_calibration(...)`
- `is_calibrated_output_enabled()`

Default behavior remains unchanged unless calibrated output is explicitly enabled.

## Flash Layout

### RP2040
- 8KB reserved total
- app blob sector + calibration sector

### RP2350
- 12KB reserved total
- app blob sector + calibration sector + top guard sector
- guard sector protects calibration from RP2350 UF2 absolute-block behavior

## Validation and Safety
- record validation uses magic/version/payload-size/CRC
- invalid records return safe status (`kNotFound` / `kCorrupt`)
- writes are blocked by default when linker reservation is missing (`kUnprotectedLayout`)
- unsafe override remains explicit and opt-in:
  - `BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=ON`

## Migration Requirements
Existing external firmware repos must include flash reservation before `pico_sdk_init()`:

```cmake
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()
```

If repo tooling still points to an older `brain-sdk` submodule revision, update submodule commit first.

## Hardware Validation Status
- RP2350 storage/calibration persistence flow validated across UF2 firmware updates.
- RP2040 and RP2350 build matrix validated for `test` and `sandbox`.
