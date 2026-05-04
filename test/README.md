# Test Apps (`test`)

Purpose: reusable manual hardware regression apps for Brain SDK components.

The `test` firmware is now a single **Critical Issues Test Package**:
- flash once
- open serial terminal
- select a test from the startup menu in `test/main.cpp`

After a test app starts, it owns the loop. Reset the board to return to the startup menu.

## Available Test Apps
- `LedsTest` (`apps/leds_test.cpp`)
- `MidiToCvTest` (`apps/midi_to_cv_test.cpp`)
- `AudioProcessorTest` (`apps/audio_processor_test.cpp`)
- `MultipotTest` (`apps/multipot_test.cpp`)
- `PotReadStabilityTest` (`apps/pot_read_stability_test.cpp`)
- `StorageTest` (`apps/storage_test.cpp`)
- `StoragePersistenceCheckTest` (`apps/storage_persistence_check_test.cpp`)
- `PotsStorageStressTest` (`apps/pots_storage_stress_test.cpp`)
- `OutputsStorageStressTest` (`apps/outputs_storage_stress_test.cpp`)

## Critical Issues Coverage

- Pot mux bleed / unstable pot reads:
  - `PotReadStabilityTest`
- Multi-function pot mode behavior:
  - `MultipotTest`
- MIDI input + CV output integration:
  - `MidiToCvTest`
- AudioProcessor ISR loop + pot mux + guardrails:
  - `AudioProcessorTest`
- Flash storage correctness + persistence:
  - `StorageTest`, `StoragePersistenceCheckTest`
- Flash writes while real-time IO engines are active:
  - `PotsStorageStressTest`, `OutputsStorageStressTest`
- UI LED behavior regressions:
  - `LedsTest`

## Build

```bash
cmake --build build --target test -j4
```

## Adding New Regression Apps

1. Add new app files in `test/apps/` (`*_test.h` + `*_test.cpp`).
2. Add source file to `test/CMakeLists.txt`.
3. Add app include + menu entry + switch case in `test/main.cpp`.
4. Add app to the list in this README.
5. Keep runtime UX consistent:
   - print clear startup info
   - print explicit hardware interaction instructions
   - report measurable result or explicit `PASS`/`FAIL` conditions
   - avoid hidden assumptions about board state

## Optional: Run One App Directly

If you want a dedicated firmware image for one app, temporarily instantiate it directly in `test/main.cpp` and rebuild:
```bash
cmake --build build --target test -j4
```
