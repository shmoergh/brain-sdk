#pragma once

#include <cstdint>
#include <functional>

#include "pico/stdlib.h"

class Button {
public:
	Button(uint gpio_pin, uint32_t debounce_ms = 50, uint32_t long_press_ms = 500);

	void init(bool pull_up = true);
	void update();

	void set_on_press(std::function<void()> callback);
	void set_on_release(std::function<void()> callback);
	void set_on_single_tap(std::function<void()> callback);
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

