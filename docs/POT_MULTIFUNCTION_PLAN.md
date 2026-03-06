# Pot Multi-Function Helper (UI) — Research + Implementation Plan

## Goal
Create a Brain SDK helper in `brain-ui` that lets one physical pot drive multiple logical functions (based on modifier/button context) without value jumps when switching context.

Example:
- Pot X -> velocity (default)
- Pot X + Button A -> tempo
- Pot X + Button B -> scale

## Industry patterns found

### 1) Pick-up / Soft Takeover
Common in DAWs and controller mappings.
Behavior: parameter does not move until the physical control reaches/crosses the stored parameter value.

References:
- Ableton Live manual, Takeover Mode (`None`, `Pick-Up`, `Value Scaling`)
  - https://www.ableton.com/en/manual/midi-and-key-remote-control/
- Deluge firmware `midi_takeover.cpp` (`PICKUP`, crossing detection)
  - https://raw.githubusercontent.com/SynthstromAudible/DelugeFirmware/main/src/deluge/io/midi/midi_takeover.cpp

### 2) Value Scaling / Dynamic convergence
Also common in DAWs.
Behavior: while control and parameter are mismatched, movement is scaled so both converge smoothly; once synced, tracking is 1:1.

References:
- Ableton Live manual (Value Scaling)
  - https://www.ableton.com/en/manual/midi-and-key-remote-control/
- Deluge firmware `midi_takeover.cpp` (`SCALE` mode with directional runway math)
  - https://raw.githubusercontent.com/SynthstromAudible/DelugeFirmware/main/src/deluge/io/midi/midi_takeover.cpp

### 3) Relative encoder mode (for completeness)
Not the target for Brain pots (absolute ADC), but conceptually related and useful as fallback.

Reference:
- Deluge firmware (`RELATIVE`/`JUMP`/`PICKUP`/`SCALE`)
  - same file as above

## Proposed SDK API (C++11, embedded-safe)

Namespace: `brain::ui`

### Core types

- `enum class PotMode : uint8_t`
  - `kDirect`       // direct map from pot value
  - `kPickup`       // algorithm 1
  - `kValueScale`   // algorithm 2

- `struct PotFunctionConfig`
  - `uint8_t function_id`
  - `uint8_t pot_index`                // which physical pot (0..num_pots-1)
  - `int32_t min_value`
  - `int32_t max_value`
  - `int32_t initial_value`
  - `PotMode mode`
  - `uint8_t pickup_hysteresis`        // small deadband (e.g. 1-2 steps)

- `class PotMultiFunction`
  - `void init(uint8_t max_functions)`
  - `bool register_function(const PotFunctionConfig& cfg)`
  - `void set_active_function(uint8_t pot_index, uint8_t function_id)`
  - `void set_active_functions(const uint8_t* per_pot_function_ids, uint8_t count)`
  - `void update(const Pots& pots)`
  - `int32_t get_value(uint8_t function_id) const`
  - `bool get_changed(uint8_t function_id) const`
  - `void clear_changed_flags()`

## Internal model

Each logical function stores:
- persisted parameter value
- last raw pot sample seen while active
- sync state (`picked_up` for pickup)
- scaler state for value scaling:
  - anchor raw/value at gesture start
  - active direction (`-1 / +1 / 0`)
  - frozen slope numerator/denominator per direction segment

No heap allocation in hot path; use fixed-size arrays configured at init.

## Algorithm details

## Algorithm 1 (kPickup)
- On context switch to a function:
  - function enters `picked_up = false` unless raw pot already near stored value.
- On each update:
  - if not picked up: check crossing/meeting condition vs previous raw sample and target value (with hysteresis).
  - when picked: enable direct 1:1 mapping.
- Prevents jumps; user must meet current value first.

## Algorithm 2 (kValueScale)
- On context switch to a function:
  - capture `raw_anchor` and `value_anchor`.
  - reset directional scaler state.
- On movement:
  - detect direction from raw delta.
  - for current direction, compute remaining raw runway and value runway.
  - derive slope (value_delta/raw_delta) and keep it stable while direction stays same.
  - apply integer/fixed-point accumulation.
- On direction change:
  - re-anchor at current raw/value and recompute slope for new direction.

This solves the “constantly changing upper/lower ranges” problem by freezing resolution for each continuous movement direction.

## Answers to open questions from the spec

1) "Ranges change continuously while turning — what should happen?"
- Use **directional anchoring**:
  - keep slope constant while turning in same direction,
  - re-anchor only when direction changes.
- User feel: predictable local resolution, no jittery remapping.

2) "Pot at min/max but function value not at min/max"
- In the blocked direction, delta is zero (can’t go further).
- In the opposite direction, immediately compute a valid slope from new runway and continue smoothly.

3) "Pot and function are opposite extremes"
- Full travel maps full remaining value runway in chosen direction.
- No jump on activation; first movement applies scaled convergence.

## Numeric strategy (RP2040-safe)
- Integer math in hot path.
- For non-integer slopes in value scaling, use fixed-point accumulator (Q15 or Q16.16).
- Clamp final values to `[min_value, max_value]`.

## Integration pattern for firmware authors

1. Register all logical functions once in setup.
2. In update loop, resolve active function per pot from button/modifier state.
3. Call `set_active_function(...)` then `update(pots)`.
4. Read `get_value(function_id)` and apply to DSP/control.

## Phased implementation plan

### Phase 1 — Skeleton + direct mode
- Add class/header/source in `lib/brain-ui`.
- Function registry + context switching + value storage.
- `kDirect` behavior.

### Phase 2 — Pickup mode
- Add crossing detection + hysteresis.
- Add tests for switch-then-turn behavior.

### Phase 3 — ValueScale mode
- Add directional anchoring + fixed-point slope.
- Add tests for monotonicity, no jump, edge extremes.

### Phase 4 — Docs + examples
- Add docs page + minimal example snippet.
- Add migration notes for existing firmwares.

## Test plan (host-side)
Create deterministic tests under `sandbox`:
- registration/context routing
- pickup lock/unlock/crossing behavior
- value-scale convergence in both directions
- edge cases: min/max anchors, opposite extremes, repeated context switches

## Naming note
Use generic naming in SDK/docs (`Pickup`, `ValueScale`, `Takeover`) with no product/vendor references.
