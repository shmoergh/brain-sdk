# Audio Processor Utility

`AudioProcessor` is the SDK utility for sample-rate audio DSP. You give it a callback; it calls that callback once per audio sample on a hardware-paced loop and routes the result to the DAC.

In 2.1 it's a thin shim over the SDK's shared `AdcEngine` and `OutputEngine`. It coexists freely with `Inputs`, `Pots`, `PotMultiFunction`, and `Outputs` on one `Brain` instance — they all share the same underlying ADC/DAC engines.

Two callback shapes are available:

- **Legacy (mono):** one input (IN1) → DSP → one output (OUT A). Use `init_audio_processor` + `ProcessSampleFn`.
- **V2 (stereo):** both inputs (IN1, IN2) → DSP → both outputs (OUT A, OUT B). Use `init_audio_processor_v2` + `ProcessFrameFnV2`. You can claim either channel or both.

## How the audio loop works

1. The ADC samples (POT, IN1, IN2) in round-robin at the audio sample period (default 23 µs ≈ 43.5 kHz).
2. After each frame the ADC fires an IRQ and the IRQ calls your DSP function.
3. Your function returns the output sample(s).
4. `OutputEngine` streams the result(s) to the DAC at the same rate, locked to the ADC clock — no drift, no buffering glitches.

There is no internal queue between your callback and the DAC: the sample your callback returns *is* the next sample the DAC outputs.


## Mono example (legacy API)

A simple lowpass filter driven by POT2:

```cpp
#define BRAIN_USE_AUDIO_PROCESSOR 1
#include "brain/brain.h"

#include <pico/stdlib.h>

Brain brain;

struct FxState {
    int32_t lp = 0;
};

static int16_t lowpass(int16_t in, const AudioProcessorFrame* frame, void* ctx) {
    FxState* s = static_cast<FxState*>(ctx);

    uint8_t cutoff = 64;
    if (frame && frame->pot_count > 1) {
        cutoff = frame->pot_raw_u8[1];
    }

    s->lp += ((static_cast<int32_t>(in) - s->lp) * (4 + (cutoff >> 1))) >> 8;
    return static_cast<int16_t>(s->lp);
}

int main() {
    stdio_init_all();

    FxState state{};
    AudioProcessorConfig cfg{};
    cfg.sample_period_us = 23;     // ~43.5 kHz
    cfg.enable_pot_mux = true;
    cfg.pot_count = 3;
    cfg.pot_settle_discard_samples = 2;
    cfg.pot_average_samples = 4;

    if (!brain_init_succeeded(brain.init_audio_processor(cfg, lowpass, &state))) {
        return 1;
    }

    while (true) {
        // Main loop is free for monitoring/UI work.
        sleep_ms(10);
    }
}
```


## Stereo example (v2 API)

Stereo passthrough with both inputs and both outputs:

```cpp
#define BRAIN_USE_AUDIO_PROCESSOR 1
#include "brain/brain.h"

#include <pico/stdlib.h>

Brain brain;

static void stereo_passthrough(int16_t in1, int16_t in2,
                               const AudioProcessorFrameV2* /*frame*/,
                               int16_t* out_a, int16_t* out_b,
                               void* /*user_ctx*/) {
    *out_a = in1;
    *out_b = in2;
}

int main() {
    AudioProcessorConfigV2 cfg{};
    cfg.sample_rate_hz = 43500;
    cfg.enable_pot_mux = true;
    cfg.pot_count = 3;
    cfg.claim_channel_a = true;
    cfg.claim_channel_b = true;

    if (!brain_init_succeeded(
            brain.init_audio_processor_v2(cfg, stereo_passthrough, nullptr))) {
        return 1;
    }

    while (true) {
        sleep_ms(10);
    }
}
```

You can opt out of one channel (`claim_channel_b = false`) and still use the v2 callback shape — useful when you want both inputs available but only one output is being driven by audio (the other one is free for `Outputs::set_voltage_*` writes).


## Coexistence with other components

In 2.1, `AudioProcessor` does not block any other component:

```cpp
brain.init_audio_processor(cfg, dsp, &state);
brain.init_outputs();      // OK
brain.init_inputs();       // OK
brain.init_pots();         // OK
brain.init_pot_multi();    // OK
```

Order doesn't matter — any combination works.

The only runtime constraint is per-channel: when `AudioProcessor` claims an output channel as audio, `Outputs::set_voltage_*` writes to that channel return `false` (no-op) until `AudioProcessor::stop()` releases it. The unclaimed channel is unaffected — you can write manual CV to it normally.

That's the entire ownership story now. There are no init-order rules and no "this must be initialized before that" gotchas.


## API reference

### Core lifecycle

- `BrainInitStatus init(const AudioProcessorConfig& config, ProcessSampleFn fn, void* ctx = nullptr)`
  Starts the legacy single-channel (IN1 → OUT A) audio loop.
- `BrainInitStatus init_v2(const AudioProcessorConfigV2& config, ProcessFrameFnV2 fn, void* ctx = nullptr)`
  Starts the v2 stereo (IN1+IN2 → OUT A+OUT B) audio loop.
- `void stop()`
  Stops audio processing, releases claimed output channels, and reverts the ADC engine to CV mode.
- `bool is_initialized() const`
  Returns whether audio is currently running.

### Stats / introspection

- `AudioProcessorStats get_stats() const`
  Returns `tick_count`, `overrun_count`, `pot_mux_switch_count`, `pot_settle_discard_count`.
- `uint16_t get_pot_raw_u8(uint8_t index) const`
  Returns the latest 8-bit pot value (0..255). Reads from the shared `AdcEngine` snapshot, so this works whether or not pot scanning was enabled in the audio config (you'll just get 0s if pots aren't being scanned).


## Config and callback types

### `AudioProcessorConfig` (legacy)

- `sample_period_us` — Audio sample period in microseconds. Default `23` (~43.5 kHz).
- `enable_pot_mux` — Whether to enable pot scanning in the shared ADC engine.
- `pot_count` — Number of pots scanned (max `AudioProcessorFrame::kMaxPots` = 4).
- `pot_settle_discard_samples` — Number of pot samples discarded after each mux switch.
- `pot_average_samples` — Number of pot samples averaged before publishing a fresh value.
- `max_dma_drain_samples_per_tick` — *Ignored in 2.1.* Preserved for source compatibility.
- `spi_baud_hz` — *Ignored in 2.1.* Preserved for source compatibility.

### `AudioProcessorConfigV2` (stereo)

- `sample_rate_hz` — Audio sample rate in Hz. Default `43500` (≈ 23 µs period). The implementation derives `sample_period_us = 1'000'000 / sample_rate_hz`.
- `enable_pot_mux` — As above.
- `pot_count` — As above.
- `pot_settle_discard_samples` — As above.
- `pot_average_samples` — As above.
- `claim_channel_a` — Claim DAC channel A for audio output. Default `true`.
- `claim_channel_b` — Claim DAC channel B for audio output. Default `true`. At least one channel must be claimed.

### `AudioProcessorFrame` (passed to legacy callback)

- `tick` — Monotonic tick counter (1, 2, 3, ...).
- `flags` — Frame flags (currently just `kFlagOverrun`).
- `pot_count` — Number of valid entries in `pot_raw_u8`.
- `pot_raw_u8[kMaxPots]` — Latest 8-bit pot values from the shared engine snapshot.

### `AudioProcessorFrameV2` (passed to v2 callback)

Same shape as `AudioProcessorFrame`. The pot fields update at the same rate.

### `AudioProcessorStats`

- `tick_count` — Number of DSP callback invocations since `init`.
- `overrun_count` — Approximate count of audio glitches. In 2.1, derived from the `OutputEngine` ring underrun counter delta. Should stay at 0 with a well-behaved DSP.
- `pot_mux_switch_count` — Mux switches since engine start.
- `pot_settle_discard_count` — Settle samples discarded during pot scanning.

### Callback types

```cpp
using ProcessSampleFn = int16_t (*)(int16_t input_sample,
                                     const AudioProcessorFrame* frame,
                                     void* user_ctx);

using ProcessFrameFnV2 = void (*)(int16_t in1, int16_t in2,
                                   const AudioProcessorFrameV2* frame,
                                   int16_t* out_a, int16_t* out_b,
                                   void* user_ctx);
```


## Real-time safety

Your DSP function runs in the ADC IRQ at audio rate (~43 kHz at default settings = ~23 µs per call). Inside it, avoid:

- blocking calls of any kind (`sleep_*`, `printf`, USB I/O, flash writes)
- dynamic allocation (`new`, `malloc`)
- holding mutexes/locks
- long loops or recursive work that doesn't fit comfortably under the sample period

The engine bumps the audio IRQ to highest priority on Cortex-M, so your DSP also briefly delays other IRQs. Keep callback work small.

The main loop runs at normal priority and is yours for UI, monitoring, MIDI handling, etc.


## Stopping audio

```cpp
brain.audio_processor.stop();
```

Stops the audio loop, releases any claimed output channels back to manual ownership, and switches `AdcEngine` back to CV mode for any other consumers (`Inputs`, `Pots`). The output channel(s) snap to the last `set_voltage_*` value written before audio claimed them, or 0 if no manual value was ever set. Glitch-free transition.

After `stop()`, you can call `init` or `init_v2` again to restart with a different config.
