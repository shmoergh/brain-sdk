# BUTTON Component

## Overview
The Button component provides an interface for the buttons on the Brain. It handles debouncing, state tracking, and multiple event types including press, release, single tap, and long press detection.

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

## Example - Single Tap Detection
```cpp
#define BRAIN_USE_BUTTONS 1
#include "brain/brain.h"

#include <pico/stdlib.h>
#include <stdio.h>

Brain brain;

int main() {
    stdio_init_all();

    // Wrapper uses Brain button pins/timing defaults
    BrainInitStatus status = brain.init_buttons(true);
    if (!brain_init_succeeded(status)) return 1;

    // Use button_a as the equivalent button
    brain.buttons.button_a.set_on_single_tap([]() {
        printf("Quick tap!\n");
    });

    brain.buttons.button_a.set_on_long_press([]() {
        printf("Held down!\n");
    });

    while (true) {
        brain.update_buttons();
        sleep_ms(1);
    }
}

```

## Example — Press & release example
```cpp
#define BRAIN_USE_BUTTONS 1
#include "brain/brain.h"

#include <pico/stdlib.h>
#include <stdio.h>

Brain brain;

int main() {
    stdio_init_all();

    // Initialize Brain button pair (default pins, pull-up enabled)
    BrainInitStatus status = brain.init_buttons(true);
    if (!brain_init_succeeded(status)) return 1;

    // Use button_a as the equivalent of "my_button"
    brain.buttons.button_a.set_on_press([]() {
        printf("Button pressed!\n");
    });

    brain.buttons.button_a.set_on_release([]() {
        printf("Button released!\n");
    });

    while (true) {
        brain.update_buttons();
        sleep_ms(1);
    }
}

```

## Example — Long Press Detection
```cpp
#define BRAIN_USE_BUTTONS 1
#include "brain/brain.h"

#include <pico/stdlib.h>
#include <stdio.h>

Brain brain;

int main() {
    stdio_init_all();

    if (!brain_init_succeeded(brain.init_buttons(true))) return 1;

    brain.buttons.button_a.set_on_press([]() {
        printf("Button pressed\n");
    });

    brain.buttons.button_a.set_on_long_press([]() {
        printf("Long press detected!\n");
    });

    while (true) {
        brain.update_buttons();
        sleep_ms(1);
    }
}

```


## Class API and lifecycle

- `Button(uint gpio_pin, uint32_t debounce_ms = 50, uint32_t long_press_ms = 500)`
  Creates a button instance bound to one GPIO pin with configurable debounce and long-press thresholds.

- `void init(bool pull_up = true)`
  Initializes the GPIO input and internal timing/state tracking.

- `void update()`
  Runs debounce and event detection logic. Must be called repeatedly in the main loop.

## Callback registration API

- `void set_on_press(std::function<void()> callback)`
  Registers callback for press event.

- `void set_on_release(std::function<void()> callback)`
  Registers callback for release event.

- `void set_on_single_tap(std::function<void()> callback)`
  Registers callback for single-tap event.

- `void set_on_long_press(std::function<void()> callback)`
  Registers callback for long-press event.

## Timing/control parameters (constructor-defined)

- `gpio_pin`
  Input pin number used by this button instance.

- `debounce_ms`
  Debounce duration in milliseconds used to accept state transitions.

- `long_press_ms`
  Hold duration in milliseconds required before long-press callback fires.

## Runtime wiring mode

- `init(true)`
  Enables pull-up input mode (commonly active-low button wiring).

- `init(false)`
  Enables pull-down input mode.