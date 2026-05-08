#include "basic_pot_reads_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace sandbox::apps {

void BasicPotReadsTest::init() {
	stdio_init_all();
	sleep_ms(200);

	printf("\n\r--------\n\r");
	printf("Basic pot reads (engine-driven, buffered)\n");
	printf("Move pots to verify independent tracking. Stable when still.\n\n");

	BrainInitStatus status = brain_.init_pots(create_default_pots_config(3, 8));
	initialized_ = brain_init_succeeded(status);
	if (!initialized_) {
		printf("[ERROR] Failed to initialize pots.\n");
	}
}

void BasicPotReadsTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.update_pots();

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) < 100000) {
		return;
	}
	last_print_us_ = now_us;

	const uint16_t pot1 = brain_.pots.get_buffered(0);
	const uint16_t pot2 = brain_.pots.get_buffered(1);
	const uint16_t pot3 = brain_.pots.get_buffered(2);

	printf(
		"\rPOT1=%3u POT2=%3u POT3=%3u    ",
		static_cast<unsigned>(pot1),
		static_cast<unsigned>(pot2),
		static_cast<unsigned>(pot3));
	fflush(stdout);
}

}  // namespace sandbox::apps
