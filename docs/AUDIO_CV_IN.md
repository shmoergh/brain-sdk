# AudioCvIn Component

## Overview
The AudioCvIn component provides a two-channel analog input interface for reading audio and CV (control voltage) signals using the RP2040's built-in ADC. It handles level-shifted ±5V Eurorack signals and provides both raw ADC values and calibrated voltage readings.

## Features
- Two independent analog input channels (A and B)
- Reads ±5V Eurorack signals (level-shifted to ADC range)
- 12-bit ADC resolution (0-4095)
- Voltage conversion with configurable calibration
- Returns values in original ±5V range
- Optimized for audio and CV signal processing

## Hardware
- **Channel A**: GPIO 27 (ADC1)
- **Channel B**: GPIO 28 (ADC2)
- Input signals are level-shifted from ±5V to ~240mV-3V for ADC compatibility
- Hardware calibration constants defined in `brain-common.h`

## Usage

### Basic Setup
1. **Initialization**: Create an `AudioCvIn` instance and call `init()`
2. **Update Loop**: Call `update()` regularly to refresh readings
3. **Read Values**: Use getter methods to access raw or voltage values

### Example - Reading CV Inputs
```cpp
#include "brain-io/audio-cv-in.h"

brain::io::AudioCvIn cv_in;

if (!cv_in.init()) {
    // Initialization failed
    return;
}

while (true) {
    cv_in.update();  // Refresh ADC readings

    // Get voltage values (-5.0V to +5.0V)
    float voltage_a = cv_in.get_voltage_channel_a();
    float voltage_b = cv_in.get_voltage_channel_b();

    printf("Channel A: %.2fV, Channel B: %.2fV\n", voltage_a, voltage_b);

    sleep_ms(10);
}
```

### Example - Reading Raw ADC Values
```cpp
#include "brain-io/audio-cv-in.h"

brain::io::AudioCvIn cv_in;
cv_in.init();

while (true) {
    cv_in.update();

    // Get raw 12-bit ADC values (0-4095)
    uint16_t raw_a = cv_in.get_raw_channel_a();
    uint16_t raw_b = cv_in.get_raw_channel_b();

    printf("Raw A: %d, Raw B: %d\n", raw_a, raw_b);

    sleep_ms(10);
}
```

### Example - Using Channel Constants
```cpp
#include "brain-io/audio-cv-in.h"
#include "brain-common/brain-common.h"

brain::io::AudioCvIn cv_in;
cv_in.init();

while (true) {
    cv_in.update();

    // Use channel constants for clearer code
    uint16_t raw = cv_in.get_raw(BRAIN_AUDIO_CV_IN_CHANNEL_A);
    float voltage = cv_in.get_voltage(BRAIN_AUDIO_CV_IN_CHANNEL_B);

    printf("Channel A raw: %d, Channel B: %.2fV\n", raw, voltage);

    sleep_ms(10);
}
```

## API Reference

### Initialization
- `bool init()` - Initialize ADC hardware and configure input channels
  - Returns `true` if successful, `false` on error

### Update
- `void update()` - Refresh ADC readings for both channels (call in main loop)

### Reading Values

#### Raw ADC Values (0-4095)
- `uint16_t get_raw(int channel)` - Get raw ADC value for specified channel
- `uint16_t get_raw_channel_a()` - Get raw ADC value for channel A
- `uint16_t get_raw_channel_b()` - Get raw ADC value for channel B

#### Voltage Values (-5.0V to +5.0V)
- `float get_voltage(int channel)` - Get converted voltage for specified channel
- `float get_voltage_channel_a()` - Get converted voltage for channel A
- `float get_voltage_channel_b()` - Get converted voltage for channel B

## Channel Constants
Use these constants (defined in `brain-common.h`) when calling methods:
- `BRAIN_AUDIO_CV_IN_CHANNEL_A` - Channel A constant
- `BRAIN_AUDIO_CV_IN_CHANNEL_B` - Channel B constant

## Voltage Conversion
The component automatically converts ADC readings to the original ±5V signal range using calibration constants:
- Hardware level-shifts ±5V input to ~240mV-3V for ADC
- Calibration constants account for the level-shifting circuit
- Output voltage represents the original ±5V Eurorack signal

Calibration constants are defined in `brain-common.h`:
- `BRAIN_AUDIO_CV_IN_CAL_ADC_LOW` - ADC value at -5V input
- `BRAIN_AUDIO_CV_IN_CAL_ADC_HIGH` - ADC value at +5V input

## Performance Notes
- ADC provides 12-bit resolution (4096 steps)
- Over ±5V range, this gives ~2.44mV per step
- Sufficient for most Eurorack CV applications (1V/octave = ~409 steps/octave)
- For audio signals, consider oversampling if needed
- `update()` is non-blocking and fast

## Common Use Cases
- **CV Input**: Read control voltages for modulation, sequencing, etc.
- **Audio Input**: Read audio signals for processing or analysis
- **Envelope Following**: Track amplitude of incoming signals
- **Pitch Detection**: Analyze audio/CV signals (requires additional processing)
- **Gate/Trigger Detection**: Threshold voltage readings for digital events

## Notes
- Designed for Eurorack ±5V signals
- Call `update()` regularly for continuous monitoring
- Voltage accuracy depends on hardware calibration
- ADC input impedance may affect high-impedance sources
- For best results with AC signals, consider the ADC sample rate
