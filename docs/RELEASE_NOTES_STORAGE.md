# Storage + Calibration Release Notes

## Summary
Persistent flash storage and calibration support were added to the SDK and remain compatible with `CvCalibrationV1` saved on existing devices.

## Current API Names
Storage (global namespace):
- `read_cv_calibration(...)`
- `write_cv_calibration(...)`
- `clear_cv_calibration()`
- `read_app_blob(...)`
- `write_app_blob(...)`
- `clear_app_blob()`
- `is_layout_protected()`

Outputs calibrated path:
- `set_voltage_calibrated_millivolts(...)`
- `set_calibration(...)`
- `clear_calibration()`
- `load_calibration_from_flash()`
- `has_calibration()`

MidiToCV opt-in calibrated path:
- `enable_calibrated_output(...)`
- `disable_calibrated_output()`
- `set_cv_calibration(...)`
- `is_calibrated_output_enabled()`

## Migration Note
If a firmware repo was created before reservation support, add:
```cmake
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()
```
