#include "leds.h"

#include <hardware/gpio.h>
#include <hardware/pwm.h>
#include <pico/stdlib.h>

Leds::Leds(LedMode mode) : mode_(mode) {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		leds_[i].gpio_pin = led_pins[i];
		leds_[i].mode = mode_;
	}
	button_led_.gpio_pin = GPIO_BRAIN_BUTTON_1_LED;
	button_led_.mode = LedMode::kSimple;
}

Leds::Leds(bool simple_mode) : Leds(simple_mode ? LedMode::kSimple : LedMode::kPwm) {}

void Leds::init() {
	init(mode_);
}

void Leds::init(LedMode mode) {
	mode_ = mode;
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		init_channel(leds_[i], mode_);
		channel_off(leds_[i]);
	}
	button_init();
	button_off();
}

void Leds::set_mode(LedMode mode) {
	if (mode_ == mode) {
		return;
	}

	mode_ = mode;
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		set_channel_mode(leds_[i], mode_);
	}
}

LedMode Leds::get_mode() const {
	return mode_;
}

void Leds::update() {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		channel_update(leds_[i]);
	}
	channel_update(button_led_);
}

void Leds::on(uint8_t led) {
	if (validate_led(led)) {
		channel_on(leds_[led]);
	}
}

void Leds::off(uint8_t led) {
	if (validate_led(led)) {
		channel_off(leds_[led]);
	}
}

void Leds::toggle(uint8_t led) {
	if (validate_led(led)) {
		channel_toggle(leds_[led]);
	}
}

void Leds::set_brightness(uint8_t led, uint8_t brightness) {
	if (validate_led(led)) {
		set_channel_brightness(leds_[led], brightness);
	}
}

void Leds::blink(uint8_t led, uint times, uint interval_ms) {
	if (validate_led(led)) {
		channel_blink(leds_[led], times, interval_ms);
	}
}

void Leds::blink_duration(uint8_t led, uint duration_ms, uint interval_ms) {
	if (validate_led(led)) {
		channel_blink_duration(leds_[led], duration_ms, interval_ms);
	}
}

void Leds::start_blink(uint8_t led, uint interval_ms) {
	if (validate_led(led)) {
		channel_start_blink(leds_[led], interval_ms);
	}
}

void Leds::stop_blink(uint8_t led) {
	if (validate_led(led)) {
		channel_stop_blink(leds_[led]);
	}
}

void Leds::set_on_state_change(uint8_t led, std::function<void(bool)> callback) {
	if (validate_led(led)) {
		leds_[led].on_state_change = callback;
	}
}

void Leds::set_on_blink_end(uint8_t led, std::function<void()> callback) {
	if (validate_led(led)) {
		leds_[led].on_blink_end = callback;
	}
}

void Leds::set_from_mask(uint8_t mask) {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		if (mask & (1 << i)) {
			channel_on(leds_[i]);
		} else {
			channel_off(leds_[i]);
		}
	}
}

void Leds::on_all() {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		channel_on(leds_[i]);
	}
}

void Leds::off_all() {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		channel_off(leds_[i]);
	}
}

void Leds::startup_animation() {
	for (size_t i = 0; i < NO_OF_LEDS; i++) {
		channel_on(leds_[i]);
		sleep_ms(100);
		channel_off(leds_[i]);
	}
}

bool Leds::is_on(uint8_t led) const {
	if (!validate_led(led)) {
		return false;
	}
	return leds_[led].state;
}

bool Leds::is_blinking(uint8_t led) const {
	if (!validate_led(led)) {
		return false;
	}
	return leds_[led].blinking;
}

void Leds::button_init() {
	init_channel(button_led_, LedMode::kSimple);
}

void Leds::button_on() {
	channel_on(button_led_);
}

void Leds::button_off() {
	channel_off(button_led_);
}

void Leds::button_toggle() {
	channel_toggle(button_led_);
}

void Leds::button_set_brightness(uint8_t brightness) {
	set_channel_brightness(button_led_, brightness);
}

void Leds::button_blink(uint times, uint interval_ms) {
	channel_blink(button_led_, times, interval_ms);
}

void Leds::button_blink_duration(uint duration_ms, uint interval_ms) {
	channel_blink_duration(button_led_, duration_ms, interval_ms);
}

void Leds::button_start_blink(uint interval_ms) {
	channel_start_blink(button_led_, interval_ms);
}

void Leds::button_stop_blink() {
	channel_stop_blink(button_led_);
}

bool Leds::button_is_on() const {
	return button_led_.state;
}

bool Leds::button_is_blinking() const {
	return button_led_.blinking;
}

void Leds::button_set_on_state_change(std::function<void(bool)> callback) {
	button_led_.on_state_change = callback;
}

void Leds::button_set_on_blink_end(std::function<void()> callback) {
	button_led_.on_blink_end = callback;
}

bool Leds::validate_led(uint8_t led) const {
	return led < NO_OF_LEDS;
}

void Leds::configure_channel_for_mode(ChannelState& channel) {
	if (channel.mode == LedMode::kSimple) {
		gpio_init(channel.gpio_pin);
		gpio_set_dir(channel.gpio_pin, GPIO_OUT);
	} else {
		gpio_set_function(channel.gpio_pin, GPIO_FUNC_PWM);
		uint slice = pwm_gpio_to_slice_num(channel.gpio_pin);
		pwm_set_wrap(slice, 255);
		pwm_set_enabled(slice, true);
	}
}

void Leds::init_channel(ChannelState& channel, LedMode mode) {
	channel.mode = mode;
	channel.initialized = true;
	channel.brightness = 0;
	channel.state = false;
	channel.blinking = false;
	channel.constant_blink = false;
	channel.duration_blink = false;
	channel.blink_count = 0;
	configure_channel_for_mode(channel);
	set_channel_brightness(channel, 0);
}

void Leds::set_channel_mode(ChannelState& channel, LedMode mode) {
	if (channel.mode == mode) {
		return;
	}

	channel.mode = mode;
	if (!channel.initialized) {
		return;
	}

	configure_channel_for_mode(channel);
	if (channel.mode == LedMode::kSimple) {
		gpio_put(channel.gpio_pin, channel.brightness > 0);
	} else {
		pwm_set_gpio_level(channel.gpio_pin, channel.brightness);
	}
}

void Leds::set_channel_brightness(ChannelState& channel, uint8_t value) {
	if (!channel.initialized) {
		init_channel(channel, channel.mode);
	}

	channel.brightness = value;
	if (channel.mode == LedMode::kSimple) {
		gpio_put(channel.gpio_pin, channel.brightness > 0);
	} else {
		pwm_set_gpio_level(channel.gpio_pin, channel.brightness);
	}
	channel.state = (channel.brightness > 0);
	if (channel.on_state_change) {
		channel.on_state_change(channel.state);
	}
}

void Leds::channel_on(ChannelState& channel) {
	set_channel_brightness(channel, 255);
}

void Leds::channel_off(ChannelState& channel) {
	set_channel_brightness(channel, 0);
}

void Leds::channel_toggle(ChannelState& channel) {
	if (channel.state) {
		channel_off(channel);
	} else {
		channel_on(channel);
	}
}

void Leds::channel_blink(ChannelState& channel, uint times, uint interval_ms) {
	channel.blinking = true;
	channel.constant_blink = false;
	channel.duration_blink = false;
	channel.blink_times = times;
	channel.blink_interval_ms = interval_ms;
	channel.blink_count = 0;
	channel.last_blink_time = get_absolute_time();
}

void Leds::channel_blink_duration(ChannelState& channel, uint duration_ms, uint interval_ms) {
	channel.blinking = true;
	channel.constant_blink = false;
	channel.duration_blink = true;
	channel.duration_ms = duration_ms;
	channel.blink_interval_ms = interval_ms;
	channel.blink_count = 0;
	channel.last_blink_time = get_absolute_time();
	channel.blink_start_time = get_absolute_time();
}

void Leds::channel_start_blink(ChannelState& channel, uint interval_ms) {
	channel.blinking = true;
	channel.constant_blink = true;
	channel.duration_blink = false;
	channel.blink_interval_ms = interval_ms;
	channel.last_blink_time = get_absolute_time();
}

void Leds::channel_stop_blink(ChannelState& channel) {
	channel.blinking = false;
	channel.constant_blink = false;
	channel.duration_blink = false;
	set_channel_brightness(channel, 0);
	if (channel.on_blink_end) {
		channel.on_blink_end();
	}
}

void Leds::channel_update(ChannelState& channel) {
	if (!channel.blinking) {
		return;
	}

	absolute_time_t now = get_absolute_time();
	if (absolute_time_diff_us(channel.last_blink_time, now) / 1000 >= channel.blink_interval_ms) {
		channel.last_blink_time = now;
		if (channel.state) {
			channel_off(channel);
			if (!channel.constant_blink && !channel.duration_blink) {
				channel.blink_count++;
			}
		} else {
			channel_on(channel);
		}

		if (!channel.constant_blink && !channel.duration_blink && channel.blink_count >= channel.blink_times) {
			channel_stop_blink(channel);
		}
	}

	if (channel.duration_blink && absolute_time_diff_us(channel.blink_start_time, now) / 1000 >= channel.duration_ms) {
		channel_stop_blink(channel);
	}
}
