# Storage Baseline (Phase 0)

This document captures the pre-storage behavior before introducing persistent
flash handling in the Brain SDK.

## Current `AudioCvOut` behavior

- API used today: `AudioCvOut::set_voltage(channel, voltage)`.
- Allowed range: `0.0V` to `10.0V`.
- Out-of-range values are rejected and return `false`.
- DAC conversion is linear: `0V -> 0`, `10V -> 4095`.
- No calibration table, interpolation, or persistent correction is applied.

## Current firmware flashing behavior

- Firmware is built as UF2 files and copied to the Pico BOOTSEL drive.
- No SDK-owned flash regions are currently reserved for persistent user data.
- SDK currently has no centralized flash storage API for calibration/app data.
- Persistence across firmware updates is therefore not guaranteed by SDK.

## Phase 0 constants decided

- Reserved flash size: `8KB` total.
- App-data region: `4KB` sector at `FLASH_END - 8192 .. FLASH_END - 4097`.
- Calibration region: `4KB` sector at `FLASH_END - 4096 .. FLASH_END - 1`.
- Flash assumptions: `FLASH_SECTOR_SIZE == 4096`, `FLASH_PAGE_SIZE == 256`.
