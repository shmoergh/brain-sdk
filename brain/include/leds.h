#pragma once

#include <array>
#include <cstdint>
#include <functional>

#include <pico/time.h>

#include "common.h"

enum class LedMode : uint8_t {
	kSimple = 0,
	kPwm = 1
};

// Convenience aliases for less verbose mode selection.
constexpr LedMode kLedsModeSimple = LedMode::kSimple;
constexpr LedMode kLedsModePwm = LedMode::kPwm;

constexpr uint8_t NO_OF_LEDS = 6;
constexpr uint8_t led_pins[NO_OF_LEDS] = {
	GPIO_BRAIN_LED_1,
	GPIO_BRAIN_LED_2,
	GPIO_BRAIN_LED_3,
	GPIO_BRAIN_LED_4,
	GPIO_BRAIN_LED_5,
	GPIO_BRAIN_LED_6
};

constexpr uint8_t kLedCount = NO_OF_LEDS;

constexpr uint8_t led_pin(uint8_t index) {
	return index < NO_OF_LEDS ? led_pins[index] : 0;
}

class Leds {
public:
	/**
	 * @brief Creates a `Leds` controller for the 6 front-panel LEDs and stores the default drive mode.
	 * @param mode LED drive mode used during `init()`:
	 * - `kLedsModeSimple`: each LED GPIO is used as digital on/off (no PWM dimming).
	 * - `kLedsModePwm`: each LED GPIO is switched to PWM so `set_brightness()` controls intensity (0..255).
	 */
	explicit Leds(LedMode mode);

	/**
	 * @brief Creates a `Leds` controller using a boolean shorthand for the default mode.
	 * @param simple_mode `true` selects `kLedsModeSimple`; `false` selects `kLedsModePwm`.
	 */
	explicit Leds(bool simple_mode = false);

	/**
	 * @brief Initializes all 6 panel LEDs plus the button LED using the currently stored default mode.
	 *
	 * Panel LEDs are turned off after initialization. The dedicated button LED is always initialized
	 * in simple digital mode.
	 */
	void init();

	/**
	 * @brief Initializes all LEDs and overrides the default panel LED mode.
	 * @param mode Drive mode applied to panel LEDs:
	 * - `kLedsModeSimple`: digital on/off output.
	 * - `kLedsModePwm`: PWM output with 8-bit brightness.
	 */
	void init(LedMode mode);

	/**
	 * @brief Changes the runtime mode of all 6 panel LEDs without changing their logical state.
	 * @param mode Target mode for panel LEDs:
	 * - `kLedsModeSimple`: brightness is reduced to off/on semantics (`brightness > 0` becomes ON).
	 * - `kLedsModePwm`: keeps full 0..255 brightness control through hardware PWM.
	 */
	void set_mode(LedMode mode);

	/**
	 * @brief Returns the current panel LED mode stored by `Leds`.
	 * @return `kLedsModeSimple` or `kLedsModePwm` for panel LEDs.
	 */
	LedMode get_mode() const;

	/**
	 * @brief Advances blink state machines for all panel LEDs and the button LED.
	 *
	 * Call this repeatedly in the main loop; timed blink APIs rely on this update tick.
	 */
	void update();

	/**
	 * @brief Turns one panel LED fully on.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 */
	void on(uint8_t led);

	/**
	 * @brief Turns one panel LED fully off.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 */
	void off(uint8_t led);

	/**
	 * @brief Toggles one panel LED between on and off.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 */
	void toggle(uint8_t led);

	/**
	 * @brief Sets panel LED brightness (or on/off threshold in simple mode).
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 * @param brightness 8-bit brightness:
	 * - In `kLedsModePwm`, `0` is off and `255` is full brightness.
	 * - In `kLedsModeSimple`, `0` is off and any non-zero value is on.
	 */
	void set_brightness(uint8_t led, uint8_t brightness);

	/**
	 * @brief Starts finite blinking on one panel LED.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 * @param times Number of full blink cycles before auto-stop.
	 * @param interval_ms Delay between state toggles in milliseconds.
	 */
	void blink(uint8_t led, uint times, uint interval_ms);

	/**
	 * @brief Blinks one panel LED for a fixed wall-clock duration.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 * @param duration_ms Total blink duration in milliseconds before forced stop.
	 * @param interval_ms Delay between state toggles in milliseconds.
	 */
	void blink_duration(uint8_t led, uint duration_ms, uint interval_ms);

	/**
	 * @brief Starts continuous blinking on one panel LED until explicitly stopped.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 * @param interval_ms Delay between state toggles in milliseconds.
	 */
	void start_blink(uint8_t led, uint interval_ms);

	/**
	 * @brief Stops blinking on one panel LED and turns it off.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 */
	void stop_blink(uint8_t led);

	/**
	 * @brief Registers a callback fired when a panel LED logical state changes.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 * @param callback Handler receiving `true` when LED transitions on and `false` when it transitions off.
	 */
	void set_on_state_change(uint8_t led, std::function<void(bool)> callback);

	/**
	 * @brief Registers a callback fired when a panel LED blink program ends.
	 * @param led Zero-based panel LED index in the range 0..5. Invalid indices are ignored.
	 * @param callback Handler called when blinking is stopped by completion or explicit stop.
	 */
	void set_on_blink_end(uint8_t led, std::function<void()> callback);

	/**
	 * @brief Sets all 6 panel LEDs from a bit mask.
	 * @param mask Bit `i` controls LED `i` (0..5): `1` turns it on, `0` turns it off.
	 */
	void set_from_mask(uint8_t mask);

	/**
	 * @brief Turns all 6 panel LEDs on.
	 */
	void on_all();

	/**
	 * @brief Turns all 6 panel LEDs off.
	 */
	void off_all();

	/**
	 * @brief Plays a blocking startup sweep across the 6 panel LEDs.
	 *
	 * This routine uses `sleep_ms(100)` between LEDs and therefore blocks the caller until finished.
	 */
	void startup_animation();

	/**
	 * @brief Reports current logical on/off state for one panel LED.
	 * @param led Zero-based panel LED index in the range 0..5.
	 * @return `true` if that LED is currently on; `false` if it is off or the index is invalid.
	 */
	bool is_on(uint8_t led) const;

	/**
	 * @brief Reports whether one panel LED has an active blink program.
	 * @param led Zero-based panel LED index in the range 0..5.
	 * @return `true` if finite, duration, or continuous blink is currently active; `false` otherwise.
	 */
	bool is_blinking(uint8_t led) const;

	/**
	 * @brief Initializes the dedicated button LED GPIO as an output.
	 *
	 * The button LED always runs in simple on/off mode.
	 */
	void button_init();

	/**
	 * @brief Turns the dedicated button LED on.
	 */
	void button_on();

	/**
	 * @brief Turns the dedicated button LED off.
	 */
	void button_off();

	/**
	 * @brief Toggles the dedicated button LED.
	 */
	void button_toggle();

	/**
	 * @brief Sets button LED brightness value.
	 * @param brightness Brightness value for the button LED. In simple mode, `0` is off and non-zero is on.
	 */
	void button_set_brightness(uint8_t brightness);

	/**
	 * @brief Starts finite blinking on the button LED.
	 * @param times Number of full blink cycles before auto-stop.
	 * @param interval_ms Delay between state toggles in milliseconds.
	 */
	void button_blink(uint times, uint interval_ms);

	/**
	 * @brief Blinks the button LED for a fixed duration.
	 * @param duration_ms Total blink duration in milliseconds before forced stop.
	 * @param interval_ms Delay between state toggles in milliseconds.
	 */
	void button_blink_duration(uint duration_ms, uint interval_ms);

	/**
	 * @brief Starts continuous blinking on the button LED until `button_stop_blink()` is called.
	 * @param interval_ms Delay between state toggles in milliseconds.
	 */
	void button_start_blink(uint interval_ms);

	/**
	 * @brief Stops button LED blinking and turns it off.
	 */
	void button_stop_blink();

	/**
	 * @brief Reports whether the button LED is currently on.
	 * @return `true` when button LED state is on; `false` when off.
	 */
	bool button_is_on() const;

	/**
	 * @brief Reports whether the button LED has an active blink program.
	 * @return `true` if blink is active; `false` otherwise.
	 */
	bool button_is_blinking() const;

	/**
	 * @brief Registers a callback fired when button LED state changes.
	 * @param callback Handler receiving `true` when the button LED turns on and `false` when it turns off.
	 */
	void button_set_on_state_change(std::function<void(bool)> callback);

	/**
	 * @brief Registers a callback fired when button LED blinking ends.
	 * @param callback Handler called when button LED blink completes or is explicitly stopped.
	 */
	void button_set_on_blink_end(std::function<void()> callback);

private:
	struct ChannelState {
		uint gpio_pin = 0;
			LedMode mode = kLedsModeSimple;
		bool initialized = false;
		uint8_t brightness = 255;
		bool state = false;
		bool blinking = false;
		bool constant_blink = false;
		uint blink_times = 0;
		uint blink_interval_ms = 0;
		uint blink_count = 0;
		absolute_time_t last_blink_time{};
		std::function<void(bool)> on_state_change;
		std::function<void()> on_blink_end;
		bool duration_blink = false;
		uint duration_ms = 0;
		absolute_time_t blink_start_time{};
	};

	bool validate_led(uint8_t led) const;
	void configure_channel_for_mode(ChannelState& channel);
	void init_channel(ChannelState& channel, LedMode mode);
	void set_channel_mode(ChannelState& channel, LedMode mode);
	void set_channel_brightness(ChannelState& channel, uint8_t value);
	void channel_on(ChannelState& channel);
	void channel_off(ChannelState& channel);
	void channel_toggle(ChannelState& channel);
	void channel_blink(ChannelState& channel, uint times, uint interval_ms);
	void channel_blink_duration(ChannelState& channel, uint duration_ms, uint interval_ms);
	void channel_start_blink(ChannelState& channel, uint interval_ms);
	void channel_stop_blink(ChannelState& channel);
	void channel_update(ChannelState& channel);

	std::array<ChannelState, NO_OF_LEDS> leds_{};
	ChannelState button_led_{};
	LedMode mode_;
};
