# UI Sandbox Firmware (`sdk_test`)

Purpose: quick manual testbed for Brain SDK UI helpers.

Current focus:
- `PotMultiFunction` behavior testing
  - Direct
  - Pickup
  - ValueScale

## Controls
- Pot X: source pot under test
- Pot Y: behavior select
  - low: Direct
  - mid: Pickup
  - high: ValueScale
- Button A (hold): Tempo context
- Button B (hold): Scale context
- No button: Velocity context

## LED feedback
- LED 1-3: active behavior
- LED 4-6: active context

## Serial output
Prints status at ~10Hz with behavior/context/value and pot readings.

## Why this exists
This firmware is intended as a reusable sandbox for validating new UI/input helpers before using them in product firmware.
