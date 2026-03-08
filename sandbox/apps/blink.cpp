#include "blink.h"

#include "pico/stdlib.h"

namespace sandbox::apps {
Blink::Blink(unsigned int interval_ms)
	: interval_ms_(interval_ms), led_on_(false), led_available_(false) {}

void Blink::init() {
#if defined(PICO_DEFAULT_LED_PIN)
	led_available_ = true;
	gpio_init(PICO_DEFAULT_LED_PIN);
	gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
	gpio_put(PICO_DEFAULT_LED_PIN, 0);
#else
	led_available_ = false;
#endif
}

void Blink::update() {
	if (!led_available_) {
		sleep_ms(500);
		return;
	}

	led_on_ = !led_on_;
	gpio_put(PICO_DEFAULT_LED_PIN, led_on_ ? 1 : 0);
	sleep_ms(interval_ms_);
}
} // namespace sandbox::apps
