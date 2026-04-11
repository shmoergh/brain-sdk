# Leds Component

## Overview
`Leds` manages all six front-panel LEDs and the illuminated button LED.

## Include
```cpp
#include "brain/include/leds.h"
```

## Quick Start
```cpp
Leds leds;
leds.init(LedMode::kPwm);

leds.on(0);
leds.set_brightness(1, 96);
leds.set_from_mask(0b001011);
leds.button_on();

while (true) {
	leds.update();
}
```

## Main API
- `init()` / `init(LedMode mode)`
- `set_mode(LedMode mode)` / `get_mode()`
- `update()`
- `on/off/toggle/set_brightness`
- `blink`, `blink_duration`, `start_blink`, `stop_blink`
- `set_from_mask`, `on_all`, `off_all`, `startup_animation`
- `is_on`, `is_blinking`

## Button LED API
See [BUTTON_LED](BUTTON_LED.md).

## Pin Constants
Defined in:
- `brain/include/gpio-setup.h`
- `brain/include/constants.h`
