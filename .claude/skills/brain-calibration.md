---
name: brain-calibration
description: Use whenever a change in a Shmøergh Brain firmware could affect CV-output calibration preservation — editing CMakeLists.txt (especially around project(), pico_sdk_init(), or flash reservation), editing Storage init / configuration, calling or removing outputs.load_calibration_from_flash(), choosing between Outputs::set_voltage_millivolts and set_voltage_calibrated_millivolts, removing BRAIN_USE_STORAGE, touching the BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT or BRAIN_STORAGE_ENABLE_FLASH_RESERVATION flags, or any time the user mentions calibration, flash layout, calibration sector, UF2 size, or the brain-cv-tuner. Also fire defensively when the user is about to remove storage code, swap to a non-calibrated output API, or simplify CMake in ways that would touch the reservation. Do NOT fire on unrelated firmware edits (DSP code, MIDI handling, LFO shapes) where calibration is not at risk.
---

# Brain calibration preservation

Every Brain firmware must preserve the per-board CV calibration that lives in the reserved sector at the top of the Pico's flash. Calibration is hardware-specific, written once via the [Brain CV tuner](https://github.com/shmoergh/brain-cv-tuner/) on a multimeter rig, and must survive every subsequent firmware flash for the life of the board. **Losing it means re-running the entire calibration procedure.** There is no warning at runtime when it gets clobbered — the data is just gone.

Use this skill when reviewing or making changes that could break that contract.

## The two non-negotiable requirements

A firmware preserves calibration if and only if **both** of these are true:

### 1. Build-time: flash reservation in `CMakeLists.txt`

```cmake
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()
```

The call **must** sit between `project(...)` and `pico_sdk_init()`. Before `project()` it has nothing to bind to; after `pico_sdk_init()` the linker script is already written and the call is a no-op.

Verify it's working: a clean `cmake -S . -B build` run should print

```
-- [brain-storage] Reserved 12288 bytes at top-of-flash. Linker flash length set to ((4 * 1024 * 1024)) - (12288).
```

(8192 bytes on RP2040, 12288 on RP2350.) **If that line is missing, the reservation is broken** — the firmware image can grow into the calibration sector on the next flash.

### 2. Runtime: load calibration after init

```cpp
brain.init_all();
brain.outputs.load_calibration_from_flash();   // <-- required
```

Without this call, calibration data sits unread in flash — outputs work, but with raw DAC scaling, not calibrated mV. Add it once, immediately after `init_all()` (or after `init_outputs()` if not using `init_all`).

## The output API contract

| API | When to use | Calibration applied? |
|---|---|---|
| `outputs.set_voltage_calibrated_millivolts(ch, mv)` | Normal use — pitch CV, anything tracking absolute voltage | **Yes** |
| `outputs.set_voltage_millivolts(ch, mv)` | Hardware tests, trimmer adjustment, raw DAC writes | No |

Pitch outputs (1V/oct) and any output that has to track absolute voltage **must** use the calibrated API. Modulation, LFO, gate, and audio outs can use either — but defaulting to the calibrated API costs nothing once `load_calibration_from_flash()` has been called.

## Things that quietly destroy calibration

Watch for these — refuse to apply them or warn loudly when the user proposes them:

- **`cmake -DBRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=ON`** — disables the reservation. Exists for ad-hoc test builds only. Never use it for distributed firmware.
- **`-DBRAIN_STORAGE_ENABLE_FLASH_RESERVATION=OFF`** — same outcome, different flag. Same prohibition.
- **Removing `brain_storage_configure_flash_reservation()` from CMakeLists.txt** — usually proposed as "simplifying CMake." Don't.
- **Moving the helper call before `project()` or after `pico_sdk_init()`** — the call still compiles but does nothing. The cmake configure log will silently lack the reservation line.
- **Removing `BRAIN_USE_STORAGE` from the firmware's feature flags** — this also disables `BRAIN_USE_OUTPUTS` (it depends on storage) and removes the load path. If the user wants to drop storage, ask why; usually they want a calibration-free *test* firmware, in which case fine, but flag it explicitly.
- **Calling `write_cv_calibration` or `clear_cv_calibration` from application firmware** — those are owned by `brain-cv-tuner`. App firmware reads calibration; it does not write or clear it.

## When skipping calibration is OK

The only legitimate reason to skip calibration preservation is a **calibration-free test or diagnostic firmware** that the user explicitly wants to keep separate from production. For those:

- Fine to set `-DBRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=ON` for the local build.
- Make sure the resulting UF2 is clearly named (e.g. `brain-mytest-NOCAL.uf2`) so it can't be accidentally distributed.
- Document that the firmware will overwrite calibration if flashed onto a calibrated board.

For *every* firmware that gets distributed — keep the defaults. Both flags at their defaults: `BRAIN_STORAGE_ENABLE_FLASH_RESERVATION=ON`, `BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=OFF`.

## Audit checklist (run when reviewing any Brain firmware)

1. Open `CMakeLists.txt`. Confirm `brain_storage_configure_flash_reservation()` is present and sits between `project(...)` and `pico_sdk_init()`. ✅
2. Run `cmake -S . -B build` (or check the most recent configure log). Confirm the `[brain-storage] Reserved ... bytes at top-of-flash.` line appears. ✅
3. Open `main.cpp` (or the equivalent entry point). Confirm `outputs.load_calibration_from_flash()` is called after `brain.init_all()` (or after `init_outputs()`). ✅
4. Grep the firmware for `set_voltage_millivolts` (without `_calibrated_`). Each hit should be intentional — hardware test, trimmer adjust, or a non-pitch output where raw is fine. Ask the user if a hit looks suspicious. ✅
5. Grep for `write_cv_calibration` and `clear_cv_calibration`. There should be **zero** hits in application firmware. ✅
6. Grep `CMakeLists.txt` and any build scripts for `BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT` and `BRAIN_STORAGE_ENABLE_FLASH_RESERVATION`. Any non-default value needs a clear justification. ✅

## Reservation sizes (for context, not for the user to compute)

| Platform | Reserved bytes | Sectors |
|---|---|---|
| RP2040 (Pico 1) | 8192 | 2 × 4 KB (app blob + calibration) |
| RP2350 (Pico 2) | 12288 | 3 × 4 KB (app blob + calibration + guard) |

The RP2350's extra 4 KB guard sector exists because the UF2 absolute-block format on RP2350 can write into the top sector during firmware updates; the guard absorbs that hit so it can't reach calibration.

## References

- `docs/PRESERVING_CV_CALIBRATION.md` — full doc (CMake order details, escape-hatch flags, verification line).
- `cmake/brain-storage-reserve-flash.cmake` — the helper itself.
- `brain/include/outputs.h` — `load_calibration_from_flash`, `set_voltage_calibrated_millivolts`, `set_voltage_millivolts`.
- `AGENTS.md` §11 — "Firmware default rule: preserve CV calibration."
- [`brain-cv-tuner`](https://github.com/shmoergh/brain-cv-tuner/) — the only firmware that writes/clears calibration.
