#include "brain-ui/leds.h"

namespace brain::ui {

void Leds::init() {
	leds_.reserve(NO_OF_LEDS);

	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		leds_.emplace_back(led_pins[i]);
		leds_[i].init();
		leds_[i].off();
	}
}

void Leds::on(uint8_t led) {
	if (validate_led(led)) leds_[led].on();
}

void Leds::off(uint8_t led) {
	if (validate_led(led)) leds_[led].off();
}

void Leds::toggle(uint8_t led) {
	if (validate_led(led)) leds_[led].toggle();
}

void Leds::set_brightness(uint8_t led, uint8_t brightness) {
	if (validate_led(led)) leds_[led].set_brightness(brightness);
}

void Leds::blink_duration(uint8_t led, uint duration_ms, uint interval_ms) {
	if (validate_led(led)) leds_[led].blink_duration(duration_ms, interval_ms);
}

void Leds::start_blink(uint8_t led, uint interval_ms) {
	if (validate_led(led)) leds_[led].start_blink(interval_ms);
}

void Leds::stop_blink(uint8_t led) {
	if (validate_led(led)) leds_[led].stop_blink();
}

bool Leds::is_on(uint8_t led) {
	if (validate_led(led)) {
		return leds_[led].is_on();
	}
	return false;
}

bool Leds::is_blinking(uint8_t led) {
	if (validate_led(led)) {
		return leds_[led].is_blinking();
	}
	return false;
}

void Leds::set_from_mask(uint8_t mask) {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		if (mask & (1 << i)) {
			leds_[i].on();
		} else {
			leds_[i].off();
		}
	}
}

void Leds::on_all() {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		leds_[i].on();
	}
}

void Leds::off_all() {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		leds_[i].off();
	}
}

void Leds::startup_animation() {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		leds_[i].on();
		sleep_ms(100);
		leds_[i].off();
	}
}

bool Leds::validate_led(uint8_t led) {
	return (led >= 0 && led < 6);
}

}
