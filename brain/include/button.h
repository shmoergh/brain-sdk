#pragma once

#include <cstdint>
#include <functional>

#include "pico/stdlib.h"

class Button {
public:
	/**
	 * @brief Creates a `Button` on one GPIO pin with timing thresholds for debounce and long-press detection.
	 * @param gpio_pin GPIO pin connected to the physical button.
	 * @param debounce_ms Minimum stable time (ms) before a state change is accepted as a real press/release.
	 * @param long_press_ms Hold time (ms) required before `set_on_long_press()` callback fires.
	 */
	Button(uint gpio_pin, uint32_t debounce_ms = 50, uint32_t long_press_ms = 500);

	/**
	 * @brief Initializes the GPIO direction and pull configuration for this `Button`.
	 * @param pull_up `true` enables internal pull-up (button should short to GND when pressed). `false` leaves pull-up disabled.
	 */
	void init(bool pull_up = true);

	/**
	 * @brief Polls the button pin, applies debounce/long-press logic, and triggers registered callbacks.
	 */
	void update();

	/**
	 * @brief Registers a callback for the rising "pressed" event.
	 * @param callback Function called once when the button transitions into pressed state after debounce.
	 */
	void set_on_press(std::function<void()> callback);

	/**
	 * @brief Registers a callback for the release event.
	 * @param callback Function called once when the button transitions into released state after debounce.
	 */
	void set_on_release(std::function<void()> callback);

	/**
	 * @brief Registers a callback for a short press-release tap.
	 * @param callback Function called when the press duration is below long-press threshold.
	 */
	void set_on_single_tap(std::function<void()> callback);

	/**
	 * @brief Registers a callback for long-press detection.
	 * @param callback Function called once when the button has been held for at least `long_press_ms`.
	 */
	void set_on_long_press(std::function<void()> callback);

private:
	uint gpio_pin_;
	bool is_pressed_;
	absolute_time_t last_press_time_;
	absolute_time_t last_release_time_;
	absolute_time_t last_tap_time_;
	uint32_t debounce_ms_;
	uint32_t long_press_ms_;

	std::function<void()> on_press_;
	std::function<void()> on_release_;
	std::function<void()> on_single_tap_;
	std::function<void()> on_long_press_;

	bool long_press_triggered_;
	bool last_state_;
	absolute_time_t last_change_time_;
};
