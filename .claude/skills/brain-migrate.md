---
name: brain-migrate
description: Use when the user is upgrading an existing firmware to a newer Brain SDK version — phrases like "migrate this firmware to Brain SDK 2.x", "I just bumped the brain-sdk submodule and now things don't compile", "what changes do I need for the new SDK", or any time a firmware is on an older SDK version and the user wants help moving it forward. Walks the firmware through the documented breaking changes for each version step.
---

# Migrate a Brain firmware to a newer SDK version

Use this skill when an existing firmware needs to be brought forward to a newer Brain SDK release. Most of the work is mechanical — find old API usages, replace with new API, follow the migration doc — but the diagnosis ("which version are we on, which version are we going to, what actually broke") is what makes it worth a skill.

## Step 1 — figure out where the firmware is starting from

Inspect the firmware repo (CWD) and the brain-sdk submodule:

```bash
git -C brain-sdk describe --tags --always
git -C brain-sdk log --oneline -5
```

Heuristics for **detecting the firmware's current SDK era** by reading its source:

- **Pre-2.0 (1.x)** — `#include "brain-common/...`, `#include "brain-io/...`, `#include "brain-ui/...`, namespaces like `brain::io::AudioCvOut`. Float voltage API (`set_voltage(float)`).
- **2.0** — `#include "brain/brain.h"`, `BRAIN_USE_*` flags, integer millivolt API, `kBrainInitStatus*`. Code that explicitly handles `init_inputs()` / `init_pots()` failing when `AudioProcessor` is initialized (or vice versa). Code referencing `AudioProcessorConfig::max_dma_drain_samples_per_tick` or `spi_baud_hz` as if they matter. Code reading `OutputEngineSnapshot::audio_frame_a/b` or `audio_overflow_*` / `audio_underrun_*` counters. Code setting `PotsConfig::channel_map` to anything but identity.
- **2.1** — Coexistence of AudioProcessor + Inputs + Pots without try/fail handling. Possibly using `init_audio_processor_v2`.

Tell the user: "Looks like this firmware is on **\<era\>**; the SDK submodule is at **\<version\>**. I'll walk the migration step by step."

## Step 2 — read the migration doc(s) the user actually needs

Migration docs live in the SDK at:

- `brain-sdk/docs/2.0_MIGRATION.md` — 1.x → 2.0 (the big breaking refactor: include paths, namespaces removed, float→mV, Pulse split into Inputs/Outputs, etc.).
- `brain-sdk/docs/2.1_MIGRATION.md` — 2.0 → 2.1 (mostly source-compatible — coexistence guard removal, ignored-field deprecations, optional v2 stereo audio API).

Read the **target** migration doc end-to-end before patching. The skill should not improvise — the docs are the source of truth.

If the firmware skips a version (1.x → 2.1 directly), do 2.0 first (substantive), then 2.1 (mostly cleanup).

## Step 3 — apply the migration

### 1.x → 2.0 — substantive breaking changes

This is the big one. Walk through, in this order:

1. **Include paths.** Replace every `#include "brain-common/...` / `brain-io/...` / `brain-ui/...` / `brain-utils/...` / `brain-storage/...` with the corresponding `brain/include/...` or `brain/brain.h`. The full table is in `docs/2.0_MIGRATION.md` "Include Path Mapping".
2. **Namespaces removed.** Strip `brain::io::`, `brain::ui::`, etc. from type usage.
3. **`Pulse` split.** `Pulse` no longer exists. Input pulses → `Inputs`, output pulses → `Outputs::pulse_set/init_pulse`.
4. **Voltage API: float → millivolts.** Every `set_voltage(volts_float)` becomes `set_voltage_millivolts(channel, mv_int32)` or `set_voltage_calibrated_millivolts(...)` for pitch.
5. **Output coupling: AC/DC → explicit range.** Replace AC/DC coupling calls with `set_output_range(channel, kOutputsRange0To10V | kOutputsRangeMinus5To5V)`.
6. **`Led` + `ButtonLed` → `Leds`.** Single `Leds` class manages all panel LEDs.
7. **Init returns status.** `init_*` calls now return `BrainInitStatus` — wrap with `brain_init_succeeded(...)` for the OK-or-already-initialized check.
8. **`MidiToCV` dependency injection.** Use `brain.init_midi_to_cv(cfg)` (the wrapper handles dependency wiring) instead of constructing it directly.
9. **Feature flags.** Remove any `BrainT<>` / preset alias usage; add `#define BRAIN_USE_ALL 1` (or selective `BRAIN_USE_*`) before `#include "brain/brain.h"`.
10. **CMake.** Update `CMakeLists.txt` to include the storage flash reservation (`brain-storage-reserve-flash.cmake`) — calibration preservation is now the default.

For each change: grep the firmware, show the user a sample diff, apply, repeat. Many can be batched with `sed`/`Edit` `replace_all` once the pattern is confirmed.

### 2.0 → 2.1 — mostly cleanup

Per `docs/2.1_MIGRATION.md`, most firmwares need **zero code changes**. The migration is mostly *deletions*:

1. **Remove conflict-handling branches.** Any code that handles `init_inputs()` / `init_pots()` / `init_audio_processor()` failing because of the other being initialized — those branches are now dead. Recommend deleting; safe to leave if defensively retained.
2. **Delete `max_dma_drain_samples_per_tick` and `spi_baud_hz`** from `AudioProcessorConfig` initializers (still compile, but ignored — the deprecation warning will surface them).
3. **Delete `PotsConfig::channel_map`** if non-default — it carries `[[deprecated]]` and is ignored.
4. **Don't read `OutputEngineSnapshot::audio_frame_a/b` or `audio_overflow_*/audio_underrun_*`** — fields are gone. Most firmware doesn't touch these.
5. **`adc-arbiter.h` is gone.** Anyone including it (rare) needs to remove the include.
6. **Optional:** consider switching to `init_audio_processor_v2` if the firmware would benefit from stereo (IN1+IN2 → OUT A+OUT B in one callback). Not required.

## Step 4 — verify

After patching:

1. **Build** at the firmware's normal target. Watch for `[[deprecated]]` warnings — these are migration breadcrumbs.
2. **Confirm calibration is still preserved.** Hand off to the `brain-calibration` skill for the full audit, or at minimum check: (a) `brain_storage_configure_flash_reservation()` is still called between `project()` and `pico_sdk_init()` in `CMakeLists.txt`; (b) `cmake -S . -B build` prints the `[brain-storage] Reserved ... bytes at top-of-flash.` line; (c) `brain.outputs.load_calibration_from_flash()` is called after `brain.init_all()` in `main()`. The 1.x → 2.0 migration is a particularly easy place to drop one of these by accident.
3. **Boot on hardware** and run through the firmware's manual test checklist (or the SDK's `test/` apps if relevant). Power-cycle the board and confirm calibration survives.

## Cross-cutting reminders

- The "no `float` in hot loops" rule applies during migration too — if the old firmware used floats in audio/CV callbacks, this is the moment to switch to fixed-point. Don't carry forward float DSP just because it compiles.
- Output ranges may have been implicit pre-2.0 (AC/DC coupling). Explicitly set the desired range with `set_output_range(...)` for each channel during migration; ask the user what each channel should be if not obvious from context.

## References

- `docs/2.0_MIGRATION.md` — full 1.x → 2.0 mapping (include paths, API changes, examples).
- `docs/2.1_MIGRATION.md` — 2.0 → 2.1 cleanup and stereo API.
- `CHANGELOG.md` — version-by-version summary.
- `brain/brain.h` — current feature flag + init API surface.
- `AGENTS.md` — note: §4 ownership rules describe pre-2.1 behaviour, kept for historical reference.
