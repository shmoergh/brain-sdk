# BUTTON Component

## Overview
The Button component provides a robust interface for pushbutton inputs in the Brain module. It handles debouncing, state tracking, and multiple event types including press, release, single tap, and long press detection.

## Features
- Software debouncing with configurable timing
- Multiple event types:
  - Press (immediate feedback)
  - Release
  - Single tap (quick press-release)
  - Long press (held beyond threshold)
- Configurable debounce and long press thresholds
- Pull-up or pull-down resistor support
- Event-driven callbacks
- Polling-based operation for main loop integration

## Usage
1. **Initialization**: Create a `Button` instance with GPIO pin and optional timing parameters
2. **Setup**: Call `init()` with pull-up or pull-down configuration
3. **Polling**: Call `update()` regularly in your main loop
4. **Callbacks**: Register callback functions for different event types

## Example - Basic Press/Release
```cpp
#include "brain-ui/button.h"

// Create button on GPIO 5 with default debounce (50ms) and long press (500ms)
brain::ui::Button my_button(5);
my_button.init(true);  // true = pull-up (button connects to GND)

my_button.set_on_press([]() {
    printf("Button pressed!\n");
});

my_button.set_on_release([]() {
    printf("Button released!\n");
});

while (true) {
    my_button.update();
    sleep_ms(1);
}
```

## Example - Long Press Detection
```cpp
#include "brain-ui/button.h"

// Create button with custom debounce (30ms) and long press (1000ms)
brain::ui::Button button(5, 30, 1000);
button.init(true);

button.set_on_press([]() {
    printf("Button pressed\n");
});

button.set_on_long_press([]() {
    printf("Long press detected!\n");
});

while (true) {
    button.update();
    sleep_ms(1);
}
```

## Example - Single Tap Detection
```cpp
#include "brain-ui/button.h"

brain::ui::Button button(5);
button.init(true);

// Single tap is triggered on quick press-release
button.set_on_single_tap([]() {
    printf("Quick tap!\n");
});

// Long press is triggered when held
button.set_on_long_press([]() {
    printf("Held down!\n");
});

while (true) {
    button.update();
    sleep_ms(1);
}
```

## Example - Pull-Down Configuration
```cpp
#include "brain-ui/button.h"

brain::ui::Button button(5);
button.init(false);  // false = pull-down (button connects to VCC)

button.set_on_press([]() {
    printf("Button pressed\n");
});

while (true) {
    button.update();
    sleep_ms(1);
}
```

## API Reference

### Constructor
```cpp
Button(uint gpio_pin, uint32_t debounce_ms = 50, uint32_t long_press_ms = 500)
```
- `gpio_pin`: GPIO pin number for button input
- `debounce_ms`: Debounce time in milliseconds (default: 50ms)
- `long_press_ms`: Long press threshold in milliseconds (default: 500ms)

### Initialization
```cpp
void init(bool pull_up = true)
```
- `pull_up`: true for pull-up resistor (button to GND), false for pull-down (button to VCC)
- Configures GPIO and internal resistor

### Update
```cpp
void update()
```
- Poll button state and trigger callbacks
- Must be called regularly in main loop for proper operation
- Handles debouncing and timing

### Callbacks
```cpp
void set_on_press(std::function<void()> callback)
```
- Called when button is pressed (after debounce)

```cpp
void set_on_release(std::function<void()> callback)
```
- Called when button is released (after debounce)

```cpp
void set_on_single_tap(std::function<void()> callback)
```
- Called for quick press-release cycles (not long press)

```cpp
void set_on_long_press(std::function<void()> callback)
```
- Called when button is held beyond long press threshold

## Event Timing

### Debounce
- Prevents noise and contact bounce from triggering multiple events
- Default: 50ms
- Adjustable via constructor

### Long Press
- Triggered when button is held continuously beyond threshold
- Default: 500ms (half second)
- Adjustable via constructor
- Prevents single tap event when long press occurs

### Single Tap
- Quick press and release
- Only triggered if button is released before long press threshold
- Mutually exclusive with long press

## Hardware Considerations

### Pull-Up Configuration (Default)
- Button connects GPIO to GND
- Internal pull-up resistor enabled
- GPIO reads HIGH when not pressed, LOW when pressed
- Most common configuration for mechanical buttons

### Pull-Down Configuration
- Button connects GPIO to VCC (3.3V)
- Internal pull-down resistor enabled
- GPIO reads LOW when not pressed, HIGH when pressed

## Important Notes
- Designed for mechanical pushbuttons
- Call `update()` frequently (at least every few milliseconds)
- Debounce timing handles typical mechanical switch bounce
- Long press only triggers once per button hold
- Avoid long operations in callbacks to maintain responsiveness
- All timing is handled in software (no interrupts required)
- Multiple buttons can be managed independently
