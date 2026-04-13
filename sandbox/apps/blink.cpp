#include "blink.h"

#include "pico/stdlib.h"

namespace sandbox::apps {
Blink::Blink(unsigned int interval_ms)
	: brain_(), interval_ms_(interval_ms), led_on_(false) {}

void Blink::init() {
	brain_.leds.init(LedMode::kSimple);
	brain_.leds.off_all();
}

void Blink::update() {
	led_on_ = !led_on_;
	if (led_on_) {
		brain_.leds.on(0);
	} else {
		brain_.leds.off(0);
	}
	sleep_ms(interval_ms_);
}
} // namespace sandbox::apps
