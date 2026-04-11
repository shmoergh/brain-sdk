# Examples Firmware (`examples`)

Purpose: menu-driven runnable examples based on SDK docs and common integration patterns.

The `examples` firmware works like `test`:
- flash once
- open serial terminal
- select an example from startup menu in `examples/main.cpp`

After an example starts, it owns the loop. Reset the board to return to the menu.

## Included example apps
- `LedsExample` (`apps/leds_example.cpp`)
- `InputsExample` (`apps/inputs_example.cpp`)
- `IoPassthroughExample` (`apps/io_passthrough_example.cpp`)
- `PotsMultiExample` (`apps/pots_multi_example.cpp`)
- `MidiParserExample` (`apps/midi_parser_example.cpp`)
- `MidiToCvExample` (`apps/midi_to_cv_example.cpp`)
- `StorageExample` (`apps/storage_example.cpp`)
- `AudioProcessorExample` (`apps/audio_processor_example.cpp`)

## Build

```bash
cmake -S . -B build-rp2040 -DPICO_BOARD=pico -DPICO_PLATFORM=rp2040
cmake --build build-rp2040 --target examples -j4

cmake -S . -B build-rp2350 -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s
cmake --build build-rp2350 --target examples -j4
```

## VSCode debug launch configs

- `Examples - RP2040`
- `Examples - RP2350`
