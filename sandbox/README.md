# UI Sandbox Firmware (`sandbox`)

Purpose: quick manual testbed for Brain SDK UI helpers.

Current focus:
- `PotMultiFunction` function switching with a statically selected behavior

## Controls
- Pot X: source pot under test
- Pot Y: observed in logs only (not used for control)
- Button A (hold): Tempo function
- Button B (hold): Scale function
- No button: Velocity function
- Behavior mode: static constant in `apps/multipot_test.cpp` (`kValuePotBehavior`)

## LED feedback
- LED 1-3: configured behavior
- LED 4-6: active function

## Serial output
Prints status at ~10Hz with behavior/function/value and pot readings.

## Why this exists
This firmware is intended as a reusable sandbox for validating new UI/input helpers before using them in product firmware.
