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

Anyone can write their own apps for the Brain module. For this we'll provide an SDK with easy access to the built in I/O and interface components. We'll also provide a boilerplate shell script which makes it easy for anyone to get started with a new app/firmware.

### Core Components

- [Button](docs/BUTTON.md)
- [LED](docs/LED.md)
- [Pots](docs/POT_MULTIPLEXER.md)
- [Pulse I/O](docs/PULSE.md)
- [MIDI parser](docs/MIDI_PARSER.md)


### Folder Structure
```
brain-sdk/
├── ai/            # Project rules, todos, and documentation
├── build/         # CMake build output
├── cmake/         # CMake helper scripts
├── docs/          # Documentation and conventions
├── lib/           # Reusable libraries (e.g. brain-io, brain-ui)
├── pico-sdk/      # Pico SDK (as a git submodule)
├── scripts/       # Helper scripts (e.g. new-program.sh)
```

## Development
See [SETUP](docs/SETUP.md) for setup instructions, prerequisites, and workflow details.
