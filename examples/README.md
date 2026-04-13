# Examples Firmware (`examples`)

Purpose: runnable examples based on SDK docs and common integration patterns.

The `examples` firmware runs one selected app directly from `examples/main.cpp`.

To switch examples, edit [main.cpp](./main.cpp):
- replace the `#include "apps/..._example.h"` line
- replace the `...Example app;` declaration

## Included example apps
- `apps/leds_example.h` -> `LedsExample`
- `apps/inputs_example.h` -> `InputsExample`
- `apps/io_passthrough_example.h` -> `IoPassthroughExample`
- `apps/pots_multi_example.h` -> `PotsMultiExample`
- `apps/midi_parser_example.h` -> `MidiParserExample`
- `apps/midi_to_cv_example.h` -> `MidiToCvExample`
- `apps/storage_example.h` -> `StorageExample`
- `apps/audio_processor_example.h` -> `AudioProcessorExample`

Example:

```cpp
#include "apps/audio_processor_example.h"
AudioProcessorExample app;
```

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
