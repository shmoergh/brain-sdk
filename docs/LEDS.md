# Leds Component

## Overview
The Leds component provides a high-level interface for managing all 6 LEDs in the Brain module as a group. It simplifies common operations like controlling multiple LEDs simultaneously, animations, and coordinated blinking patterns.

## Features
- Manages all 6 Brain module LEDs as a group
- Individual LED control (on/off/toggle/brightness)
- Multi-LED operations (all on/off, set from bitmask)
- Blinking support with duration and interval control
- Built-in startup animation
- Status queries (is_on, is_blinking)
- Automatic initialization and update handling

## Usage

### Basic Setup
1. **Initialization**: Create a `Leds` instance and call `init()`
2. **Update Loop**: Call `update()` regularly in your main loop to handle blinking
3. **Control**: Use methods to control individual or multiple LEDs

### Example - Basic Control
```cpp
#include "brain-ui/leds.h"

brain::ui::Leds leds;
leds.init();

// Turn on individual LEDs
leds.on(0);
leds.on(1);

// Set brightness (0-255)
leds.set_brightness(2, 128);  // LED 2 at 50% brightness

// Toggle LED
leds.toggle(3);

// Turn off all LEDs
leds.off_all();

while (true) {
    leds.update();  // Required for blinking functionality
}
```

### Example - Blinking
```cpp
#include "brain-ui/leds.h"

brain::ui::Leds leds;
leds.init();

// Blink LED 0 for 500ms with 250ms interval
leds.blink_duration(0, 500, 250);

// Start continuous blinking on LED 1 (500ms interval)
leds.start_blink(1, 500);

// Later, stop blinking
leds.stop_blink(1);

while (true) {
    leds.update();  // Processes blink timing
}
```

### Example - Multi-LED Control
```cpp
#include "brain-ui/leds.h"

brain::ui::Leds leds;
leds.init();

// Turn on all LEDs
leds.on_all();

// Set LEDs using bitmask (bit 0 = LED 0, bit 1 = LED 1, etc.)
// Example: 0b00101010 turns on LEDs 1, 3, and 5
leds.set_from_mask(0b00101010);

// Check LED status
if (leds.is_on(0)) {
    // LED 0 is on
}

if (leds.is_blinking(1)) {
    // LED 1 is blinking
}
```

### Example - Startup Animation
```cpp
#include "brain-ui/leds.h"

brain::ui::Leds leds;
leds.init();

// Play startup animation
leds.startup_animation();

while (true) {
    leds.update();
}
```

## API Reference

### Initialization
- `void init()` - Initialize all 6 LEDs
- `void update()` - Update LED states (call in main loop)

### Single LED Methods
- `void on(uint8_t led)` - Turn on LED (0-5)
- `void off(uint8_t led)` - Turn off LED (0-5)
- `void toggle(uint8_t led)` - Toggle LED state (0-5)
- `void set_brightness(uint8_t led, uint8_t brightness)` - Set LED brightness (0-255)
- `void blink_duration(uint8_t led, uint duration_ms, uint interval_ms)` - Blink for duration
- `void start_blink(uint8_t led, uint interval_ms)` - Start continuous blinking
- `void stop_blink(uint8_t led)` - Stop blinking

### Multi-LED Methods
- `void set_from_mask(uint8_t mask)` - Set LEDs from 6-bit bitmask
- `void on_all()` - Turn on all LEDs
- `void off_all()` - Turn off all LEDs

### Status Queries
- `bool is_on(uint8_t led)` - Check if LED is on
- `bool is_blinking(uint8_t led)` - Check if LED is blinking

### Animations
- `void startup_animation()` - Play built-in startup animation

## Notes
- LED indices are 0-5 (corresponding to the 6 LEDs on Brain module)
- LEDs are transistor-driven for Eurorack compatibility
- Brightness control uses PWM
- `update()` must be called regularly for blinking to work
- The bitmask in `set_from_mask()` uses bits 0-5 (LSB to MSB)
- Invalid LED indices are validated internally and ignored

## Brain Module LED Pins
The Leds component automatically manages the following GPIO pins:
- LED 1: GPIO defined in `GPIO_BRAIN_LED_1`
- LED 2: GPIO defined in `GPIO_BRAIN_LED_2`
- LED 3: GPIO defined in `GPIO_BRAIN_LED_3`
- LED 4: GPIO defined in `GPIO_BRAIN_LED_4`
- LED 5: GPIO defined in `GPIO_BRAIN_LED_5`
- LED 6: GPIO defined in `GPIO_BRAIN_LED_6`

These are defined in `brain-common/brain-common.h`.
