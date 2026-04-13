# Brain Class

`Brain` is the top-level wrapper class that groups SDK components. Use this to make sure that all modules are instantiated only once and initialized properly. You can access all modules and utilities through this class:

- `brain.buttons`
- `brain.pots`
- `brain.pot_multi`
- `brain.leds`
- `brain.outputs`
- `brain.inputs`
- `brain.midi_parser`
- `brain.midi_to_cv`
- `brain.storage`
- `brain.audio_processor`

It also provides compile-time feature selection and centralized init/update helpers.

- `init_all()` initializes all enabled core components (`leds`, `buttons`, `storage`, `outputs`, `inputs`, `pots`, `midi_parser`) but **not** utilities such as `midi_to_cv`, `pot_multi`, or `audio_processor`.

## How to use it

#### Step 1: enable Brain modules and include the `brain` wrapper

You can set which modules should the wrapper use by enabling them through a `#define` macro. The simplest is to set `#define BRAIN_USE_ALL 1` which creates all module instances in the `brain` class.

```cpp
#define BRAIN_USE_ALL 1
#include "brain/brain.h"
```

Or you can initialize modules selectively:

```cpp
#define BRAIN_USE_LEDS 1
#define BRAIN_USE_INPUTS 1
#define BRAIN_USE_OUTPUTS 1
#include "brain/brain.h"
```

Important:
- Selective init is runtime behavior only. For example calling only `init_leds()` (and not `init_inputs()`) does not remove inputs code from the binary it only means inputs is not initialized at runtime.
- If you use `BRAIN_USE_ALL 1`, all components/utilities are still compiled in. This means everything is included at compile time, even if you initialize only some parts during startup.
- `init_*()` controls what gets initialized, not what gets compiled.

This means there are two separate decisions:

1. Compile-time decision — What gets built into the firmware is decided by `BRAIN_USE_*` macros (or `BRAIN_USE_ALL`).
2. Runtime decision — What gets initialized/activated now is decided by `brain.init_*()` calls.

- Use selective init if you need runtime control (for example different app modes initialize different modules).
- Use `init_all()` if you want predictable startup with minimal integration code.

#### Step 2: instantiate the wrapper

```cpp
Brain brain;
```

#### Step 3: initialize modules

Initialize modules in your `main()` function. If you use all modules:

```cpp
BrainInitStatus status = brain.init_all();
if (!brain_init_succeeded(status)) return 1;
```

If you use modules individually:

```cpp
BrainInitStatus status;

status = brain.init_leds(kLedsModeSimple);
if (!brain_init_succeeded(status)) return 1;

status = brain.init_inputs();
if (!brain_init_succeeded(status)) return 1;
```

#### Step 4: `update` modules in main loop

Update modules (or all) in the main loop of your firmware. If you use all modules:

```cpp
while (true) {
	brain.update_all();
}
```

Or if you use individual modules:

```cpp
while (true) {
	brain.update_leds();
	brain.update_inputs();
}
```

#### Putting it together

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

## Using the modules

You can do things with the components of the `brain` wrapper through the wrapper's instances. For example:

```cpp
brain.leds.on(0);
brain.outputs.set_output_range(kOutputsChannelA, kOutputsRange0To10V);
brain.outputs.set_voltage_millivolts(kOutputsChannelA, 2500);
bool protected_layout = brain.storage.is_layout_protected();
(void)protected_layout;
```

## Component flags

Use `BRAIN_USE_*` macros before including `brain/brain.h`:
- `BRAIN_USE_ALL`
- `BRAIN_USE_LEDS`
- `BRAIN_USE_BUTTONS`
- `BRAIN_USE_OUTPUTS`
- `BRAIN_USE_INPUTS`
- `BRAIN_USE_POTS`
- `BRAIN_USE_STORAGE`
- `BRAIN_USE_MIDI_PARSER`
- `BRAIN_USE_MIDI_TO_CV`
- `BRAIN_USE_POT_MULTI_FUNCTION`
- `BRAIN_USE_AUDIO_PROCESSOR`

Rules:
- At least one macro must be defined (explicit config required).
- `BRAIN_USE_ALL=1` enables all modules explicitly.
- `BRAIN_USE_OUTPUTS=1` requires `BRAIN_USE_STORAGE=1`.
- `BRAIN_USE_MIDI_TO_CV=1` requires `BRAIN_USE_OUTPUTS=1` and `BRAIN_USE_MIDI_PARSER=1`.
- `BRAIN_USE_POT_MULTI_FUNCTION=1` requires `BRAIN_USE_POTS=1`.

If you want to use the wrapper class in multiple files, we recommended to create a shared-config file and include it in all your files:

```cpp
// brain_user_config.h
#pragma once
#define BRAIN_USE_LEDS 1
#define BRAIN_USE_STORAGE 1
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

For larger firmwares you can define the enable flags in `CMakeLists.txt`:

```cmake
target_compile_definitions(my_firmware PRIVATE
    BRAIN_USE_LEDS=1
    BRAIN_USE_STORAGE=1
    BRAIN_USE_OUTPUTS=1
)
```


## Initializing modules

You can call `init_*` functions multiple times, it won't re-initialize or break state (idempotent). The init function will return:

### Constants

- `kBrainInitStatusOk` — initialization successful
- `kBrainInitStatusAlreadyInitialized` — already initialized
- `kBrainInitStatusFailed` — failed to initialize

#### State query helpers

- `is_leds_initialized()`
- `is_buttons_initialized()`
- `is_storage_initialized()`
- `is_outputs_initialized()`
- `is_inputs_initialized()`
- `is_pots_initialized()`
- `is_midi_parser_initialized()`
- `is_midi_to_cv_initialized()`
- `is_pot_multi_initialized()`
- `is_audio_processor_initialized()`

#### Pot utility init/update

- `init_pot_multi(...)` (auto-inits `pots` if needed)
- `update_pot_multi(...)` (buffered default)
- `update_pot_multi_single()` (explicit direct read path)
- `reset_pot_multi_for_mode_change(...)`

#### Pots lifecycle

- `init_pots(...)` initializes once (idempotent).
- `reconfigure_pots(...)` explicitly reapplies a pots profile after mode switch.
- `reconfigure_pots(...)` can also reset `pot_multi` runtime state so stale profile assumptions do not leak across modes.

#### Audio utility init/status

- `init_audio_processor(const AudioProcessorConfig&, ProcessSampleFn, void* user_ctx = nullptr)`
- `is_audio_processor_initialized()`
- `brain.audio_processor.get_stats()`
- `brain.audio_processor.get_pot_raw_u8(index)`

## Audio Processor guardrails

`AudioProcessor` owns ADC/DMA + pot sampling when active, so `Brain` enforces runtime guardrails to make sure they don't conflict with other modules.

If `audio_processor` is already initialized:

- `init_inputs()` will fail
- `init_pots(...)` will fail
- `reconfigure_pots(...)` will fail
- `init_pot_multi(...)` will fail

The other way around, if `inputs`, `pots`, or `pot_multi` are already initialized:

- `init_audio_processor(...)` will fail

You can call these updates but they won't do anything (no-op):

- `update_inputs()` is a no-op when inputs are not initialized
- `update_pots()` is a no-op when pots are not initialized
- `update_pot_multi()` / `update_pot_multi_single()` remain no-op-safe when pot-multi is not initialized

## ADC behavior

`Brain` gives you switches for how ADC-based modules behave. These exist because `Inputs` and `Pots` both rely on ADC and there are optimized vs. fallback read modes. The controls are:

- `enable_adc_optimization(bool)` — Master switch, default: `true`. If `false`, the ADC functions below are effectively disabled.
- `set_audio_cv_dma_enabled(bool)` Controls whether `Inputs` uses DMA for sampling audio/CV. Default: `false`.
	- Enabling DMA just enables the mode; actual DMA setup is attempted during init/update, with fallback to non-DMA reads if setup fails.

- `set_shared_pot_sampling_enabled(bool)` — Controls whether `Pots` uses the optimized shared/buffered pot sampling path. Default: `true`.
	- When set `true` `Pots` uses its optimized/stable read path (more settling/averaging behavior). You usually get smoother, less jumpy knob values.
	- When `false`, `Pots` uses a simpler/faster path. Reads can be noisier or less stable, but behavior is more direct.

Set these flags before calling `init_*()`.
