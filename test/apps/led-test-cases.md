# LED test cases

## leds.cpp

Using NO_OF_LEDS and led_pins from leds.h.

### General
- Startup animation plays
- update() is called regularly when blink-related tests are running
- Repeated init() call behavior is defined and tested

### Simple mode
- Each LED turns on and off in simple mode
- Each LED toggles in simple mode
- Each LED blinks in simple mode
- start_blink() and stop_blink() work for each LED in simple mode
- set_brightness(led, 0) turns LED off in simple mode
- set_brightness(led, non-zero) turns LED on in simple mode
- All LEDs can be turned on or off calling on_all() and off_all() in simple mode
- is_on() and is_blinking() return expected values after each action in simple mode

### PWM mode
- Each LED turns on and off in PWM mode
- Each LED toggles in PWM mode
- Each LED blinks in PWM mode
- start_blink() and stop_blink() work for each LED in PWM mode
- Each LED brightness can be set in 10% steps (including explicit 0% and 100%)
- All LEDs can be turned on or off calling on_all() and off_all() in PWM mode
- is_on() and is_blinking() return expected values after each action in PWM mode

### Mask mode
- Various combinations can be set to turn LEDs on and off
- set_from_mask(0x00) turns all LEDs off
- set_from_mask(0x3F) turns all LEDs on
- Walking one-hot mask across all LEDs works
- Bits above NO_OF_LEDS are ignored safely

### Index / bounds
- Invalid LED indices (NO_OF_LEDS, 255) are handled safely
- Invalid index calls do not change valid LED states

### State transitions
- on()/off()/toggle()/set_from_mask() behavior is verified when the target LED is currently blinking
- Blink does not progress if update() is not called

## led.cpp (direct Led class tests)

- blink(times, interval) finite blink sequence works as expected
- set_on_state_change() callback fires with correct state values
- set_on_blink_end() callback fires when blink sequence ends
