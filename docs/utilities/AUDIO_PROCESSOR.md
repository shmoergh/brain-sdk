# Audio Processor Utility

`AudioProcessor` is the SDK utility for deterministic, interrupt-driven sample processing.

Use it when you need a fixed sample-period callback that processes input audio/CV and writes output in a tight real-time loop.

It owns:

- timer ISR scheduling at the configured sample period
- DAC output writes (SPI) — channel A in single-stream mode, both A and B in dual-stream mode

ADC sampling is handled by the shared `AdcEngine` — `AudioProcessor` subscribes to the audio input channel(s) at `init()` and pulls the latest sample from the engine each tick. Pot mux sampling is owned by `Pots`. `Pots`, `Inputs`, and `AudioProcessor` can run together in any combination.

`AudioProcessor` ships with two `init(...)` overloads:

- **Single-stream** (the 80% case): Input A → callback → Output A. One ADC subscription, one SPI write per tick. This is the lean default — channel B is left alone and incurs no per-tick cost.
- **Dual-stream**: both Input A → Output A and Input B → Output B in the same callback, with optional cross-mix between the streams. Two ADC subscriptions, two SPI writes per tick.


## Processing model
You provide a callback:

```cpp
int16_t (*)(int16_t input_sample, const AudioProcessorFrame* frame, void* user_ctx)
```

That callback is called once per sample tick. `frame` carries tick metadata and flags.

Pot snapshots (`frame.pot_raw_u8[]`, `frame.pot_count`) are populated only when `AudioProcessor` has been wired to a `Pots` instance via `set_pots(...)`. `Brain::init_audio_processor(...)` does this automatically when `Pots` is also initialized; if you build directly without `Brain`, call `audio_processor.set_pots(&pots)` before `init(...)`.


## Example

This example shows the typical wrapper-based setup: init `Pots` and `AudioProcessor` together, define a DSP callback that reacts to a knob, and let `Brain::init_audio_processor(...)` wire everything up. Once initialized, the audio timer fires every `sample_period_us` and your callback runs in ISR context; the main loop stays lightweight and can be used for monitoring (for example reading stats).

```cpp
#define BRAIN_USE_POTS 1
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

	// Init Pots first so AudioProcessor can be wired to it automatically
	// and pot values appear in `frame->pot_raw_u8[]`.
	if (!brain_init_succeeded(brain.init_pots(create_default_pots_config(3, 8)))) return 1;

	FxState state{};
	AudioProcessorConfig cfg{};
	cfg.sample_period_us = 23;    // ~43.5kHz

	BrainInitStatus status = brain.init_audio_processor(cfg, lowpass, &state);
	if (!brain_init_succeeded(status)) return 1;

	while (true) {
		AudioProcessorStats stats = brain.audio_processor.get_stats();
		(void)stats;
		sleep_ms(10);
	}
}
```

## Audio processor API

### Core lifecycle
- `void set_pots(Pots* pots)`
  Wires up an optional `Pots` instance so `frame->pot_raw_u8[]` and `get_pot_raw_u8(i)` return live values. `Brain::init_audio_processor(...)` calls this automatically when `Pots` is initialized; only needed when building without `Brain`.
- `BrainInitStatus init(const AudioProcessorConfig& config, ProcessSampleFn process_sample_fn, void* user_ctx = nullptr)`
  Initializes the SPI DAC, subscribes to the audio input channel via `AdcEngine`, and starts the sample-rate timer.
- `void stop()`
  Stops the timer, releases the SPI DAC, and unsubscribes from `AdcEngine`.
- `bool is_initialized() const`
  Returns initialization/running state.

### Constants
- `kBrainInitStatusOk`, `kBrainInitStatusAlreadyInitialized`, `kBrainInitStatusFailed`
  Initialization status constants returned by `init(...)`.

### Runtime telemetry
- `AudioProcessorStats get_stats() const`
  Returns tick and overrun counters. (`pot_mux_switch_count` and `pot_settle_discard_count` are legacy fields on the struct and are always `0`.)
- `uint16_t get_pot_raw_u8(uint8_t index) const`
  Legacy shim. Returns the latest pot value from the wired `Pots` instance, mapped to 8 bits. Returns `0` if no `Pots` was wired. Prefer calling `pots.get(i)` directly.


## Config and callback types

### `AudioProcessorConfig`
- `sample_period_us`
  Tick period in microseconds (`23` default).
- `spi_baud_hz`
  SPI baud for DAC writes.

#### Legacy fields (ignored at runtime)

These fields are accepted on the config struct for source compatibility and have no runtime effect. Pot sampling is configured via `Pots`/`PotsConfig`:

- `enable_pot_mux` — ignored. Pot sampling is configured on the `Pots` side.
- `pot_count` — ignored. Pot count is configured on the `Pots` side.
- `pot_settle_discard_samples` — ignored. Use `PotsConfig::settle_discard_samples`.
- `pot_average_samples` — ignored. Use `PotsConfig::samples_per_read`.
- `max_dma_drain_samples_per_tick` — ignored. DMA draining is owned by `AdcEngine`.

### `AudioProcessorFrame`
- `tick`
  Processing tick counter.
- `flags`
  Frame flags (e.g. overrun flag).
- `pot_count`
  Number of valid pot entries in this frame.
- `pot_raw_u8[kMaxPots]`
  Pot snapshots.

### `AudioProcessorStats`
- `tick_count` — number of sample-rate timer ticks since init.
- `overrun_count` — ticks that exceeded `sample_period_us` of work.
- `pot_mux_switch_count` — legacy field. Always `0`.
- `pot_settle_discard_count` — legacy field. Always `0`.

### Callback type
- `using ProcessSampleFn = int16_t (*)(int16_t input_sample, const AudioProcessorFrame* frame, void* user_ctx);`
  User DSP callback invoked each sample tick.


## Minimal passthrough example
```cpp
#include "brain/include/audio-processor.h"

AudioProcessor proc;

static int16_t passthrough(int16_t in, const AudioProcessorFrame*, void*) {
	return in;
}

int main() {
	AudioProcessorConfig cfg{};
	if (!brain_init_succeeded(proc.init(cfg, passthrough, nullptr))) return 1;
	while (true) {}
}
```

## Dual-stream mode

For two simultaneous audio streams (e.g. a stereo effect, or two independent mono effects on separate inputs), use the dual-stream `init(...)` overload. AudioProcessor will subscribe to both ADC input channels, call your callback once per tick with both inputs, and write both DAC channels every tick.

### Callback signature

```cpp
struct DualStreamSamples {
    int16_t in[kMaxAudioStreams];   // in[kAudioStreamA], in[kAudioStreamB]
    int16_t out[kMaxAudioStreams];  // user fills out[kAudioStreamA], out[kAudioStreamB]
};

using ProcessDualStreamFn = void (*)(
    DualStreamSamples* samples,
    const AudioProcessorFrame* frame,
    void* user_ctx);
```

`samples->out[]` is pre-populated with a passthrough of `samples->in[]` so a callback that returns without writing produces clean passthrough on both channels.

### Stereo passthrough example

```cpp
#define BRAIN_USE_AUDIO_PROCESSOR 1
#include "brain/brain.h"

Brain brain;

static void passthrough_dual(DualStreamSamples* /*samples*/, const AudioProcessorFrame*, void*) {
    // samples->out[] is pre-filled with samples->in[]; nothing to do.
}

int main() {
    AudioProcessorConfig cfg{};
    cfg.sample_period_us = 23;
    cfg.spi_baud_hz = 8000000;   // raise for dual-stream — see budget table below

    if (!brain_init_succeeded(brain.init_audio_processor(cfg, passthrough_dual, nullptr))) return 1;
    while (true) {}
}
```

### Cross-mix example (mid/side)

The dual callback sees both inputs at once, so cross-channel processing is natural:

```cpp
static void mid_side(DualStreamSamples* s, const AudioProcessorFrame*, void*) {
    const int16_t mid  = (s->in[kAudioStreamA] + s->in[kAudioStreamB]) / 2;
    const int16_t side = (s->in[kAudioStreamA] - s->in[kAudioStreamB]) / 2;
    // ...process mid and/or side separately here...
    s->out[kAudioStreamA] = mid + side;
    s->out[kAudioStreamB] = mid - side;
}
```

### Performance budget

At the default 23 µs sample period (~3450 cycles per tick on RP2350 @ 150 MHz, ~6900 @ 300 MHz):

| Per-tick step                              | Cycles (est.) | µs at 150 MHz |
|--------------------------------------------|--------------:|--------------:|
| `AdcEngine::drain_now()` (steady state)    | 50–100        | ~0.5          |
| 2× `get_latest()` (with IRQ guard)         | 60            | ~0.4          |
| `fill_pot_frame()` (3 pots)                | 80            | ~0.5          |
| 2× `write_dac_channel` @ 8 MHz SPI         | ~1200         | ~8            |
| 2× SPI mutex acquire/release               | ~30           | ~0.2          |
| User dual callback (typical)               | 200–2000      | 1.3–13        |
| Bookkeeping + return                       | ~100          | ~0.7          |
| **Total (typical)**                        | ~1700–3500    | **~11–23**    |

Notes:
- The `AudioProcessorConfig::spi_baud_hz` default is 1 MHz — too low for dual-stream. At 1 MHz, the two SPI writes alone would take ~32 µs and overrun every tick. **Recommend ≥ 4 MHz, ideally 8 MHz** in dual-stream mode. The MCP4822 supports up to 20 MHz.
- If your DSP callback is heavy, stretch `sample_period_us` to 30–40 µs for breathing room, or overclock the RP2350 to 300 MHz to roughly double the cycle budget.
- Single-stream mode is unaffected — only one ADC channel and one SPI write per tick. The same table for single stream halves the SPI line and removes one `get_latest`.

If `AudioProcessorStats::overrun_count` climbs while running dual-stream, the most likely culprit is `spi_baud_hz` being too low.

## Integration notes

`AudioProcessor` shares the ADC with `Pots` and `Inputs` through the `AdcEngine` singleton. There are no init-order constraints and no exclusion between modules — any combination can run concurrently. `Brain::init_audio_processor(...)` automatically wires `AudioProcessor` to `Pots` (via `set_pots`) when both are present, so pot snapshots flow into your callback's `frame->pot_raw_u8[]` without extra setup.

`AudioProcessor` and `Outputs` write to the same physical MCP4822 DAC over the same SPI bus. Their writes are safely serialized by an internal SPI mutex — both classes take the lock for the duration of each transaction, so audio-rate writes from the audio ISR and CV writes from `Outputs` never collide on the chip-select line. There is no init-order rule between them.

## Real-time safety notes
Inside `ProcessSampleFn`, avoid:

- blocking calls
- dynamic allocation
- logging/printf
- flash writes
- heavy locks

Treat callback code as hard real-time DSP code.
