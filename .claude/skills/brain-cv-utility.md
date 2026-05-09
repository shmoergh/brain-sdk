---
name: brain-cv-utility
description: Use when creating, editing, or extending a Shmøergh Brain CV utility firmware — LFO, ADSR / envelope generator, sample-and-hold, slew limiter, attenuverter, quantizer, voltage source, or any firmware whose primary job is generating or shaping CV at control rate (sub-audio, typically 1–10 kHz update). Triggers when the user mentions LFO, envelope, ADSR, S&H, sample and hold, quantize, slew, or asks to add a CV generator to a firmware. Do not use for audio-rate DSP (use brain-audio-effect) or MIDI-driven firmwares (use brain-sequencer).
---

# Brain CV utility firmware

Use this skill for control-rate CV generators and shapers — anything that runs faster than UI but slower than audio. Typical update rate: 1 to 10 kHz from a hardware timer or a tight main loop.

## Up-front questions (ask before writing code)

1. **Output range per channel.** Defaults by sub-archetype (always confirm):
   - **ADSR / envelope:** unipolar (0..10 V). Most VCAs and filter cutoffs expect 0+.
   - **LFO:** bipolar (−5..+5 V). Modulation wants to swing both directions around the input.
   - **S&H / slew:** match the input range — ask the user what's coming in.
   - **Quantizer:** unipolar (0..10 V), 1V/oct, calibrated.
   - **Attenuverter / mixer:** match the inputs.

2. **Update rate.** Ask: "How fast does the CV need to update?" For LFOs at audio-adjacent rates (≥1 kHz) use a hardware timer. For envelopes a few hundred Hz is plenty. Quantizer can update on input change only.

3. **Number of voices/outputs.** Brain has 2 analog outs. Ask which is which (e.g. "OUT A = LFO 1, OUT B = LFO 2", or "OUT A = envelope, OUT B = inverted envelope").

## Hard rules

### No `float` / `double` in the generation loop

Use integer phase counters, lookup tables, and Q15/Q31 fixed-point. The reasons are the same as for audio (RP2040 no-FPU, RP2350 FPU expensive for transcendentals) but the consequence is different: at control rate floats might *work*, they'll just eat headroom that pots/UI need. Stay integer.

**LFO via lookup table** — canonical pattern:

```cpp
// 1024-entry signed sine LUT, range -32768..32767, computed once at init
static int16_t kSineLut[1024];

void init_sine_lut() {
    for (int i = 0; i < 1024; ++i) {
        kSineLut[i] = static_cast<int16_t>(32767.0f * sinf(2.0f * 3.14159265f * i / 1024.0f));
    }
    // floats here are fine — runs once at boot
}

// In the timer/loop, integer-only:
static uint32_t phase = 0;          // Q22.10: low 10 bits = sub-index, high = LUT index
static uint32_t increment = 0;      // set from rate pot

// each tick:
phase += increment;
const uint32_t idx = (phase >> 10) & 0x3FF;       // 0..1023
const int16_t s = kSineLut[idx];                  // -32768..32767
// Map to bipolar millivolts (-5000..+5000):
const int32_t mv = (static_cast<int32_t>(s) * 5000) >> 15;  // s/32768 * 5000
brain.outputs.set_voltage_millivolts(kOutputsChannelA, mv);
```

**ADSR via integer phase counter** — segment-based, accumulate `level` per tick by an integer step that you compute once when entering the segment. No `expf` per tick.

**Quantizer via integer LUT** — read input millivolts, divide by `1000/12` (≈83 mV per semitone), round to nearest integer semitone, multiply back, and write via `set_voltage_calibrated_millivolts`.

### Use the calibrated voltage API for pitch-relevant outputs

Quantizers, 1V/oct sources, anything that has to track musical intervals → `set_voltage_calibrated_millivolts(...)`. For LFO/ADSR/mod, raw `set_voltage_millivolts(...)` is fine.

### Preserve CV calibration

Two things, both required: (1) `brain_storage_configure_flash_reservation()` in `CMakeLists.txt` between `project()` and `pico_sdk_init()`, (2) `brain.outputs.load_calibration_from_flash()` after `brain.init_all()`. Quantizers and any 1V/oct source must use `set_voltage_calibrated_millivolts(...)`. See the `brain-calibration` skill and `docs/PRESERVING_CV_CALIBRATION.md`.

### Pots = parameters, not raw control voltages

Read pots via the SDK's pot path (buffered or direct), smooth them in software, and feed them into the parameter computation. Don't drive an output directly from a raw pot read every tick — pot ADC noise will show up as zipper noise in the CV.

## Skeleton (LFO with rate + shape)

```cpp
#define BRAIN_USE_LEDS 1
#define BRAIN_USE_BUTTONS 1
#define BRAIN_USE_STORAGE 1
#define BRAIN_USE_OUTPUTS 1
#define BRAIN_USE_POTS 1
#include "brain/brain.h"

#include "pico/time.h"

static int16_t g_sine_lut[1024];
static volatile uint32_t g_phase = 0;
static volatile uint32_t g_increment = 0;

static bool tick_cb(repeating_timer_t* /*t*/) {
    g_phase += g_increment;
    return true;
}

int main() {
    Brain brain;
    brain.init_all();

    brain.outputs.set_output_range(kOutputsChannelA, kOutputsRangeMinus5To5V);  // bipolar LFO

    // build LUT (init only — float here is fine)
    for (int i = 0; i < 1024; ++i) {
        g_sine_lut[i] = static_cast<int16_t>(32767.0f * sinf(2.0f * 3.14159265f * i / 1024.0f));
    }

    repeating_timer_t timer;
    add_repeating_timer_us(-200, tick_cb, nullptr, &timer);   // 5 kHz update

    uint32_t smoothed_rate_pot = 0;
    while (true) {
        brain.update();

        // smooth pot (one-pole, control-rate)
        const uint32_t raw = brain.pots.get_value(0);   // returns 0..4095 typically
        smoothed_rate_pot += (raw - smoothed_rate_pot) >> 4;

        // map pot to phase increment (integer; tune to your taste)
        g_increment = 1 + (smoothed_rate_pot << 4);

        // read phase (atomic on RP2040/2350 for aligned 32-bit) and write LFO
        const uint32_t phase = g_phase;
        const uint32_t idx = (phase >> 22) & 0x3FF;     // adjust shift to match increment scale
        const int16_t s = g_sine_lut[idx];
        const int32_t mv = (static_cast<int32_t>(s) * 5000) >> 15;
        brain.outputs.set_voltage_millivolts(kOutputsChannelA, mv);
    }
}
```

(Tune the bit shifts to your phase-accumulator format and target rate range — the structure is what matters.)

## ADSR notes

- Five states: idle / attack / decay / sustain / release.
- Each state has an integer "step per tick" computed once on state entry from the relevant pot. No per-tick float work.
- Trigger via a button (`brain.buttons`) or external input (`brain.inputs`) edge.
- Output as unipolar millivolts via `set_voltage_millivolts(kOutputsChannelA, level_mv)`.

## S&H notes

- Drive sampling from a clock — either the pulse input (read via `brain.inputs`) or an internal timer.
- On rising edge, sample `brain.inputs.read_millivolts(...)` and write that value to OUT.
- Output range = input range; ask the user.

## Quantizer notes

- Read input millivolts.
- Convert to semitones: `semitones = (mv * 12 + 500) / 1000` (round-to-nearest).
- Convert back: `quantized_mv = semitones * 1000 / 12`.
- Output via `set_voltage_calibrated_millivolts`.

## Feature flag dependencies

Same compile-time rules as every Brain firmware:
- `BRAIN_USE_OUTPUTS` ⇒ `BRAIN_USE_STORAGE`
- `BRAIN_USE_POT_MULTI_FUNCTION` ⇒ `BRAIN_USE_POTS`

S&H and quantizer also need `BRAIN_USE_INPUTS` for the analog input.

## References

- `brain/include/outputs.h` — `set_voltage_millivolts`, `set_voltage_calibrated_millivolts`, `set_output_range`.
- `brain/include/inputs.h` — analog input read in millivolts.
- `brain/include/pots.h` — pot read paths.
- `examples/apps/sample_and_hold.cpp` — S&H reference.
- `examples/apps/pots_multi_example.cpp` — pot reading patterns.
- `docs/PRESERVING_CV_CALIBRATION.md`.
