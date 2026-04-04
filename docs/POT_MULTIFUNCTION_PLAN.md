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
- `update(Pots&)`
- `update_buffered(Pots&, bool perform_scan = true)`
- `get_value(function_id)`
- `get_changed(function_id)`
- `clear_changed_flags()`

## Modes
- `PotMode::kDirect`
- `PotMode::kPickup`
- `PotMode::kValueScale`

## Recommended Runtime Pattern
1. `pots.scan()` once per frame (or use `update_buffered(..., true)`)
2. route active function IDs from button/context state
3. run `PotMultiFunction` update
4. consume logical values with `get_value(...)`
