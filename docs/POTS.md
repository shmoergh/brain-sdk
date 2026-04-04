# Pots Component

## Overview
`Pots` reads up to 4 potentiometers through a mux (Brain uses 3).

## Include
```cpp
#include "brain/include/pots.h"
#include "brain/include/pot-multi-function-core.h"
```

## Quick Start
```cpp
Pots pots;
auto cfg = create_default_pots_config(3, 7); // 3 pots, 7-bit output
pots.init(cfg);

while (true) {
	pots.scan();
	uint16_t p0 = pots.get_buffered(0);
	(void)p0;
}
```

## Pots API
- `void init(const PotsConfig& cfg)`
- `void scan()`
- `uint16_t get(uint8_t index)`
- `uint16_t get_buffered(uint8_t index) const`
- `uint16_t get_raw(uint8_t index)`
- `void set_on_change(std::function<void(uint8_t, uint16_t)> cb)`
- `void set_simple(bool)`
- `void set_optimized_sampling_enabled(bool)`
- `bool is_optimized_sampling_enabled() const`
- `void set_output_resolution(uint8_t)`
- `void set_settling_delay_us(uint32_t)`
- `void set_samples_per_read(uint8_t)`
- `void set_change_threshold(uint16_t)`

## PotMultiFunction
Use `PotMultiFunction` when one physical pot controls multiple logical parameters.

Modes:
- `PotMode::kDirect`
- `PotMode::kPickup`
- `PotMode::kValueScale`

Typical flow:
1. register function configs
2. set active function per pot each frame
3. call `update(...)` or `update_buffered(...)`
4. read function value with `get_value(function_id)`
