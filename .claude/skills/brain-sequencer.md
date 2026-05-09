---
name: brain-sequencer
description: Use when creating, editing, or extending a Shmøergh Brain sequencer or MIDI tool firmware — step sequencers, arpeggiators, MIDI-to-CV converters, MIDI utilities, clock generators, gate/trigger generators, or any firmware whose primary job is reacting to MIDI or producing timed CV/gate events. Triggers when the user mentions MIDI input, pitch CV, gate, trigger, clock, BPM, steps, arpeggiator, or asks to add MIDI handling to a firmware. Do not use for audio-rate DSP (use brain-audio-effect) or for free-running CV utilities like LFO/ADSR (use brain-cv-utility).
---

# Brain sequencer / MIDI tool firmware

Use this skill for firmwares whose hot loop is **event-driven** (MIDI bytes, clock ticks, button presses) rather than audio-rate. Timing precision matters but sample-rate latency does not.

## Up-front questions (ask before writing code)

1. **Output range per channel.** Sequencers commonly use:
   - **OUT A = unipolar (0..10 V)** for pitch CV (1V/oct → ~10 octaves of range)
   - **OUT B = unipolar** for gate/envelope, or **bipolar** for modulation
   Always confirm with the user — Brain has only 2 analog outs and the choice depends on what the firmware drives.

2. **Pulse output usage?** The dedicated digital pulse output (GPIO via `Outputs::pulse_set(bool)`) is typically used for clock/gate. Ask if the firmware needs it, and at what rate (e.g. 24 PPQN MIDI clock, 4 PPQN gate, etc.).

3. **MIDI features.** "Does the firmware need: note in / note out / clock in / clock out / CC handling / SysEx?" Drives which MIDI parser callbacks to wire up.

4. **Internal or external clock?** Drives whether to set up a hardware timer (`add_repeating_timer_us`) for internal tempo or to slave timing to incoming MIDI clock messages.

## Hard rules

### No `float` / `double` for tempo / clock / step math

Tempo conversions, swing offsets, gate-length calculations — all integer-only. Use microseconds (`uint64_t` from `to_us_since_boot`) or sample counters, not floating-point seconds.

```cpp
// 120 BPM = 500000 us per quarter note
const uint32_t us_per_quarter = 60000000UL / bpm;        // integer divide is fine here
const uint32_t us_per_step    = us_per_quarter / 4;       // 16th notes
```

### Pitch CV via the calibrated voltage API

For 1V/oct pitch output use `set_voltage_calibrated_millivolts(...)`, not the raw DAC API. This honors the calibration table that the user (or factory) wrote to flash.

```cpp
// MIDI note → millivolts (1V/oct, C0 = 0 mV typically)
const int32_t mv = (note - kBaseNote) * 1000 / 12;  // 1000 mV per octave / 12 semitones
brain.outputs.set_voltage_calibrated_millivolts(kOutputsChannelA, mv);
```

For non-pitch CV (mod, velocity, aftertouch shaped) `set_voltage_millivolts(...)` is fine.

### Don't block in the main loop

Sequencer firmwares typically have multiple time-driven things going on (clock, step advance, MIDI input, UI). Use `to_us_since_boot(get_absolute_time())` deadlines or `pico/time.h` repeating timers, not `sleep_ms`. Keep `brain.update()` running every iteration so MIDI parsing and pots/buttons stay live.

### Preserve CV calibration

Two things, both required: (1) `brain_storage_configure_flash_reservation()` in `CMakeLists.txt` between `project()` and `pico_sdk_init()`, (2) `brain.outputs.load_calibration_from_flash()` after `brain.init_all()`. Pitch CV must use `set_voltage_calibrated_millivolts(...)` (the API table above already does this). See the `brain-calibration` skill and `docs/PRESERVING_CV_CALIBRATION.md`.

## Skeleton (MIDI-to-CV-style, internal clock)

```cpp
#define BRAIN_USE_LEDS 1
#define BRAIN_USE_BUTTONS 1
#define BRAIN_USE_STORAGE 1
#define BRAIN_USE_OUTPUTS 1
#define BRAIN_USE_POTS 1
#define BRAIN_USE_MIDI_PARSER 1
#define BRAIN_USE_MIDI_TO_CV 1
#include "brain/brain.h"

int main() {
    Brain brain;
    brain.init_all();

    // Output ranges (set BEFORE any audio claim; sequencers don't typically use AudioProcessor)
    brain.outputs.set_output_range(kOutputsChannelA, kOutputsRange0To10V);   // pitch
    brain.outputs.set_output_range(kOutputsChannelB, kOutputsRange0To10V);   // gate / mod

    MidiToCvConfig mt_cfg{};
    // ...configure note-priority, glide, channel, etc.
    brain.init_midi_to_cv(mt_cfg);

    while (true) {
        brain.update();   // pumps MIDI parser, pots scan, button debounce, midi-to-cv
    }
}
```

For a step sequencer (no MIDI in), drop `MIDI_PARSER` / `MIDI_TO_CV` and add a repeating timer for step advance:

```cpp
static bool step_tick_cb(repeating_timer_t* /*t*/) {
    g_advance_step = true;
    return true;
}

// in main, after brain.init_all():
repeating_timer_t timer;
add_repeating_timer_us(-static_cast<int>(us_per_step), step_tick_cb, nullptr, &timer);
```

Then in the main loop, when `g_advance_step` is set, advance the step, write the pitch via `set_voltage_calibrated_millivolts`, and pulse the gate.

## Gate / trigger output

Two options:

- **Dedicated pulse out** — `brain.outputs.pulse_set(true)` / `pulse_set(false)`. This is the digital GPIO; clean, fast, no DAC bandwidth used.
- **OUT B as gate** — write a unipolar high (e.g. 5000 mV) for gate-on, 0 for gate-off via `set_voltage_millivolts(kOutputsChannelB, 5000)`. Use this when you want adjustable gate level.

Triggers (short pulses) — set the output high, schedule a deadline (e.g. `gate_off_at = now + 5_ms`), and clear it when the deadline passes in the main loop. Don't `sleep_ms` to time the trigger — that blocks MIDI/UI.

## Feature flag dependencies

From `brain/brain.h` `static_assert`s:
- `BRAIN_USE_OUTPUTS` ⇒ `BRAIN_USE_STORAGE`
- `BRAIN_USE_MIDI_TO_CV` ⇒ `BRAIN_USE_OUTPUTS` AND `BRAIN_USE_MIDI_PARSER`
- `BRAIN_USE_POT_MULTI_FUNCTION` ⇒ `BRAIN_USE_POTS`

Easiest path: `BRAIN_USE_ALL=1` and let the linker drop unused code. Selective flags are a code-size optimization, not a correctness one.

## References

- `brain/include/midi-parser.h` — MIDI byte-stream parser, callback hooks.
- `brain/include/midi-to-cv.h` — pre-built MIDI-to-CV with note priority, glide, pitch bend.
- `brain/include/outputs.h` — `set_voltage_calibrated_millivolts`, `pulse_set`, output ranges.
- `examples/apps/midi_parser_example.cpp` — raw MIDI parsing.
- `examples/apps/midi_to_cv_example.cpp` — full MIDI-to-CV setup.
- `docs/PRESERVING_CV_CALIBRATION.md`.
