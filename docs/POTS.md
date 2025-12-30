# Pots Component

## Overview
The Pots component enables reading multiple potentiometers using a single Pico ADC channel via a 74HC4051 analog multiplexer. It manages channel selection, settling time, value scaling, and change detection with configurable thresholds.

## Features
- Supports up to 4 potentiometers on one ADC input
- Automatic multiplexer channel switching and settling
- Configurable output resolution (e.g., 7-bit for 0-127 range)
- Multi-sample averaging for stable readings
- Change detection with configurable threshold
- Simple mode for basic use cases
- Event callbacks for value changes
- Both scaled and raw ADC value access

## Usage

### Basic Setup
1. **Configuration**: Create a config using `create_default_config()` or customize your own
2. **Initialization**: Create a `Pots` instance and call `init()` with your config
3. **Polling**: Call `scan()` regularly in your main loop
4. **Access**: Use `get(index)` to read scaled values or `get_raw(index)` for raw ADC
5. **Callbacks**: Register a callback with `set_on_change()` for change notifications

### Example
```cpp
#include "brain-ui/pots.h"

// Create with default Brain module configuration (3 pots, 7-bit resolution)
brain::ui::Pots pots;
auto config = brain::ui::create_default_config(3, 7);
pots.init(config);

// Set callback for pot changes
pots.set_on_change([](uint8_t pot_index, uint16_t value) {
    printf("Pot %d changed to %d\n", pot_index, value);
});

while (true) {
    pots.scan();  // Check for changes and trigger callbacks

    // Read current values (0-127 with 7-bit resolution)
    uint16_t pot0 = pots.get(0);
    uint16_t pot1 = pots.get(1);
    uint16_t pot2 = pots.get(2);

    // Or get raw 12-bit ADC values (0-4095)
    uint16_t raw = pots.get_raw(0);
}
```

### Custom Configuration
```cpp
brain::ui::PotsConfig custom_config;
custom_config.simple = false;
custom_config.adc_gpio = 26;
custom_config.s0_gpio = 14;
custom_config.s1_gpio = 15;
custom_config.num_pots = 3;
custom_config.channel_map[0] = 0;
custom_config.channel_map[1] = 1;
custom_config.channel_map[2] = 2;
custom_config.output_resolution = 7;  // 0-127 range
custom_config.settling_delay_us = 200;  // 200µs settling time
custom_config.samples_per_read = 4;  // Average 4 samples
custom_config.change_threshold = 4;  // Minimum change to trigger callback

pots.init(custom_config);
```

## Configuration Options

### PotsConfig Structure
- `simple` - Enable simple mode for basic operation
- `adc_gpio` - ADC GPIO pin (typically 26-29)
- `s0_gpio`, `s1_gpio` - Multiplexer select line GPIOs
- `num_pots` - Number of active potentiometers (1-4)
- `channel_map` - Logical-to-physical channel mapping array
- `output_resolution` - Output resolution in bits (e.g., 7 for 0-127)
- `settling_delay_us` - Settling time after channel change in microseconds
- `samples_per_read` - Number of samples to average per reading
- `change_threshold` - Minimum change to trigger callback

### Runtime Configuration
You can update configuration at runtime:
```cpp
pots.set_simple(true);
pots.set_output_resolution(8);  // Change to 0-255 range
pots.set_settling_delay_us(250);
pots.set_samples_per_read(8);
pots.set_change_threshold(2);
```

## Notes
- Designed for Eurorack potentiometers (10k-100k typical)
- Default settling time is 200µs for stable readings
- Change threshold prevents noise-triggered callbacks
- Avoid long operations in callbacks to maintain responsiveness
- The Brain module has 3 potentiometers (using channels 0-2)
