# Brain SDK docs

## What is the Brain SDK

The Brain SDK is a firmware and library collection for the Brain module of the [Shmøergh Moduleur](https://www.shmoergh.com/moduleur), designed to accelerate embedded development. It provides reusable components for I/O, controls (buttons, LEDs), MIDI, DSP, and more, along with example programs and scripts to streamline project setup and prototyping.

## Documentation

- [Installation](./INSTALLATION.md)
- [Getting Started](./GETTING_STARTED.md)
- [2.0 Migration](./2.0_MIGRATION.md)

#### Components

- [Brain Wrapper](./BRAIN.md) 👈 Start here
- [Pots](./components/POTS.md)
- [Buttons](./components/BUTTON.md)
- [Button LED](./components/BUTTON_LED.md)
- [LED strip](./components/LEDS.md)
- [Inputs](./INPUTS.md)
- [Outputs](./OUTPUTS.md)

#### Utilities

- [MIDI parser](./utilities/MIDI_PARSER.md) — Parse incoming MIDI data
- [MIDI to CV converter](./utilities/MIDI_TO_CV.md) — Convert MIDI to control voltage
- [Storage](./utilities/STORAGE.md) — Persisting CV calibration and firmware data
- [Audio Processor](./utilities/AUDIO_PROCESSOR.md) — Using DMA for audio inputs and pot reading and rendering audio output at a fixed rate