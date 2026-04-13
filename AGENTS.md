# AGENTS.md

Guidance for coding/documentation agents working in `brain-sdk`.

## 1) Repo purpose and scope

This repository contains the Brain SDK for the Shmøergh Brain module firmware ecosystem.

Primary goals in this repo:
- Maintain reusable firmware components (`brain/include`, `brain/src`)
- Keep top-level `Brain` wrapper behavior coherent (`brain/brain.h`)
- Keep manual hardware regression apps working (`test/`)
- Keep quick experiment wrapper working (`sandbox/`)
- Keep docs aligned with code (`docs/`)

Out of scope unless explicitly requested:
- Broad changes inside `pico-sdk/` subtree
- Changing hardware assumptions without coordinated docs/tests updates

## 2) Important directories

- `brain/`
  - `brain.h`: top-level wrapper class and feature gating
  - `include/`: public SDK headers
  - `src/`: component implementations
- `docs/`: user-facing docs (installation, getting started, migration, component/utility docs)
- `test/`: manual hardware regression firmware and menu
- `sandbox/`: thin app wrapper for quick interactive testing
- `scripts/new-brain-app.sh`: scaffolds external firmware projects using this SDK
- `cmake/brain-storage-reserve-flash.cmake`: flash reservation helper for storage/calibration

## 3) Build and run basics

### Configure + build (default board/platform)
```bash
cmake -S . -B build
cmake --build build
```

### Build manual test firmware
```bash
cmake --build build --target test -j4
```

### Build sandbox firmware
```bash
cmake --build build --target sandbox -j4
```

### Board defaults
Top-level CMake defaults to:
- `PICO_BOARD=pico2`
- `PICO_PLATFORM=rp2350-arm-s`

Override at configure time for alternatives.

## 4) Brain wrapper contract (must preserve)

`brain/brain.h` enforces compile-time feature ownership and runtime init/update rules.

### Feature macro requirements
At least one of these must be defined before including `brain/brain.h`:
- `BRAIN_USE_ALL`
- or selective `BRAIN_USE_*` flags

### Compile-time dependency rules (static_assert)
- `BRAIN_USE_OUTPUTS` requires `BRAIN_USE_STORAGE`
- `BRAIN_USE_MIDI_TO_CV` requires `BRAIN_USE_OUTPUTS` and `BRAIN_USE_MIDI_PARSER`
- `BRAIN_USE_POT_MULTI_FUNCTION` requires `BRAIN_USE_POTS`

### Runtime init semantics
All `init_*` methods return `BrainInitStatus`:
- `kOk`
- `kAlreadyInitialized`
- `kFailed`

`brain_init_succeeded(status)` treats both `kOk` and `kAlreadyInitialized` as success.

### `init_all()` coverage
`init_all()` initializes core modules only:
- leds, buttons, storage, outputs, pots, inputs, midi_parser

Utilities remain explicit:
- `init_midi_to_cv(...)`
- `init_pot_multi(...)`
- `init_audio_processor(...)`

### Ownership guardrails (critical)
When audio processor is initialized, these must fail:
- `init_inputs()`
- `init_pots(...)` / `reconfigure_pots(...)`
- `init_pot_multi(...)`

If inputs/pots/pot_multi are already initialized, `init_audio_processor(...)` must fail.

Do not bypass these guardrails without explicit product-level decision.

## 5) ADC policy controls

`Brain` exposes policy toggles:
- `enable_adc_optimization(bool)` (master)
- `set_audio_cv_dma_enabled(bool)`
- `set_shared_pot_sampling_enabled(bool)`

Internally this propagates into:
- `Inputs::set_audio_cv_dma_enabled(...)`
- `Pots::set_optimized_sampling_enabled(...)`

Apply policy before relevant `init_*()` for deterministic behavior.

## 6) Component behavior notes worth preserving

- `Inputs` reports mV using Brain-specific front-end mapping constants (`common.h`), not naive full-scale ADC mapping.
- `Outputs` voltage API is millivolt-based (`set_voltage_millivolts`, `set_voltage_calibrated_millivolts`), with explicit per-channel output range.
- `Pots` has buffered and direct read paths; buffered path relies on `scan()`.
- `PotMultiFunction` depends on stable active-function mapping and update mode (`update_buffered` vs `update_single`).
- `MidiParser` and `MidiToCV` are loop-driven; if update/process isn’t called, behavior stalls.

## 7) Testing expectations

Tests in `test/` are manual hardware regression apps, not pure host unit tests.

If behavior changes in critical areas (pots stability, audio processor ownership, storage persistence, MIDI->CV path, LED behavior), update or verify the corresponding test app(s):
- `PotReadStabilityTest`
- `AudioProcessorTest`
- `StorageTest` / `StoragePersistenceCheckTest`
- `MidiToCvTest`
- `LedsTest`
- `MultipotTest`

## 8) Documentation expectations

When API behavior changes, update matching docs in `docs/` within the same change set.

Current docs entry points:
- `docs/README.md`
- `docs/INSTALLATION.md`
- `docs/GETTING_STARTED.md`
- `docs/2.0_MIGRATION.md`
- component docs under `docs/components/`
- utility docs under `docs/utilities/`

Keep examples aligned with real signatures from headers in `brain/include/` and wrapper methods in `brain/brain.h`.

## 9) Safe edit boundaries

Preferred targets for normal work:
- `brain/include/*`
- `brain/src/*`
- `brain/brain.h`
- `docs/*`
- `test/*`
- `sandbox/*`
- `scripts/*`

Avoid editing vendored third-party code (`pico-sdk/`) unless explicitly requested.

## 10) Practical contribution checklist

Before finalizing a change:
1. Confirm compile-time feature dependencies still hold.
2. Confirm runtime ownership guardrails still hold.
3. Confirm docs/examples use current API names and signatures.
4. Build at least affected targets (`brain` consumers, `test`/`sandbox` when relevant).
5. If behavior changed, describe migration impact clearly.

## 11) Firmware default rule: preserve CV calibration

For any new firmware/app/template in this repo, CV calibration preservation is the default behavior, not optional polish.

Required by default:
- Keep flash reservation for storage/calibration (`brain-storage-reserve-flash.cmake` path in CMake flow).
- Initialize `Storage` in protected-layout mode when using wrapper flows.
- Ensure CV output paths use/retain persisted calibration data (do not silently reset/ignore calibration on boot).

Only skip this if the task explicitly asks for a calibration-free firmware.
