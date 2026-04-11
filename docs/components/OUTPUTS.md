# Outputs Component

## Overview
`Outputs` combines:
- Audio/CV output (DAC channels A/B)
- Pulse output (digital gate/trigger output)

## Include
```cpp
#include "brain/include/outputs.h"
```

## Quick Start
```cpp
Outputs outputs;
outputs.init();

// CV output
outputs.set_output_range(AudioCvOutChannel::kChannelA, AudioCvOutRange::kRange0To10V);
outputs.set_voltage_millivolts(AudioCvOutChannel::kChannelA, 2500);

// Pulse output
outputs.pulse_set(true);
sleep_ms(5);
outputs.pulse_set(false);
```

## Audio/CV Output API

### Initialization
- `bool init_audio_cv(...)`

### Voltage + Range
- `bool set_voltage_millivolts(AudioCvOutChannel channel, int32_t millivolts)`
- `bool set_voltage_calibrated_millivolts(AudioCvOutChannel channel, int32_t target_millivolts)`
- `bool set_output_range(AudioCvOutChannel channel, AudioCvOutRange range)`
- `AudioCvOutRange get_output_range(AudioCvOutChannel channel) const`
- `int32_t get_last_set_millivolts(AudioCvOutChannel channel) const`
- `uint16_t get_last_dac_value(AudioCvOutChannel channel) const`

Range semantics:
- `AudioCvOutRange::kRange0To10V` accepts `0..10000` mV
- `AudioCvOutRange::kRangeMinus5To5V` accepts `-5000..5000` mV

`set_voltage_millivolts(...)` fails (`false`) when out-of-range.
`set_voltage_calibrated_millivolts(...)` clamps safely.

### Calibration
- `bool set_calibration(const CvCalibrationV1& cal)`
- `void clear_calibration()`
- `bool load_calibration_from_flash()`
- `bool has_calibration() const`

## Pulse Output API
- `void init_pulse()`
- `void pulse_set(bool on)`
- `bool pulse_get() const`

## Combined Helpers
- `bool init()` initializes cv + pulse output

## Migration Note
Legacy APIs removed:
- `set_voltage(float)`
- `set_voltage_calibrated(float)`
- `set_coupling(AudioCvOutCoupling)`

Use millivolts + `set_output_range(...)`.
