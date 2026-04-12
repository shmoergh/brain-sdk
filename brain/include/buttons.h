#pragma once

#include <cstdint>

#include "button.h"
#include "common.h"

class Buttons {
public:
	/**
	 * @brief Creates a two-button helper that owns `button_a` and `button_b`.
	 * @param button_a_pin GPIO pin for button A.
	 * @param button_b_pin GPIO pin for button B.
	 * @param debounce_ms Debounce window (ms) used for both buttons.
	 * @param long_press_ms Long-press threshold (ms) used for both buttons.
	 */
	explicit Buttons(
		uint button_a_pin = GPIO_BRAIN_BUTTON_1,
		uint button_b_pin = GPIO_BRAIN_BUTTON_2,
		uint32_t debounce_ms = 50,
		uint32_t long_press_ms = 500)
		: button_a(button_a_pin, debounce_ms, long_press_ms),
		  button_b(button_b_pin, debounce_ms, long_press_ms) {}

	/**
	 * @brief Initializes both button GPIO inputs (`button_a` and `button_b`).
	 * @param pull_up `true` enables internal pull-up on both pins (active-low wiring). `false` disables internal pull-ups.
	 */
	void init(bool pull_up = true) {
		button_a.init(pull_up);
		button_b.init(pull_up);
	}

	/**
	 * @brief Updates both buttons once (equivalent to calling `button_a.update()` and `button_b.update()`).
	 */
	void update() {
		button_a.update();
		button_b.update();
	}

	Button button_a;
	Button button_b;
};
