# Brain Class

## Overview
`Brain` is the top-level wrapper that groups SDK components:
- `leds`
- `buttons`
- `outputs`
- `inputs`
- `pots`
- `midi_parser`
- `midi_to_cv`
- `pot_multi`

It also provides compile-time feature selection and centralized init/update helpers.
`init_all()` initializes all enabled core components (`leds`, `buttons`, `outputs`, `inputs`, `pots`, `midi_parser`) but not utilities such as `midi_to_cv`.

## Include
```cpp
#define BRAIN_USE_ALL 1
#include "brain/brain.h"
```

## Quick Start
```cpp
#define BRAIN_USE_ALL 1
#include "brain/brain.h"

Brain brain;

int main() {
	BrainInitStatus status = brain.init_all();
	if (!brain_init_succeeded(status)) {
		return 1;
	}

	while (true) {
		brain.update_all();
	}
}
```

## Component Access
```cpp
brain.leds.on(0);
brain.outputs.set_output_range(AudioCvOutChannel::kChannelA, AudioCvOutRange::kRange0To10V);
brain.outputs.set_voltage_millivolts(AudioCvOutChannel::kChannelA, 2500);
```

## Selective Init
```cpp
#define BRAIN_USE_ALL 1
#include "brain/brain.h"

Brain brain;
brain.init_leds();
brain.init_outputs();
brain.init_midi_to_cv(AudioCvOutChannel::kChannelA, 1); // auto-inits outputs + midi_parser
```

Important:
- `Selective Init` is runtime behavior only.
- If you use `Brain`, all components/utilities are still compiled in.
- `init_*()` controls what gets initialized, not what gets compiled.

When to use this:
- Use selective init if you need runtime control (for example different app modes initialize different modules).
- Use `init_all()` if you want predictable startup with minimal integration code.

## Compile-Time Feature Selection
Use `BRAIN_USE_*` macros before including `brain/brain.h`:
- `BRAIN_USE_ALL`
- `BRAIN_USE_LEDS`
- `BRAIN_USE_BUTTONS`
- `BRAIN_USE_OUTPUTS`
- `BRAIN_USE_INPUTS`
- `BRAIN_USE_POTS`
- `BRAIN_USE_MIDI_PARSER`
- `BRAIN_USE_MIDI_TO_CV`
- `BRAIN_USE_POT_MULTI_FUNCTION`

Rules:
- At least one macro must be defined (explicit config required).
- `BRAIN_USE_ALL=1` enables all modules explicitly.
- `BRAIN_USE_MIDI_TO_CV=1` requires `BRAIN_USE_OUTPUTS=1` and `BRAIN_USE_MIDI_PARSER=1`.
- `BRAIN_USE_POT_MULTI_FUNCTION=1` requires `BRAIN_USE_POTS=1`.

Recommended shared-config pattern (multi-file firmware):
```cpp
// brain_user_config.h
#pragma once
#define BRAIN_USE_LEDS 1
#define BRAIN_USE_OUTPUTS 1
#define BRAIN_USE_MIDI_PARSER 1
#define BRAIN_USE_MIDI_TO_CV 1
#define BRAIN_USE_POT_MULTI_FUNCTION 1
```

```cpp
// any source file using Brain
#include "brain_user_config.h"
#include "brain/brain.h"

Brain brain;
```

Equivalent CMake-based config:
```cmake
target_compile_definitions(my_firmware PRIVATE
    BRAIN_USE_LEDS=1
    BRAIN_USE_OUTPUTS=1
)
```

When to use this:
- Use `BRAIN_USE_ALL=1` for simplest startup with explicit intent.
- Use selective `BRAIN_USE_*` macros when flash/RAM budget matters or you want strict module ownership.
- Use inline defines only for small/single-file experiments.

## Init Status and Idempotency
All `init_*` calls are idempotent and return:
- `BrainInitStatus::kOk`
- `BrainInitStatus::kAlreadyInitialized`
- `BrainInitStatus::kFailed`

State query helpers:
- `is_leds_initialized()`
- `is_buttons_initialized()`
- `is_outputs_initialized()`
- `is_inputs_initialized()`
- `is_pots_initialized()`
- `is_midi_parser_initialized()`
- `is_midi_to_cv_initialized()`
- `is_pot_multi_initialized()`

Pot utility init/update:
- `init_pot_multi(...)` (auto-inits `pots` if needed)
- `update_pot_multi(...)` (buffered default)
- `update_pot_multi_single()` (explicit direct read path)
- `reset_pot_multi_for_mode_change(...)`

Pots lifecycle:
- `init_pots(...)` initializes once (idempotent).
- `reconfigure_pots(...)` explicitly reapplies a pots profile after mode switch.
- `reconfigure_pots(...)` can also reset `pot_multi` runtime state so stale profile assumptions do not leak across modes.

Sampling semantics:
- `Pots::get()` is the safe default and returns buffered values.
- `Pots::get_single()` is the direct/single-read path.
- `PotMultiFunction::update()` is buffered by default.
- `PotMultiFunction::update_single()` is the direct path.

## ADC Policy Controls
`Brain` can configure ADC optimization policy:
- `enable_adc_optimization(bool)`
- `set_audio_cv_dma_enabled(bool)`
- `set_shared_pot_sampling_enabled(bool)`

Apply policy before calling `init_*()` for deterministic startup behavior.
