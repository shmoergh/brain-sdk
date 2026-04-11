# Inputs Component

## Overview
`Inputs` combines:
- Audio/CV analog input (channels A/B)
- Pulse input (digital gate/trigger input)

## Include
```cpp
#include "brain/include/inputs.h"
```

## Quick Start
```cpp
Inputs inputs;
inputs.init();

while (true) {
	inputs.update();

	// Audio/CV
	uint16_t raw_a = inputs.get_raw_channel_a();                 // 0..4095
	int32_t mv_b = inputs.get_voltage_millivolts_channel_b();    // calibrated mV domain

	// Pulse input
	bool gate = inputs.pulse_read();
	(void)raw_a;
	(void)mv_b;
	(void)gate;
}
```

## Audio/CV Input API

### Initialization / Update
- `bool init_audio_cv()`
- `void update_audio_cv()`

### DMA Control
- `void set_audio_cv_dma_enabled(bool enabled)`
- `bool is_audio_cv_dma_enabled() const`
- `bool is_audio_cv_dma_active() const`

### Raw Readout
- `uint16_t get_raw(int channel)`
- `uint16_t get_raw_channel_a() const`
- `uint16_t get_raw_channel_b() const`

Channels:
- `AudioCvInChannel::kChannelA`
- `AudioCvInChannel::kChannelB`

### Voltage Readout (Millivolts)
- `int32_t get_voltage_millivolts(int channel) const`
- `int32_t get_voltage_millivolts_channel_a() const`
- `int32_t get_voltage_millivolts_channel_b() const`

## Pulse Input API
- `bool init_pulse()`
- `void pulse_end()`
- `bool pulse_read() const` logical level (inversion handled)
- `bool pulse_read_raw() const` raw GPIO level
- `void pulse_poll()`
- `void pulse_on_rise(std::function<void()> cb)`
- `void pulse_on_fall(std::function<void()> cb)`
- `void pulse_set_input_glitch_filter_us(uint32_t us)`
- `void pulse_enable_interrupts()`
- `void pulse_disable_interrupts()`

## Combined Helpers
- `bool init()` initializes audio/cv + pulse input
- `void update()` updates audio/cv + pulse input

## Notes
- Legacy float API `get_voltage*()` was replaced by millivolt getters.
- Use `BrainAdcLockGuard` if your app also accesses ADC registers/FIFO directly.
