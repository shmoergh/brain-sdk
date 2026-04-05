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

It also provides compile-time feature selection and centralized init/update helpers.
`init_all()` initializes all enabled core components (`leds`, `buttons`, `outputs`, `inputs`, `pots`, `midi_parser`) but not utilities such as `midi_to_cv`.

## Include
```cpp
#include "brain/brain.h"
```

## Quick Start
```cpp
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
Brain brain;
brain.init_leds();
brain.init_outputs();
brain.init_midi_to_cv(AudioCvOutChannel::kChannelA, 1); // auto-inits outputs + midi_parser
```

## Compile-Time Feature Selection
Use `BrainT<...>` flags:
- `kBrainFeatureLeds`
- `kBrainFeatureButtons`
- `kBrainFeatureOutputs`
- `kBrainFeatureInputs`
- `kBrainFeaturePots`
- `kBrainFeatureMidiParser`
- `kBrainFeatureMidiToCv` (requires `kBrainFeatureOutputs` + `kBrainFeatureMidiParser`)

Examples:
- `using BrainIO = BrainT<kBrainFeatureInputs | kBrainFeatureOutputs>;`
- `using BrainUI = BrainT<kBrainFeatureLeds | kBrainFeatureButtons | kBrainFeaturePots>;`
- `using BrainWithMidiParser = BrainT<kBrainFeatureMidiParser>;`
- `using BrainWithMidiToCv = BrainT<kBrainFeatureOutputs | kBrainFeatureMidiParser | kBrainFeatureMidiToCv>;`
- `using BrainMinimal = BrainT<0>;`

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

## ADC Policy Controls
`Brain` can configure ADC optimization policy:
- `enable_adc_optimization(bool)`
- `set_audio_cv_dma_enabled(bool)`
- `set_shared_pot_sampling_enabled(bool)`

Apply policy before calling `init_*()` for deterministic startup behavior.
