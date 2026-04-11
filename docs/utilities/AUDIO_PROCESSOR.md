# Audio Processor Utility

`AudioProcessor` is the SDK utility for deterministic, interrupt-driven sample processing.

Use it when you need a fixed sample-period callback that processes input audio/CV and writes output in a tight real-time loop.

It owns the low-level runtime path for:

- ADC sampling
- DMA ring transport
- optional pot-mux sampling
- timer ISR scheduling
- DAC channel-A output writes


## Processing model
You provide a callback:

```cpp
int16_t (*)(int16_t input_sample, const AudioProcessorFrame* frame, void* user_ctx)
```

That callback is called once per sample tick.
`frame` carries tick metadata, flags, and pot snapshots (if enabled).


## Example

This example shows the typical wrapper-based setup for `AudioProcessor`: define a DSP callback, create a small state object, and initialize the processor through `brain.init_audio_processor(...)` with an explicit config. Once initialized, `AudioProcessor` owns the real-time ADC/DMA/timer/DAC path and calls your callback at the configured sample period. The main loop stays lightweight and can be used for monitoring (for example reading stats), while the actual sample processing happens inside the callback.

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
	cfg.sample_period_us = 23;    // ~43.5kHz
	cfg.enable_pot_mux = true;
	cfg.pot_count = 3;
	cfg.pot_settle_discard_samples = 2;
	cfg.pot_average_samples = 4;
	cfg.max_dma_drain_samples_per_tick = 64;

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
- `BrainInitStatus init(const AudioProcessorConfig& config, ProcessSampleFn process_sample_fn, void* user_ctx = nullptr)`
  Initializes ADC/DMA/SPI/timer pipeline and starts processing loop.
- `void stop()`
  Stops processing and releases owned runtime resources.
- `bool is_initialized() const`
  Returns initialization/running state.

### Runtime telemetry
- `AudioProcessorStats get_stats() const`
  Returns counters such as tick count, overruns, pot mux switches, settle discards.
- `uint16_t get_pot_raw_u8(uint8_t index) const`
  Returns latest buffered pot snapshot value (0..255 style scale in frame context).


## Config and callback types

### `AudioProcessorConfig`
- `sample_period_us`
  Tick period in microseconds (`23` default).
- `enable_pot_mux`
  Enables pot sampling in processor path.
- `pot_count`
  Number of pots included in frame snapshots.
- `pot_settle_discard_samples`
  Number of post-switch samples discarded for settle.
- `pot_average_samples`
  Averaging count for pot read stability.
- `max_dma_drain_samples_per_tick`
  Safety bound for DMA drain workload per tick.
- `spi_baud_hz`
  SPI baud for DAC writes.

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
- `tick_count`
- `overrun_count`
- `pot_mux_switch_count`
- `pot_settle_discard_count`

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

## Ownership guardrail example (with wrapper)
```cpp
#define BRAIN_USE_POTS 1
#define BRAIN_USE_AUDIO_PROCESSOR 1
#include "brain/brain.h"

Brain brain;

static int16_t pass(int16_t in, const AudioProcessorFrame*, void*) { return in; }

int main() {
	if (!brain_init_succeeded(brain.init_pots())) return 1;

	AudioProcessorConfig cfg{};
	BrainInitStatus st = brain.init_audio_processor(cfg, pass, nullptr);
	(void)st; // expected kFailed due to ownership conflict
}
```

## Integration/ownership notes
When using `Brain`, audio processor ownership is exclusive vs `inputs`/`pots`/`pot_multi` runtime paths:

- If audio processor is active, `init_inputs`, `init_pots`, `init_pot_multi` fail.
- If those modules are already active, `init_audio_processor` fails.

## Real-time safety notes
Inside `ProcessSampleFn`, avoid:

- blocking calls
- dynamic allocation
- logging/printf
- flash writes
- heavy locks

Treat callback code as hard real-time DSP code.