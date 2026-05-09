---
name: brain-audio-effect
description: Use when creating, editing, or extending a Shmøergh Brain audio-effect firmware (filter, distortion, delay, reverb, EQ, waveshaper, chorus, etc.) — any firmware that processes an incoming audio signal at audio rate using the Brain SDK's AudioProcessor. Triggers when the user mentions DSP, audio processing, AudioProcessor, sample rate, filter cutoff, distortion drive, delay time, reverb, or asks to add an audio effect to a firmware that has brain-sdk as a submodule. Do not use for CV utilities (LFO/ADSR), MIDI tools, or sequencers.
---

# Brain audio-effect firmware

Use this skill whenever the user is building or modifying an audio-effect firmware on the Brain module. The Brain DSP path runs on a Pico (RP2040 or RP2350) — the constraints below are not stylistic preferences, they are what makes audio-rate code actually work on this hardware.

## Up-front questions (ask before writing code)

1. **Output range per channel.** "Should the audio output be unipolar (0..10 V) or bipolar (−5..+5 V)?" For audio effects the answer is almost always **bipolar** — `AudioProcessor` claims its channel into bipolar mode automatically when started, so the answer mainly matters if the firmware also writes non-audio CV to the *other* channel.
2. **Mono or stereo?** Mono uses the legacy `init_audio_processor(...)` + `ProcessSampleFn` (one input, one output, channel A). Stereo uses `init_audio_processor_v2(...)` + `ProcessFrameFnV2` (IN1+IN2 → OUT A+OUT B). Default to **mono** unless the user asks for stereo.
3. **What pots control what?** Brain has 3 pots. Map each pot index to a DSP parameter (e.g. POT0=cutoff, POT1=resonance, POT2=drive) before writing the callback.

## Hard rules

### No `float` / `double` in the per-sample callback

RP2040 has no FPU; RP2350 has an M33 FPU but division and transcendentals (sin, exp, tanh, log) are still expensive. The DSP callback fires at ~43 kHz — float math will overrun. Use:

- **Q15 fixed-point** for samples (they arrive as `int16_t` already).
- **Q15 / Q31** for coefficients and state.
- **Lookup tables** for sin/cos/tanh/exp (compute once at init, index by phase).
- **Bit shifts** instead of divides (`>> 8` not `/ 256`).
- **`int32_t` accumulators** to keep intermediate products from overflowing, then clamp to `int16_t` on output.

Floats are fine in `init()` (runs once) for things like computing LUTs.

The reference one-pole lowpass in `examples/apps/audio_processor_example.cpp` is the canonical pattern:

```cpp
// in: int16_t, cutoff: 0..255 from POT
s->lp += ((static_cast<int32_t>(in) - s->lp) * (4 + (cutoff >> 1))) >> 8;
return clamp_i16(s->lp);
```

Note: `int32_t` accumulator, integer-only math, `clamp_i16` to bound the result.

### Smooth pot reads, do not use them raw

Pot values arrive in `frame->pot_raw_u8[N]` (0..255). Reading them raw into a coefficient causes zipper noise. Apply a one-pole smoother on the *parameter*, not the audio:

```cpp
// 1-pole smoother on pot value, runs in the audio callback
s->cutoff_smoothed += (static_cast<int32_t>(frame->pot_raw_u8[0]) - s->cutoff_smoothed) >> 5;
```

The shift (5 here) sets the smoothing rate — bigger = slower.

### Preserve CV calibration

Brain firmwares default to preserving CV calibration in flash. Don't disable it. Keep the storage flash reservation (`brain-storage-reserve-flash.cmake`) and let `init_all()` initialize storage in protected-layout mode. See [docs/PRESERVING_CV_CALIBRATION.md](../../docs/PRESERVING_CV_CALIBRATION.md).

## Skeleton (mono, Brain SDK 2.1)

```cpp
#define BRAIN_USE_ALL 1
#include "brain/brain.h"

struct EffectState {
    int32_t lp = 0;
    int32_t cutoff_smoothed = 0;
};

static EffectState g_state;

static int16_t process_sample(int16_t in, const AudioProcessorFrame* frame, void* user_ctx) {
    EffectState* s = static_cast<EffectState*>(user_ctx);

    // smooth the pot
    if (frame && frame->pot_count > 0) {
        s->cutoff_smoothed += (static_cast<int32_t>(frame->pot_raw_u8[0]) - s->cutoff_smoothed) >> 5;
    }
    int32_t cutoff = s->cutoff_smoothed;  // 0..255

    // 1-pole lowpass (integer)
    s->lp += ((static_cast<int32_t>(in) - s->lp) * (4 + (cutoff >> 1))) >> 8;

    int32_t out = s->lp;
    if (out > 32767) out = 32767;
    if (out < -32768) out = -32768;
    return static_cast<int16_t>(out);
}

int main() {
    Brain brain;
    brain.init_all();

    AudioProcessorConfig cfg{};
    cfg.sample_period_us = 23;          // ~43.5 kHz
    cfg.enable_pot_mux = true;
    cfg.pot_count = 3;
    cfg.pot_settle_discard_samples = 2;
    cfg.pot_average_samples = 4;

    if (!brain_init_succeeded(brain.init_audio_processor(cfg, &process_sample, &g_state))) {
        // init failed — handle as the firmware needs
    }

    while (true) {
        brain.update();
    }
}
```

For stereo, use `init_audio_processor_v2(cfg_v2, &process_frame_v2, ctx)` with `ProcessFrameFnV2(int16_t in1, int16_t in2, const AudioProcessorFrameV2*, int16_t* out_a, int16_t* out_b, void*)`.

## Effect-specific starting shapes (all integer-only)

- **Filter (1-pole LP/HP):** see skeleton above. For 2-pole/SVF use the pattern in `examples/apps/audio_processor_example.cpp` and adapt with Q15 coefficients.
- **Distortion / waveshaper:** precompute a 256- or 1024-entry signed shape table at init (`int16_t shape[1024]`), index by `(in >> 5) + 512`. Drive = pre-gain shift before the table.
- **Delay:** allocate an `int16_t` ring buffer in static storage, sized to max delay × sample rate (e.g. 500 ms × 43500 ≈ 22 KB — fits in RP2040 RAM; fits with room on RP2350). Read with linear or all-pass interpolation; do NOT allocate in the audio callback.
- **Reverb:** Schroeder-style (4 combs + 2 all-pass), all integer. Avoid the temptation to use float coefficients — precompute everything as Q15 in init.

## Output range — the AudioProcessor gotcha

When `AudioProcessor` claims a channel for audio it forces that channel's range to **bipolar (−5..+5 V)** by writing the coupling pin high. `AudioProcessor::stop()` does **NOT** restore the prior range. If your firmware previously set a channel to unipolar via `Outputs::set_output_range(channel, kOutputsRange0To10V)` and then later starts/stops audio on that channel, you must call `set_output_range(...)` again after `stop()` to put it back into unipolar.

If your firmware drives the *non-audio* channel as CV, ask the user what range that channel should be in and call `set_output_range(...)` for it explicitly during init.

## Coexistence (Brain SDK 2.1)

In 2.1, `AudioProcessor` coexists freely with `Inputs`, `Pots`, and `PotMultiFunction` on a single `Brain` instance — they all share the internal ADC/Output engines. Order of init does not matter. (Pre-2.1 had ownership conflicts; if you're working on an older firmware, see the `brain-migrate` skill before applying patterns from this skill.)

## References

- `brain/include/audio-processor.h` — `AudioProcessor`, `AudioProcessorConfig`, frame/callback types.
- `brain/include/outputs.h` — `set_output_range`, `kOutputsRange0To10V`, `kOutputsRangeMinus5To5V`.
- `examples/apps/audio_processor_example.cpp` — canonical mono lowpass.
- `docs/PRESERVING_CV_CALIBRATION.md` — calibration default rule.
- `docs/2.1_MIGRATION.md` — coexistence and engine architecture.
- `AGENTS.md` (note: pre-2.1 ownership rules in §4 are obsolete in 2.1+).
