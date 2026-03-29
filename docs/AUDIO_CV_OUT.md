# AudioCvOut Component

## Overview
The AudioCvOut component provides a two-channel analog output interface using an MCP4822 dual DAC with switchable DC/AC coupling. It enables precise voltage control (0-10V) for Eurorack CV and audio output applications.

## Features
- Two independent output channels (A and B)
- 12-bit DAC resolution (4096 steps)
- 0-10V output range per channel
- Switchable DC/AC coupling per channel via CD4053 analog switch
- Optional calibrated voltage output path (loaded from flash or injected in-memory)
- SPI-based communication with MCP4822
- Configurable GPIO pins for flexibility
- Eurorack-compatible output levels

## Hardware
- **DAC**: MCP4822 dual 12-bit DAC
- **Coupling Switch**: CD4053 analog switch for DC/AC selection
- **Default GPIO Assignments**:
  - SPI SCK: GPIO 2 (clock)
  - SPI TX/MOSI: GPIO 3 (data)
  - CS (chip select): GPIO 5
  - Coupling A control: GPIO 6
  - Coupling B control: GPIO 7
- **SPI Instance**: spi0 (default)

## Usage

### Basic Setup
1. **Initialization**: Create an `AudioCvOut` instance and call `init()`
2. **Set Voltage**: Use `set_voltage()` to output desired voltage
3. **Configure Coupling**: Use `set_coupling()` to select DC or AC mode

### Example - Simple CV Output
```cpp
#include "brain-io/audio-cv-out.h"

brain::io::AudioCvOut cv_out;

if (!cv_out.init()) {
    // Initialization failed
    return;
}

// Set channel A to 5V (DC coupled by default)
cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, 5.0f);

// Set channel B to 3.3V
cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, 3.3f);
```

### Example - DC vs AC Coupling
```cpp
#include "brain-io/audio-cv-out.h"

brain::io::AudioCvOut cv_out;
cv_out.init();

// Channel A: DC coupled for CV (preserves DC component)
cv_out.set_coupling(brain::io::AudioCvOutChannel::kChannelA,
                    brain::io::AudioCvOutCoupling::kDcCoupled);
cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, 2.5f);

// Channel B: AC coupled for audio (blocks DC, passes AC)
cv_out.set_coupling(brain::io::AudioCvOutChannel::kChannelB,
                    brain::io::AudioCvOutCoupling::kAcCoupled);
cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelB, 5.0f);  // Bias point
```

### Example - Custom GPIO Configuration
```cpp
#include "brain-io/audio-cv-out.h"

brain::io::AudioCvOut cv_out;

// Initialize with custom GPIO pins
bool success = cv_out.init(
    spi0,      // SPI instance
    5,         // CS pin
    2,         // SCK pin
    3,         // TX/MOSI pin
    6,         // Coupling control pin A
    7          // Coupling control pin B
);

if (success) {
    cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, 4.0f);
}
```

### Example - 1V/Octave CV Output
```cpp
#include "brain-io/audio-cv-out.h"

brain::io::AudioCvOut cv_out;
cv_out.init();

// Ensure DC coupling for accurate CV
cv_out.set_coupling(brain::io::AudioCvOutChannel::kChannelA,
                    brain::io::AudioCvOutCoupling::kDcCoupled);

// Output C2 (MIDI note 36) at 1V/octave (assuming 0V = C0, MIDI 24)
// (36 - 24) / 12 = 1.0V (one octave above C0)
uint8_t midi_note = 36;
float voltage = (midi_note - 24) / 12.0f;
cv_out.set_voltage(brain::io::AudioCvOutChannel::kChannelA, voltage);
```

### Example - Calibrated Output
```cpp
#include "brain-io/audio-cv-out.h"

brain::io::AudioCvOut cv_out;
cv_out.init();
cv_out.set_coupling(brain::io::AudioCvOutChannel::kChannelA,
                    brain::io::AudioCvOutCoupling::kDcCoupled);

// Load calibration from reserved flash. If unavailable/corrupt, keep going.
if (!cv_out.load_calibration_from_flash()) {
    printf("Calibration not found or invalid, using raw output.\n");
}

cv_out.set_voltage_calibrated(brain::io::AudioCvOutChannel::kChannelA, 3.0f);
```

## API Reference

### Initialization
```cpp
bool init(spi_inst_t* spi_instance = spi0,
          uint cs_pin = GPIO_BRAIN_AUDIO_CV_OUT_CS,
          uint sck_pin = GPIO_BRAIN_AUDIO_CV_OUT_SCK,
          uint tx_pin = GPIO_BRAIN_AUDIO_CV_OUT_TX,
          uint coupling_pin_a = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A,
          uint coupling_pin_b = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B)
```
- Initialize SPI and GPIO for DAC and coupling control
- All parameters optional (uses Brain module defaults)
- Returns `true` if successful, `false` on error

### Voltage Control
```cpp
bool set_voltage(AudioCvOutChannel channel, float voltage)
```
- Set output voltage for specified channel
- `channel`: `kChannelA` or `kChannelB`
- `voltage`: 0.0V to 10.0V
- Returns `true` if successful, `false` if out-of-range (`<0V` or `>10V`)

```cpp
bool set_voltage_calibrated(AudioCvOutChannel channel, float target_voltage)
```
- Set output voltage using in-memory calibration table when loaded
- `target_voltage` is clamped to `0.0V..10.0V`
- Applies interpolated DAC-LSB offset and clamps final DAC code to `0..4095`

```cpp
bool set_calibration(const brain::storage::CvCalibrationV1& cal)
void clear_calibration()
bool load_calibration_from_flash()
bool has_calibration() const
uint16_t get_last_dac_value(AudioCvOutChannel channel) const
```
- `set_calibration(...)`: loads calibration table into memory
- `clear_calibration()`: disables calibration for calibrated writes
- `load_calibration_from_flash()`: reads calibration via `brain::storage`
- `has_calibration()`: reports in-memory calibration state
- `get_last_dac_value(...)`: diagnostics helper for tests/debug

### Coupling Control
```cpp
bool set_coupling(AudioCvOutChannel channel, AudioCvOutCoupling coupling)
```
- Configure DC/AC coupling for specified channel
- `channel`: `kChannelA` or `kChannelB`
- `coupling`: `kDcCoupled` or `kAcCoupled`
- Returns `true` if successful

## Enums

### AudioCvOutChannel
- `kChannelA` - Channel A (DAC channel 0)
- `kChannelB` - Channel B (DAC channel 1)

### AudioCvOutCoupling
- `kDcCoupled` - Direct coupling, preserves DC component (for CV)
- `kAcCoupled` - AC coupling, blocks DC component (for audio)

## DC vs AC Coupling

### DC Coupled (kDcCoupled)
- Passes both DC and AC components
- Use for: CV signals, envelope outputs, LFOs, sequencer outputs
- Output matches DAC voltage exactly (0-10V)

### AC Coupled (kAcCoupled)
- Blocks DC component, passes AC component only
- Use for: Audio signals, oscillators
- DC level is removed via capacitor
- Useful to center audio signals around 0V

## Performance Notes
- 12-bit resolution = 4096 steps over 10V range
- Voltage step size: ~2.44mV
- Sufficient for 1V/octave CV (1V = ~409 steps = 83 cents/step)
- SPI speed configured for fast updates
- No DMA - updates are blocking but fast
- For audio synthesis, update rate depends on your sample rate

## Common Use Cases
- **CV Output**: Pitch CV (1V/octave), modulation CV, envelope output
- **Sequencer**: Note CV and gate/trigger generation
- **LFO**: Low-frequency modulation output
- **Audio Output**: Simple waveform generation (with AC coupling)
- **Control Signals**: Any 0-10V control parameter

## Important Notes
- Voltage range is 0-10V (Eurorack standard)
- `set_voltage()` rejects out-of-range values
- `set_voltage_calibrated()` clamps input voltage and DAC output safely
- Default coupling mode is DC coupled
- MCP4822 provides simultaneous buffered outputs
- SPI communication is fast but blocking
- For high-speed audio, consider interrupt-driven updates
- GPIO defaults are defined in `brain-gpio-setup.h`

## Wiring Reference
The Brain module handles wiring internally. If building custom hardware:
- Connect MCP4822 to Pico via SPI (SCK, MOSI, CS)
- Connect CD4053 control inputs to coupling control GPIOs
- Ensure proper power supply decoupling
- Output impedance considerations for Eurorack loads
