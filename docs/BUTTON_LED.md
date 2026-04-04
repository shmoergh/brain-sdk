# Button LED API (via `Leds`)

## Overview
The old standalone `ButtonLed` class was removed.

Use the dedicated button-led methods on `Leds`.

## Include
```cpp
#include "brain/include/leds.h"
```

## Usage
```cpp
Leds leds;
leds.init();

leds.button_on();
leds.button_blink_duration(1000, 120);

while (true) {
	leds.update();
}
```

## Button LED Methods
- `button_init()`
- `button_on()`
- `button_off()`
- `button_toggle()`
- `button_set_brightness(uint8_t)`
- `button_blink(uint times, uint interval_ms)`
- `button_blink_duration(uint duration_ms, uint interval_ms)`
- `button_start_blink(uint interval_ms)`
- `button_stop_blink()`
- `button_is_on() const`
- `button_is_blinking() const`
- `button_set_on_state_change(std::function<void(bool)>)`
- `button_set_on_blink_end(std::function<void()>)`
