# LED Channel API (via `Leds`)

## Overview
The old standalone `Led` class was removed.

Use `Leds` and control one channel by index (`0..5`).

## Include
```cpp
#include "brain/include/leds.h"
```

## Single-LED Usage
```cpp
Leds leds;
leds.init(kLedsModePwm);

const uint8_t kLed = 0;
leds.set_brightness(kLed, 128);
leds.on(kLed);
leds.blink(kLed, 3, 150);

while (true) {
	leds.update();
}
```

## Per-Channel Methods
- `on(index)`
- `off(index)`
- `toggle(index)`
- `set_brightness(index, value)`
- `blink(index, times, interval_ms)`
- `blink_duration(index, duration_ms, interval_ms)`
- `start_blink(index, interval_ms)`
- `stop_blink(index)`
- `set_on_state_change(index, cb)`
- `set_on_blink_end(index, cb)`
- `is_on(index)`
- `is_blinking(index)`
