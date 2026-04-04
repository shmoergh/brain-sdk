#pragma once

#include <cstdint>

#include "button.h"
#include "common.h"

class Buttons {
public:
	explicit Buttons(
		uint button_a_pin = GPIO_BRAIN_BUTTON_1,
		uint button_b_pin = GPIO_BRAIN_BUTTON_2,
		uint32_t debounce_ms = 50,
		uint32_t long_press_ms = 500)
		: button_a(button_a_pin, debounce_ms, long_press_ms),
		  button_b(button_b_pin, debounce_ms, long_press_ms) {}

	void init(bool pull_up = true) {
		button_a.init(pull_up);
		button_b.init(pull_up);
	}

	void update() {
		button_a.update();
		button_b.update();
	}

	Button button_a;
	Button button_b;
};
