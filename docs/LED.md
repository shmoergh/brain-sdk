# LED Component

## Overview
The LED component manages individual LEDs in the Brain module, supporting brightness control via PWM, multiple blinking patterns, and event-driven callbacks.

## Features
- Controls LED state (on/off/toggle)
- Adjustable brightness using Pico PWM (0-255)
- Multiple blinking modes:
  - Count-based blinking (blink N times)
  - Duration-based blinking (blink for N milliseconds)
  - Continuous blinking (until stopped)
- Event callbacks for state changes and blink completion
- Status queries (is_on, is_blinking)

## Usage
1. **Initialization**: Create an `Led` instance with GPIO pin, then call `init()`
2. **Control**: Use methods to set brightness, turn on/off, toggle, or start blinking
3. **Polling**: Call `update()` in the main loop for blink timing
4. **Callbacks**: Register callbacks for state changes or blink completion

## Example - Basic Control
```cpp
#include "brain-ui/led.h"

brain::ui::Led status_led(10);  // GPIO 10
status_led.init();

status_led.set_brightness(128);  // 50% brightness
status_led.on();                 // Turn on
status_led.toggle();             // Toggle state
status_led.off();                // Turn off

while (true) {
    status_led.update();
}
```

## Example - Blinking Patterns
```cpp
#include "brain-ui/led.h"

brain::ui::Led led(10);
led.init();

// Blink 5 times with 200ms interval
led.blink(5, 200);

// Blink for 2 seconds with 100ms interval
led.blink_duration(2000, 100);

// Start continuous blinking with 500ms interval
led.start_blink(500);

// Later, stop blinking
led.stop_blink();

while (true) {
    led.update();  // Required for blink timing
}
```

## Example - Callbacks
```cpp
#include "brain-ui/led.h"

brain::ui::Led led(10);
led.init();

// Callback when LED state changes (on/off)
led.set_on_state_change([](bool is_on) {
    printf("LED is now: %s\n", is_on ? "ON" : "OFF");
});

// Callback when blink sequence completes
led.set_on_blink_end([]() {
    printf("Blink sequence finished\n");
});

led.blink(3, 250);  // Blink 3 times, callbacks will be triggered

while (true) {
    led.update();
}
```

## Example - Status Queries
```cpp
#include "brain-ui/led.h"

brain::ui::Led led(10);
led.init();
led.start_blink(500);

while (true) {
    led.update();

    if (led.is_on()) {
        // LED is currently on
    }

    if (led.is_blinking()) {
        // LED is in a blink pattern
    }
}
```

## API Reference

### Constructor
- `Led(uint gpio_pin)` - Create LED on specified GPIO pin

### Initialization
- `void init()` - Initialize GPIO and PWM for the LED

### Basic Control
- `void on()` - Turn LED on at current brightness
- `void off()` - Turn LED off (0% brightness)
- `void toggle()` - Toggle LED state
- `void set_brightness(uint8_t value)` - Set brightness (0-255)

### Blinking
- `void blink(uint times, uint interval_ms)` - Blink N times with interval
- `void blink_duration(uint duration_ms, uint interval_ms)` - Blink for duration
- `void start_blink(uint interval_ms)` - Start continuous blinking
- `void stop_blink()` - Stop any active blinking

### Update
- `void update()` - Update LED state and handle timing (call in main loop)

### Callbacks
- `void set_on_state_change(std::function<void(bool)> callback)` - Called when LED changes state
- `void set_on_blink_end(std::function<void()> callback)` - Called when blink sequence ends

### Status
- `bool is_on() const` - Check if LED is currently on
- `bool is_blinking() const` - Check if LED is in a blink pattern

## Notes
- Designed for transistor-driven LEDs (Eurorack compatible)
- Must call `update()` regularly for blinking to work
- Brightness uses PWM hardware
- Avoid blocking operations in callbacks
- Multiple LEDs can be managed independently
- For managing all 6 Brain module LEDs together, see [Leds](LEDS.md)
