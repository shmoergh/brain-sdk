#pragma once

#include <stdint.h>
#include <vector>

#include "common.h"
#include "led.h"

constexpr uint8_t NO_OF_LEDS = 6;
constexpr uint8_t led_pins[NO_OF_LEDS] = {
	GPIO_BRAIN_LED_1,
	GPIO_BRAIN_LED_2,
	GPIO_BRAIN_LED_3,
	GPIO_BRAIN_LED_4,
	GPIO_BRAIN_LED_5,
	GPIO_BRAIN_LED_6
};

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
	void blink_duration(uint8_t led, uint duration_ms, uint interval_ms);
	void start_blink(uint8_t led, uint interval_ms);
	void stop_blink(uint8_t led);

	void set_from_mask(uint8_t mask);
	void on_all();
	void off_all();
	void startup_animation();

	bool is_on(uint8_t led);
	bool is_blinking(uint8_t led);

private:
	std::vector<Led> leds_;
	LedMode mode_;

	bool validate_led(uint8_t led);
};

namespace brain {
namespace ui {
using ::NO_OF_LEDS;
using ::led_pins;
using ::Leds;
}  // namespace ui
}  // namespace brain
