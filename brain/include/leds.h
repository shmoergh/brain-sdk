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
	explicit Leds(LedMode mode);
	explicit Leds(bool simple_mode = false);

	void init();
	void init(LedMode mode);
	void set_mode(LedMode mode);
	LedMode get_mode() const;
	void update();

	void on(uint8_t led);
	void off(uint8_t led);
	void toggle(uint8_t led);
	void set_brightness(uint8_t led, uint8_t brightness);
	void blink(uint8_t led, uint times, uint interval_ms);
	void blink_duration(uint8_t led, uint duration_ms, uint interval_ms);
	void start_blink(uint8_t led, uint interval_ms);
	void stop_blink(uint8_t led);

	void set_on_state_change(uint8_t led, std::function<void(bool)> callback);
	void set_on_blink_end(uint8_t led, std::function<void()> callback);

	void set_from_mask(uint8_t mask);
	void on_all();
	void off_all();
	void startup_animation();

	bool is_on(uint8_t led) const;
	bool is_blinking(uint8_t led) const;

	void button_init();
	void button_on();
	void button_off();
	void button_toggle();
	void button_set_brightness(uint8_t brightness);
	void button_blink(uint times, uint interval_ms);
	void button_blink_duration(uint duration_ms, uint interval_ms);
	void button_start_blink(uint interval_ms);
	void button_stop_blink();
	bool button_is_on() const;
	bool button_is_blinking() const;
	void button_set_on_state_change(std::function<void(bool)> callback);
	void button_set_on_blink_end(std::function<void()> callback);

private:
	struct ChannelState {
		uint gpio_pin = 0;
		LedMode mode = LedMode::kSimple;
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

