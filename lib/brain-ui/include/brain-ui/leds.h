#ifndef BRAIN_UI_LEDS_H_
#define BRAIN_UI_LEDS_H_

#include <stdint.h>
#include <vector>

#include "brain-common/brain-common.h"
#include "brain-ui/led.h"

/**
 * A helper class to manage leds in the Brain module
 */
namespace brain::ui {

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
		void init();
		void update();

		// Single LED methods
		void on(uint8_t led);
		void off(uint8_t led);
		void toggle(uint8_t led);
		void set_brightness(uint8_t led, uint8_t brightness);
		void blink_duration(uint8_t led, uint duration_ms, uint interval_ms);
		void start_blink(uint8_t led, uint interval_ms);
		void stop_blink(uint8_t led);

		// Multi led methods
		void on_mask(uint8_t mask);
		void off_mask(uint8_t mask);
		void on_all();
		void off_all();

		// Animations
		void startup_animation();

		bool is_on(uint8_t led);
		bool is_blinking(uint8_t led);

	private:
		std::vector<Led> leds_;

		bool validate_led(uint8_t led);
};

}

#endif