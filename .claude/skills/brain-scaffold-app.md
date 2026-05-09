---
name: brain-scaffold-app
description: Use when the user wants to start a new Shmøergh Brain firmware from scratch — phrases like "create a new Brain app", "scaffold a new firmware", "start a new Brain project", or "I want to make a new <effect/sequencer/LFO/etc.>". Sets up the directory, brain-sdk submodule, CMake, main.cpp, and the initial output-range / feature-flag wiring. Then hands off to the matching archetype skill (brain-audio-effect, brain-sequencer, brain-cv-utility) for body code.
---

# Scaffold a new Brain firmware

Use this skill at the start of a new firmware. Its job is to gather the few decisions that determine the project shape, run the existing `scripts/new-brain-app.sh` scaffolder, then patch the generated files so they reflect those decisions before any DSP / domain code is written.

## Step 1 — gather decisions (ask the user, in order)

Ask **all four** questions before doing anything. Don't assume defaults silently.

1. **App name and location.**
   - "What's the app name?" (alphanumeric, hyphens, underscores)
   - "Where should it be created?" (default: alongside `brain-sdk`, i.e. `../<name>`)

2. **Archetype.** "What kind of firmware is this?"
   - **audio effect** — filter, distortion, delay, reverb, EQ, waveshaper, etc.
   - **sequencer / MIDI tool** — step sequencer, arpeggiator, MIDI-to-CV, MIDI utility
   - **CV utility** — LFO, ADSR, S&H, quantizer, slew, attenuverter
   - **blank** — none of the above; user wants a clean slate

3. **Output range per channel.** Brain has two analog outputs (A and B). For each one the user will use, ask: "Should OUT A be unipolar (0..10 V) or bipolar (−5..+5 V)?" — same for OUT B. Use these archetype defaults to pre-suggest, but always confirm:
   - audio effect: A=bipolar (audio claims this anyway), B=ask
   - sequencer: A=unipolar (pitch CV typically), B=ask (often unipolar for gate, bipolar for mod)
   - CV utility: ADSR=unipolar, LFO=bipolar, S&H/quantizer="inherit input"
   - blank: ask both

4. **Target board.** "RP2040 or RP2350 (Pico 2)?" Default to **RP2350** (matches SDK default `PICO_BOARD=pico2`, `PICO_PLATFORM=rp2350-arm-s`). Mention briefly: RP2040 has no FPU, RP2350 does — but in either case audio code should stay integer-only.

## Step 2 — run the SDK scaffolder

Use the existing script. It clones `brain-sdk` as a submodule, sets up CMake, and writes a basic `main.cpp` with `BRAIN_USE_ALL=1`:

```bash
./brain-sdk/scripts/new-brain-app.sh <app-name-or-path>
```

The script must be run from inside `brain-sdk/` (or anywhere — it resolves the SDK location from its own path). It is interactive and asks "Continue? (y/n)"; pass that prompt through to the user.

If `brain-sdk` is not yet a submodule of the workspace this skill is invoked from, run the scaffolder from the SDK checkout the user has — the script picks up the SDK's git remote automatically.

## Step 3 — install Claude skills into the new firmware

After the scaffolder finishes:

```bash
cd <new-app-dir>
./brain-sdk/scripts/install-claude-skills.sh
```

This symlinks every `brain-*` skill (including the archetype skills below) into `.claude/skills/` so the firmware project picks them up.

## Step 4 — patch `main.cpp` and `CMakeLists.txt` for the chosen archetype

The scaffolder writes a generic `main.cpp` with `#define BRAIN_USE_ALL 1`. Tighten it based on the archetype:

| Archetype | Suggested feature flags | Notes |
|---|---|---|
| audio effect | `BRAIN_USE_ALL` (simplest) — or selective: `STORAGE`, `OUTPUTS`, `POTS`, `AUDIO_PROCESSOR`, optional `LEDS`/`BUTTONS` | `AUDIO_PROCESSOR` requires `STORAGE` + `OUTPUTS` for calibration; `OUTPUTS` requires `STORAGE`. |
| sequencer / MIDI | `STORAGE`, `OUTPUTS`, `MIDI_PARSER`, `MIDI_TO_CV` (optional), `LEDS`, `BUTTONS`, `POTS` | `MIDI_TO_CV` requires `OUTPUTS` + `MIDI_PARSER`. |
| CV utility | `STORAGE`, `OUTPUTS`, `POTS`, `INPUTS` (for S&H/quantizer), `LEDS` | All CV firmwares should preserve calibration. |
| blank | leave as `BRAIN_USE_ALL` | User opts in/out later. |

For **board override**, edit the generated `CMakeLists.txt` to set `PICO_BOARD` and `PICO_PLATFORM` if the user picked RP2040:

```cmake
set(PICO_BOARD pico CACHE STRING "")
set(PICO_PLATFORM rp2040 CACHE STRING "")
```

For **output ranges**, insert calls early in `main()` after `init_all()` (or after `init_outputs()`):

```cpp
brain.outputs.set_output_range(kOutputsChannelA, kOutputsRange0To10V);   // unipolar
brain.outputs.set_output_range(kOutputsChannelB, kOutputsRangeMinus5To5V); // bipolar
```

Don't set the range on a channel that `AudioProcessor` will claim — it will be overridden to bipolar anyway. If the user *also* uses that channel for non-audio CV when audio is stopped, document the range-restore gotcha (see `brain-audio-effect`).

## Step 5 — hand off to the archetype skill

After scaffolding is complete, explicitly invoke the archetype skill:

- **audio effect** → `brain-audio-effect`
- **sequencer / MIDI** → `brain-sequencer`
- **CV utility** → `brain-cv-utility`
- **blank** → no handoff; the user will direct next steps.

The archetype skill carries the per-domain rules (no floats in audio, fixed-point patterns, MIDI clock handling, LFO LUT shapes, etc.).

## Cross-cutting reminders

- **No `float`/`double` in any hot loop.** This is a hard rule for every Brain firmware. RP2040 has no FPU; RP2350 has one but it's still costly. Use Q15/Q31 fixed-point and lookup tables. Float is OK in init code that runs once.
- **Preserve CV calibration by default** — keep the storage flash reservation and let `Storage` init in protected-layout mode. See `docs/PRESERVING_CV_CALIBRATION.md`. Only skip if the user explicitly asks for a calibration-free firmware.

## References

- `scripts/new-brain-app.sh` — what we wrap (do not reimplement scaffolding).
- `scripts/install-claude-skills.sh` — installs the archetype skills into the new firmware.
- `brain/brain.h` — feature flag dependency rules (compile-time `static_assert`s).
- `brain/include/outputs.h` — output range enum + `set_output_range`.
- `docs/GETTING_STARTED.md` — manual setup the script automates.
- `docs/PRESERVING_CV_CALIBRATION.md`.
