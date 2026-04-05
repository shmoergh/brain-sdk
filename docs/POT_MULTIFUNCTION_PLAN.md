# Pot Multi-Function Notes

## Status
`PotMultiFunction` is implemented and available via:
- `brain/include/pot-multi-function-core.h`
- `brain/include/pots.h`

No `brain::ui` namespace is required for new code.

## Core API
- `register_function(const PotFunctionConfig&)`
- `set_active_function(...)`
- `set_active_functions(...)`
- `update(Pots&)` (buffered default path)
- `update_single(Pots&)` (explicit direct path)
- `update_buffered(Pots&, bool perform_scan = true)`
- `reset_for_mode_change(bool clear_active_mappings = true)`
- `get_value(function_id)`
- `get_changed(function_id)`
- `clear_changed_flags()`

## Modes
- `PotMode::kDirect`
- `PotMode::kPickup`
- `PotMode::kValueScale`

## Recommended Runtime Pattern
1. call `update(...)` each frame (safe buffered default), or use `update_buffered(...)` for explicit scan control
2. route active function IDs from button/context state
3. run `PotMultiFunction` update
4. consume logical values with `get_value(...)`
