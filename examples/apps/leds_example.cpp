#include "leds_example.h"

#include <stdio.h>

#include "pico/stdlib.h"

void LedsExample::init() {
	printf("\n--------\n");
	printf("Example: LED running light\n");

	if (!brain_init_succeeded(brain_.init_leds(LedMode::kSimple))) {
		printf("[ERROR] init_leds failed\n");
		return;
	}

	brain_.leds.off_all();
	initialized_ = true;
	last_step_ms_ = to_ms_since_boot(get_absolute_time());
}

void LedsExample::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.update_leds();

	const uint32_t now = to_ms_since_boot(get_absolute_time());
	if ((now - last_step_ms_) >= 120) {
		last_step_ms_ = now;
		brain_.leds.off_all();
		brain_.leds.on(index_);
		index_ = static_cast<uint8_t>((index_ + 1) % kLedCount);
	}
}
