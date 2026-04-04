# Brain SDK Overview

The Brain SDK is a firmware and library collection for the Brain module of the [Shmøergh Moduleur](https://www.shmoergh.com/moduleur), designed to accelerate embedded development. It provides reusable components for I/O, UI, DSP, and more, along with example programs and scripts to streamline project setup and prototyping.

## Features
Brain uses a Raspberry Pi Pico or Pico 2 (depending on what the builder chose to connect) in it's Core board. It's a universal digital module for Eurorack synths with flexible inputs and outputs.
- 2 analog inputs for CV and audio signals using 2 of the the Pico's ADCs
- 2 analog outputs which uses an MCP4822 DAC. Output range can be set per channel to `0..10V` or `-5..+5V` (hardware offset path via CD4053).
- 1 pulse input, using digital GPIO, preceded by a transistor switch (for safety)
- 1 pulse output, using digital GPIO with a simple transistor switch
- 1 dedicated MIDI input. This input is not universal because MIDI requires special input circuitry

Additionally there are some controls on the UI:
- 3 potmeters, multiplexed, using the third ADC of the Pico
- 2 pushbuttons
- 6 LEDs. The power for the LEDs are taken from the external power supply (using transistors) because of the Pico's limited power capabilities. LED control supports both PWM brightness and simple GPIO on/off mode.

As the Pico and Pico2 are pin compatible, the newer version can be used for heavier programs like effects or DSP.

## SDK

Anyone can write their own apps for the Brain module. The SDK provides easy access to all built-in I/O and interface components, along with utility classes for common tasks. A boilerplate shell script makes it easy to get started with new apps/firmware.

### Core Components

#### I/O Components
- [Inputs](docs/INPUTS.md) - Audio/CV input + pulse input in one class
- [Outputs](docs/OUTPUTS.md) - Audio/CV output + pulse output in one class
- [MIDI Parser](docs/MIDI_PARSER.md) - UART-based MIDI input with message parsing

#### UI Components
- [Button](docs/BUTTON.md) - Debounced pushbutton input with callbacks
- [Button LED](docs/BUTTON_LED.md) - Button LED control via `Leds::button_*` methods
- [LED Channel](docs/LED.md) - Per-channel LED control via `Leds`
- [Leds](docs/LEDS.md) - Group LED controller for all 6 Brain module LEDs
- [Pots](docs/POTS.md) - Multiplexed potentiometer reader

#### Utilities
- [MIDI to CV](docs/MIDI_TO_CV.md) - Complete MIDI-to-CV converter with note priority
- [Utilities](docs/UTILITIES.md) - RingBuffer and helper functions (map, clamp)

#### Storage
- [Storage](docs/STORAGE.md) - Reserved flash layout, calibration persistence, and app blob APIs
- [Storage Release Notes](docs/RELEASE_NOTES_STORAGE.md) - API additions, migration requirements, and validation summary

#### Flat API Entry Point
- Include `brain/brain.h`
- Use `Brain` (all features enabled) or `BrainT<...>` for compile-time feature selection
- Available feature flags: `kBrainFeatureLeds`, `kBrainFeatureButtons`, `kBrainFeatureInputs`, `kBrainFeatureOutputs`, `kBrainFeaturePots`
- Presets: `BrainAll`, `BrainIO`, `BrainUI`, `BrainMinimal`
- No namespace qualification is required for new code
- Full wrapper reference: [Brain class docs](docs/BRAIN.md)

Example:
```cpp
#include "brain/brain.h"

// Full wrapper (all components compiled in)
Brain brain_full;

// Compile-time reduced wrapper: only LEDs + outputs
using BrainLedsOut = BrainT<kBrainFeatureLeds | kBrainFeatureOutputs>;
BrainLedsOut brain;

int main() {
	brain.init_leds(LedMode::kSimple);
	brain.init_outputs();

	brain.leds.on(0);
	brain.outputs.set_output_range(AudioCvOutChannel::kChannelA, AudioCvOutRange::kRange0To10V);
	brain.outputs.set_voltage_millivolts(AudioCvOutChannel::kChannelA, 2500);
	brain.outputs.pulse_set(true);

	while (true) {
		brain.update_leds();
	}
}
```

ADC optimization defaults:
- `Inputs` uses DMA burst sampling for AudioCV by default.
- `Pots` uses optimized multiplexed scan (settle + discard + averaged reads) by default.
- `Brain` coordinates and configures these policies.
- You can disable either path before `init()`:

```cpp
Brain brain;
brain.set_audio_cv_dma_enabled(false);           // fall back to Inputs internal ADC reads
brain.set_shared_pot_sampling_enabled(false);    // fall back to Pots internal scan reads
brain.init();
```

If your app does custom ADC work (for example on core1 or inside IRQ handlers), use `BrainAdcLockGuard` around ADC register/FIFO access so it is serialized with SDK components:

```cpp
#include "brain/brain.h"
#include <hardware/adc.h>

void my_custom_adc_read() {
	BrainAdcLockGuard adc_guard;
	adc_select_input(0);
	uint16_t raw = adc_read();
	(void)raw;
}
```


### Folder Structure
```
brain-sdk/
├── build/         # CMake build output (default)
├── build-rp2040/  # Optional board-specific build output
├── build-rp2350/  # Optional board-specific build output
├── brain/         # SDK module
│   ├── brain.h    # Main entry point (Brain wrapper)
│   ├── include/   # Component headers (core + utils)
│   └── src/       # Component implementations
├── docs/          # Documentation and conventions
├── pico-sdk/      # Pico SDK (as a git submodule)
├── scripts/       # Helper scripts (e.g. new-brain-app.sh)
├── test/          # Manual hardware test apps and docs
└── sandbox/       # Thin firmware wrapper for quick experiments
```

## Development
See [SETUP](docs/SETUP.md) for setup instructions, prerequisites, and workflow details.
