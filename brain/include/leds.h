#pragma once

#include <cstdint>

#include "button-led.h"
#include "led.h"
#include "leds-core.h"

constexpr uint8_t kLedCount = NO_OF_LEDS;

constexpr uint8_t led_pin(uint8_t index) {
	return index < NO_OF_LEDS ? led_pins[index] : 0;
}
