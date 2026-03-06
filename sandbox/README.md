# UI Sandbox Firmware (`sandbox`)

Purpose: quick manual testbed for Brain SDK UI helpers.

Current focus:
- `PotMultiFunction` function switching with a statically selected mode

## Controls
- Pot X: source pot under test
- Pot Y: observed in logs only (not used for control)
- Button A (hold): Tempo function
- Button B (hold): Scale function
- No button: Velocity function
- Value mode: static constant in `apps/multipot_test.cpp` (`kValuePotMode`)

## LED feedback
- LED 1-3: configured mode
- LED 4-6: active function

## Serial output
Prints status at ~10Hz with mode/function/value and pot readings.

## Why this exists
This firmware is intended as a reusable sandbox for validating new UI/input helpers before using them in product firmware.
