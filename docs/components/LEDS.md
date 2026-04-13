# Leds Component

`Leds` is the SDK’s unified LED controller for the Brain module. Instead of manually toggling GPIO/PWM pins for each LED, you use one class to control:

- the LED strip (6 panel LEDs)
- the LED for button A (through `button_*` methods)

## LED strip

### Example
```cpp
#define BRAIN_USE_LEDS 1
#include "brain/brain.h"

#include <pico/stdlib.h>

Brain brain;

int main() {
	stdio_init_all();

	// Init LEDs in simple on/off mode
	BrainInitStatus status = brain.init_leds(kLedsModeSimple);
	if (!brain_init_succeeded(status)) return 1;

	uint8_t index = 0;

	while (true) {
		// Keep LED timing engine updated
		brain.update_leds();

		// Simple running light across the 6 LEDs
		brain.leds.off_all();
		brain.leds.on(index);
		index = (index + 1) % 6;

		sleep_ms(150);
	}
}
```

### Constants

- `kLedsModeSimple`
  Uses basic on/off behavior per LED channel.

- `kLedsModePwm`
  Enables brightness-level control via PWM behavior.

- `NO_OF_LEDS`
  Compile-time number of panel LEDs (`6`).

- `kLedCount`
  Alias of `NO_OF_LEDS`, useful for loops.

- `led_pins[NO_OF_LEDS]`
  Compile-time array mapping LED index to GPIO pin.

- `led_pin(uint8_t index)`
  Returns pin for a given LED index, or `0` if index is out of range.

### Class API and lifecycle

- `Leds(LedMode mode)`
  Constructs a controller with an explicit initial mode.

- `Leds(bool simple_mode = false)`
  Alternate constructor; `true` maps to simple mode, `false` to PWM mode.

- `init()`
  Initializes panel LED channels with the currently selected mode.

- `init(LedMode mode)`
  Sets mode and initializes channels in one call.

- `set_mode(LedMode mode)`
  Changes controller mode after construction/init.

- `get_mode() const`
  Returns the currently active mode.

- `update()`
  Advances internal timing/state logic (blinks, timed behaviors). Call repeatedly in your main loop.

## LED strip control

- `on(uint8_t led)`
  Turns one panel LED on.

- `off(uint8_t led)`
  Turns one panel LED off.

- `toggle(uint8_t led)`
  Flips current state of one panel LED.

- `set_brightness(uint8_t led, uint8_t brightness)`
  Sets brightness level (`0..255`) for one LED (meaningful in PWM mode).

- `blink(uint8_t led, uint times, uint interval_ms)`
  Starts finite blink sequence for one LED using interval timing.

- `blink_duration(uint8_t led, uint duration_ms, uint interval_ms)`
  Blinks one LED for a total duration instead of fixed blink count.

- `start_blink(uint8_t led, uint interval_ms)`
  Starts continuous blinking on one LED until stopped.

- `stop_blink(uint8_t led)`
  Stops blinking on one LED.

## Single LED strip callbacks

- `set_on_state_change(uint8_t led, std::function<void(bool)> callback)`
  Registers callback fired when that LED state changes; callback gets new state (`true` on, `false` off).

- `set_on_blink_end(uint8_t led, std::function<void()> callback)`
  Registers callback fired when a finite blink sequence finishes.

## LED strip level functions

- `set_from_mask(uint8_t mask)`
  Sets panel LED on/off states from bitmask bits.

- `on_all()`
  Turns all panel LEDs on.

- `off_all()`
  Turns all panel LEDs off.

- `startup_animation()`
  Runs the built-in startup LED animation pattern.

## LED strip state queries

- `is_on(uint8_t led) const`
  Returns whether one panel LED is currently on.

- `is_blinking(uint8_t led) const`
  Returns whether one panel LED is currently in blinking mode.

---

## Button LED

### Example

```cpp
#define BRAIN_USE_LEDS 1
#include "brain/brain.h"

#include <pico/stdlib.h>

Brain brain;

int main() {
	stdio_init_all();

	// Init LED controller via Brain wrapper
	BrainInitStatus status = brain.init_leds(kLedsModeSimple);
	if (!brain_init_succeeded(status)) return 1;

	// Optional explicit init for button LED channel
	brain.leds.button_init();

	while (true) {
		// Keep LED timing/blink engine running
		brain.update_leds();

		// Blink button LED: on 200ms, off 200ms
		brain.leds.button_on();
		sleep_ms(200);

		brain.leds.button_off();
		sleep_ms(200);
	}
}
```

### Button LED lifecycle/control

- `button_init()`
  Initializes the dedicated button LED channel.

- `button_on()`
  Turns button LED on.

- `button_off()`
  Turns button LED off.

- `button_toggle()`
  Toggles button LED state.

- `button_blink(uint times, uint interval_ms)`
  Starts finite blink sequence on button LED.

- `button_blink_duration(uint duration_ms, uint interval_ms)`
  Blinks button LED for a total duration.

- `button_start_blink(uint interval_ms)`
  Starts continuous button LED blinking.

- `button_stop_blink()`
  Stops button LED blinking.

### Button LED state queries/callbacks

- `button_is_on() const`
  Returns whether button LED is currently on.

- `button_is_blinking() const`
  Returns whether button LED is currently blinking.

- `button_set_on_state_change(std::function<void(bool)> callback)`
  Registers callback for button LED state changes.

- `button_set_on_blink_end(std::function<void()> callback)`
  Registers callback for end of finite button LED blink sequence.
