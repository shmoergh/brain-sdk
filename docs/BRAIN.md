# Brain Class

## Overview
`Brain` is the top-level wrapper that groups SDK components:
- `leds`
- `buttons`
- `outputs`
- `inputs`
- `pots`

It also provides compile-time feature selection and centralized init/update helpers.

## Include
```cpp
#include "brain/brain.h"
```

## Quick Start
```cpp
Brain brain;

int main() {
	brain.init_all();

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
```

## Compile-Time Feature Selection
Use `BrainT<...>` flags:
- `kBrainFeatureLeds`
- `kBrainFeatureButtons`
- `kBrainFeatureOutputs`
- `kBrainFeatureInputs`
- `kBrainFeaturePots`

Examples:
- `using BrainIO = BrainT<kBrainFeatureInputs | kBrainFeatureOutputs>;`
- `using BrainUI = BrainT<kBrainFeatureLeds | kBrainFeatureButtons | kBrainFeaturePots>;`
- `using BrainMinimal = BrainT<0>;`

## ADC Policy Controls
`Brain` can configure ADC optimization policy:
- `enable_adc_optimization(bool)`
- `set_audio_cv_dma_enabled(bool)`
- `set_shared_pot_sampling_enabled(bool)`

Apply policy before calling `init_*()` for deterministic startup behavior.
