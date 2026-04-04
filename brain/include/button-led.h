#pragma once

#include <functional>

#include "common.h"
#include "led.h"

class ButtonLed {
public:
	explicit ButtonLed(uint gpio_pin = GPIO_BRAIN_BUTTON_1_LED);

	void init();
	void on();
	void off();
	void toggle();

	void blink(uint times, uint interval_ms);
	void blink_duration(uint duration_ms, uint interval_ms);
	void start_blink(uint interval_ms);
	void stop_blink();
	void update();

	bool is_on() const;
	bool is_blinking() const;

	void set_on_state_change(std::function<void(bool)> callback);
	void set_on_blink_end(std::function<void()> callback);

private:
	Led led_;
};

namespace brain {
namespace ui {
using ::ButtonLed;
}  // namespace ui
}  // namespace brain
