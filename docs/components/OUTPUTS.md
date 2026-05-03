# Outputs Module

The `Outputs` module is the Brain SDK’s write side for external signals. It handles two different output types:

- **Audio/CV outputs** on channels A and B (through DAC + range switching)
- **Pulse output** (digital on/off gate/trigger style output)

In practice, you use it whenever your firmware needs to send voltages or trigger/gate events out of the module.


## Audio/CV outputs
Audio/CV output is channel-based (`kOutputsChannelA`, `kOutputsChannelB`) and range-aware.

### Constants

- `kOutputsChannelA`, `kOutputsChannelB`
  Output channel selectors.

Supported output ranges:

- `kOutputsRange0To10V` (unipolar/pseudo-DC coupled)
- `kOutputsRangeMinus5To5V` (bipolar/pseudo-AC coupled)

You should set output range before writing voltages so value validation behaves as expected.


### Example
```cpp
#define BRAIN_USE_STORAGE 1
#define BRAIN_USE_OUTPUTS 1
#include "brain/brain.h"

#include <pico/stdlib.h>

Brain brain;

int main() {
    stdio_init_all();

    BrainInitStatus status = brain.init_outputs();
    if (!brain_init_succeeded(status)) return 1;

    // Configure channel A for bipolar CV
    brain.outputs.set_output_range(
        kOutputsChannelA,
        kOutputsRangeMinus5To5V
    );

    // Write +1.25V
    bool ok = brain.outputs.set_voltage_millivolts(
        kOutputsChannelA,
        1250
    );
    (void)ok;

    while (true) {
        sleep_ms(10);
    }
}
```

### Channel ownership (2.1)

Each output channel has an owner — `kOutputsOwnerManual` (the default) or `kOutputsOwnerAudio`. Manual writes via `set_voltage_*` work on `kOutputsOwnerManual` channels and are rejected (`false` return, no-op) on channels that `AudioProcessor` has claimed as audio. Use `get_channel_owner(channel)` to inspect the current owner.

In normal usage you don't manage ownership directly — `AudioProcessor` claims channels when `init_audio_processor` / `init_audio_processor_v2` is called and releases them on `stop()`. You only need to know that:

- `set_voltage_*` works on a channel until audio claims it.
- The unclaimed channel always works for manual CV, even while audio is running on the other.

### Audio/CV output API

#### Initialization and wiring
- `bool init_audio_cv(...)`
  Initializes DAC/SPI path and output range control pins.
- `bool init()`
  Initializes both audio/CV and pulse output paths.
- `bool is_audio_cv_initialized() const`
  Returns whether audio/CV path is initialized.
- `bool is_initialized() const`
  Returns whether module-level init completed.

#### Voltage write methods
- `bool set_voltage_millivolts(AudioCvOutChannel channel, int32_t millivolts)`
  Writes voltage in mV. Returns `false` for out-of-range values, or when the channel is currently audio-owned (`AudioProcessor` claimed it).
- `bool set_voltage_calibrated_millivolts(AudioCvOutChannel channel, int32_t target_millivolts)`
  Writes voltage using calibration offsets when available/enabled. Same ownership rules as `set_voltage_millivolts`.

#### Channel ownership inspection
- `AudioCvOutOwner get_channel_owner(AudioCvOutChannel channel) const`
  Returns `kOutputsOwnerManual` or `kOutputsOwnerAudio`.
- `void set_channel_owner(AudioCvOutChannel channel, AudioCvOutOwner owner)`
  Manually changes ownership. Normally not needed — `AudioProcessor` does this for you. Use it only if you're writing a custom audio path bypassing `AudioProcessor`.

#### Range control
- `bool set_output_range(AudioCvOutChannel channel, AudioCvOutRange range)`
  Sets channel range (`0..10V` or `-5..+5V`).
- `AudioCvOutRange get_output_range(AudioCvOutChannel channel) const`
  Returns current range for a channel.

#### Calibration methods
- `void set_dependencies(Storage* storage)`
  Provides storage dependency for loading/saving calibration context.
- `bool set_calibration(const CvCalibrationV1& cal)`
  Applies calibration table in memory.
- `void clear_calibration()`
  Clears current calibration state.
- `bool load_calibration_from_flash()`
  Loads calibration payload from storage.
- `bool has_calibration() const`
  Returns whether calibration is currently loaded.

#### Diagnostics / state
- `uint16_t get_last_dac_value(AudioCvOutChannel channel) const`
  Returns last DAC code written to channel.
- `int32_t get_last_set_millivolts(AudioCvOutChannel channel) const`
  Returns last requested millivolt value for channel.

#### Legacy alias
- `using AudioCvOut = Outputs;`
  Type alias kept for compatibility with older naming.

---

## Pulse output
Pulse output is a digital output lane for gate/trigger style signaling.
This is separate from audio/CV channels and is controlled as boolean state.

### Example
```cpp
#define BRAIN_USE_STORAGE 1
#define BRAIN_USE_OUTPUTS 1
#include "brain/brain.h"

#include <pico/stdlib.h>

Brain brain;

int main() {
    stdio_init_all();

    BrainInitStatus status = brain.init_outputs();
    if (!brain_init_succeeded(status)) return 1;

    while (true) {
        brain.outputs.pulse_set(true);   // gate high
        sleep_ms(100);

        brain.outputs.pulse_set(false);  // gate low
        sleep_ms(100);
    }
}
```

### Pulse output API

#### Initialization
- `void init_pulse()`
  Initializes pulse output GPIO path.
- `bool is_pulse_initialized() const`
  Returns whether pulse path is initialized.

#### State control
- `void pulse_set(bool on)`
  Sets pulse output high/low.
- `bool pulse_get() const`
  Returns current pulse output state.

#### Combined module init
- `bool init()`
  Initializes pulse and audio/CV paths together (if you want one call).
