#pragma once

#include <pico/stdlib.h>

#include <cstdint>
#include <functional>

enum class LedMode : uint8_t {
	kSimple = 0,
	kPwm = 1
};

class Led {
public:
	Led(uint gpio_pin, bool simple_mode = false);

	void init();
	void init(LedMode mode);
	void set_mode(LedMode mode);
	LedMode get_mode() const;

	void on();
	void off();
	void toggle();
	void set_brightness(uint8_t value);

	void blink(uint times, uint interval_ms);
	void blink_duration(uint duration_ms, uint interval_ms);
	void start_blink(uint interval_ms);
	void stop_blink();
	void update();

	void set_on_state_change(std::function<void(bool)> callback);
	void set_on_blink_end(std::function<void()> callback);

	bool is_on() const;
	bool is_blinking() const;

private:
	void configure_pin_for_mode();

	uint gpio_pin_;
	LedMode mode_;
	bool initialized_;
	uint8_t brightness_;
	bool state_;
	bool blinking_;
	bool constant_blink_;
	uint blink_times_;
	uint blink_interval_ms_;
	uint blink_count_;
	absolute_time_t last_blink_time_;
	std::function<void(bool)> on_state_change_;
	std::function<void()> on_blink_end_;
	bool duration_blink_ = false;
	uint duration_ms_ = 0;
	absolute_time_t blink_start_time_ = 0;
};

namespace brain {
namespace ui {
using ::LedMode;
using ::Led;
}  // namespace ui
}  // namespace brain
