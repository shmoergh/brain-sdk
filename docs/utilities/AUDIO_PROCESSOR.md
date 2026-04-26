# Audio Processor Utility

`AudioProcessor` is the SDK utility for deterministic, interrupt-driven sample processing.

Use it when you need a fixed sample-period callback that processes input audio/CV and writes output in a tight real-time loop.

It owns:

- timer ISR scheduling at the configured sample period
- DAC channel-A output writes (SPI)

ADC sampling is handled by the shared `AdcEngine` — `AudioProcessor` subscribes to the audio input ADC channel at `init()` and pulls the latest sample from the engine each tick. Pot mux sampling is owned exclusively by `Pots`; `AudioProcessor` no longer touches multiplexer GPIOs or runs its own pot averaging. This means you can run `Pots`, `Inputs`, and `AudioProcessor` together with no special setup or restrictions.


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
  Returns tick and overrun counters. (`pot_mux_switch_count` and `pot_settle_discard_count` remain in the struct for source compatibility but are always `0` — pot mux work happens inside `Pots` now.)
- `uint16_t get_pot_raw_u8(uint8_t index) const`
  Legacy shim. Returns the latest pot value from the wired `Pots` instance, mapped to 8 bits. Returns `0` if no `Pots` was wired. New code should call `pots.get(i)` directly.


## Config and callback types

### `AudioProcessorConfig`
- `sample_period_us`
  Tick period in microseconds (`23` default).
- `spi_baud_hz`
  SPI baud for DAC writes.

#### Legacy fields (ignored at runtime)

These fields remain in the struct so existing code keeps compiling. They no longer drive behavior because pot sampling is owned by `Pots`/`AdcEngine`:

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
- `pot_mux_switch_count` — legacy field, always `0`.
- `pot_settle_discard_count` — legacy field, always `0`.

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

## Integration notes

`AudioProcessor` shares the ADC with `Pots` and `Inputs` through the `AdcEngine` singleton. There are no init-order constraints and no exclusion between modules — any combination can run concurrently. `Brain::init_audio_processor(...)` automatically wires `AudioProcessor` to `Pots` (via `set_pots`) when both are present, so pot snapshots flow into your callback's `frame->pot_raw_u8[]` without extra setup.

## Real-time safety notes
Inside `ProcessSampleFn`, avoid:

- blocking calls
- dynamic allocation
- logging/printf
- flash writes
- heavy locks

Treat callback code as hard real-time DSP code.
