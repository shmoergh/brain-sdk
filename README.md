# Brain SDK Overview

The Brain SDK is a firmware and library collection for the Brain module of the [Shmøergh Moduleur](https://www.shmoergh.com/moduleur), designed to accelerate embedded development. It provides reusable components for I/O, UI, DSP, and more, along with example programs and scripts to streamline project setup and prototyping.

## Features
Brain uses a Raspberry Pi Pico or Pico 2 (depending on what the builder chose to connect) in it's Core board. It's a universal digital module for Eurorack synths with flexible inputs and outputs.
- 2 analog inputs for CV and audio signals using 2 of the the Pico's ADCs
- 2 analog outputs which uses an MCP4822 DAC. The output of the DAC can be programmatically AC or DC coupled, using a CD4053 IC.
- 1 pulse input, using digital GPIO, preceded by a transistor switch (for safety)
- 1 pulse output, using digital GPIO with a simple transistor switch
- 1 dedicated MIDI input. This input is not universal because MIDI requires special input circuitry

Additionally there are some controls on the UI:
- 3 potmeters, multiplexed, using the third ADC of the Pico
- 2 pushbuttons
- 6 LEDs. The power for the LEDs are taken from the external power supply (using transistors) because of the Pico's limited power capabilities. The LEDs brightness can be set with PWM output from the Pico

As the Pico and Pico2 are pin compatible, the newer version can be used for heavier programs like effects or DSP.

## SDK

Anyone can write their own apps for the Brain module. The SDK provides easy access to all built-in I/O and interface components, along with utility classes for common tasks. A boilerplate shell script makes it easy to get started with new apps/firmware.

### Core Components

#### I/O Components (`brain::io`)
- [Audio/CV Input](docs/AUDIO_CV_IN.md) - Two-channel analog input with voltage conversion
- [Audio/CV Output](docs/AUDIO_CV_OUT.md) - Two-channel DAC output with DC/AC coupling
- [Pulse I/O](docs/PULSE.md) - Digital pulse input/output for gates and triggers
- [MIDI Parser](docs/MIDI_PARSER.md) - UART-based MIDI input with message parsing

#### UI Components (`brain::ui`)
- [Button](docs/BUTTON.md) - Debounced pushbutton input with callbacks
- [LED](docs/LED.md) - Individual LED control with PWM brightness
- [Leds](docs/LEDS.md) - Group LED controller for all 6 Brain module LEDs
- [Pots](docs/POTS.md) - Multiplexed potentiometer reader

#### Utilities (`brain::utils`)
- [MIDI to CV](docs/MIDI_TO_CV.md) - Complete MIDI-to-CV converter with note priority
- [Utilities](docs/UTILITIES.md) - RingBuffer and helper functions (map, clamp)


### Folder Structure
```
brain-sdk/
├── build/         # CMake build output
├── cmake/         # CMake helper scripts
├── docs/          # Documentation and conventions
├── lib/           # Reusable libraries
│   ├── brain-common/    # Common definitions and GPIO setup
│   ├── brain-io/        # I/O components (ADC, DAC, MIDI, Pulse)
│   ├── brain-ui/        # UI components (Button, LED, Leds, Pots)
│   └── brain-utils/     # Utilities (MIDI to CV, RingBuffer, helpers)
├── pico-sdk/      # Pico SDK (as a git submodule)
├── programs/      # Firmware applications
├── scripts/       # Helper scripts (e.g. new-program.sh)
└── sdk_test/      # SDK test programs
```

## Development
See [SETUP](docs/SETUP.md) for setup instructions, prerequisites, and workflow details.
