# Storage Baseline (Historical)

This file documents pre-storage behavior from older SDK revisions.

## Historical Notes (Before Storage APIs)
- No reserved flash sectors managed by SDK.
- Calibration persistence was not centrally guaranteed.
- Output API was float-based (`set_voltage(...)`) in older revisions.

## Current State
- Storage APIs are available in `brain/include/storage.h`.
- Output API uses millivolts:
  - `set_voltage_millivolts(...)`
  - `set_voltage_calibrated_millivolts(...)`
- Calibration payload format is still `CvCalibrationV1` (compatible).
