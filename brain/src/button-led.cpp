#include "button-led.h"


ButtonLed::ButtonLed(uint gpio_pin) : led_(gpio_pin, true) {}

void ButtonLed::init() {
	led_.init(LedMode::kSimple);
}

void ButtonLed::on() {
	led_.on();
}

void ButtonLed::off() {
	led_.off();
}

void ButtonLed::toggle() {
	led_.toggle();
}

void ButtonLed::blink(uint times, uint interval_ms) {
	led_.blink(times, interval_ms);
}

void ButtonLed::blink_duration(uint duration_ms, uint interval_ms) {
	led_.blink_duration(duration_ms, interval_ms);
}

void ButtonLed::start_blink(uint interval_ms) {
	led_.start_blink(interval_ms);
}

void ButtonLed::stop_blink() {
	led_.stop_blink();
}

void ButtonLed::update() {
	led_.update();
}

bool ButtonLed::is_on() const {
	return led_.is_on();
}

bool ButtonLed::is_blinking() const {
	return led_.is_blinking();
}

void ButtonLed::set_on_state_change(std::function<void(bool)> callback) {
	led_.set_on_state_change(callback);
}

void ButtonLed::set_on_blink_end(std::function<void()> callback) {
	led_.set_on_blink_end(callback);
}
