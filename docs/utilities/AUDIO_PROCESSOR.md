# AudioProcessor

## Overview
`AudioProcessor` is a deterministic ISR-driven audio utility for Brain firmware.

It owns:
- Audio/CV input A ADC sampling (`GPIO_BRAIN_AUDIO_CV_IN_A`)
- Pot-mux ADC sampling (`GPIO_BRAIN_POTMUX_ADC` + `S0/S1`)
- DMA ring transport for ADC samples
- Timer ISR loop at a fixed sample period
- SPI DAC channel-A output writes

User DSP is provided as a raw function pointer callback:
- no virtual dispatch
- no `std::function`
- callback runs in ISR context

## Include
```cpp
#include "brain/include/audio-processor.h"
```

You can use either:
- `AudioProcessor` (global alias)
- `brain::utils::AudioProcessor`

## Callback Type
```cpp
using ProcessSampleFn =
	int16_t (*)(int16_t input_sample, const AudioProcessorFrame* frame, void* user_ctx);
```

Callback contract:
- called once per timer tick (`sample_period_us`)
- called from ISR context (must be real-time safe)
- avoid blocking, dynamic allocation, logging, flash writes, and slow locks

## Config / Frame / Stats
```cpp
struct AudioProcessorConfig {
	uint32_t sample_period_us = 23;
	bool enable_pot_mux = true;
	uint8_t pot_count = 3;
	uint8_t pot_settle_discard_samples = 2;
	uint8_t pot_average_samples = 4;
	uint16_t max_dma_drain_samples_per_tick = 64;
	uint32_t spi_baud_hz = 1000000;
};

struct AudioProcessorFrame {
	uint64_t tick;
	uint16_t flags;               // kFlagOverrun currently defined
	uint8_t pot_count;
	uint8_t pot_raw_u8[4];        // 0..255 buffered pot values
};

struct AudioProcessorStats {
	uint64_t tick_count;
	uint32_t overrun_count;
	uint32_t pot_mux_switch_count;
	uint32_t pot_settle_discard_count;
};
```

## Brain Integration
Use via `Brain`:

```cpp
#define BRAIN_USE_AUDIO_PROCESSOR 1
#include "brain/brain.h"

Brain brain;
```

Init API:
- `BrainInitStatus init_audio_processor(const AudioProcessorConfig&, ProcessSampleFn, void* user_ctx = nullptr)`
- `bool is_audio_processor_initialized() const`
- member access: `brain.audio_processor`

Important:
- `init_all()` does not initialize `audio_processor`
- utility init is explicit

## Ownership Guardrails
When `audio_processor` is active, Brain rejects overlapping owners:
- `init_inputs()` -> `kFailed`
- `init_pots()` / `reconfigure_pots()` -> `kFailed`
- `init_pot_multi()` -> `kFailed`

And in reverse:
- if `inputs`, `pots`, or `pot_multi` already initialized, `init_audio_processor(...)` -> `kFailed`

## Minimal Example
```cpp
#define BRAIN_USE_ALL 1
#include "brain/brain.h"

struct FxState {
	int32_t lp = 0;
};

int16_t fx_callback(int16_t in, const AudioProcessorFrame* frame, void* user_ctx) {
	FxState* s = static_cast<FxState*>(user_ctx);
	uint8_t cutoff = (frame && frame->pot_count > 1) ? frame->pot_raw_u8[1] : 64;
	s->lp += ((static_cast<int32_t>(in) - s->lp) * (4 + (cutoff >> 1))) >> 8;
	return static_cast<int16_t>(s->lp);
}

int main() {
	Brain brain;
	FxState state{};

	AudioProcessorConfig cfg{};
	cfg.sample_period_us = 23;
	cfg.enable_pot_mux = true;
	cfg.pot_count = 3;

	if (!brain_init_succeeded(brain.init_audio_processor(cfg, fx_callback, &state))) {
		return 1;
	}

	while (true) {
		AudioProcessorStats stats = brain.audio_processor.get_stats();
		(void)stats;
		sleep_ms(10);
	}
}
```
