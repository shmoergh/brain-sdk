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

### [👉 SDK Docs](./docs/)

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
