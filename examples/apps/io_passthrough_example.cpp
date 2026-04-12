#include "io_passthrough_example.h"

#include <stdio.h>

#include "pico/stdlib.h"

void IoPassthroughExample::init() {
	printf("\n--------\n");
	printf("Example: Input A -> Output A passthrough (mV)\n");

	if (!brain_init_succeeded(brain_.init_inputs())) {
		printf("[ERROR] init_inputs failed\n");
		return;
	}
	if (!brain_init_succeeded(brain_.init_outputs())) {
		printf("[ERROR] init_outputs failed\n");
		return;
	}
	if (!brain_init_succeeded(brain_.init_leds(kLedsModeSimple))) {
		printf("[ERROR] init_leds failed\n");
		return;
	}

	brain_.outputs.set_output_range(kOutputsChannelA, kOutputsRangeMinus5To5V);
	brain_.leds.off_all();
	initialized_ = true;
	last_print_ms_ = to_ms_since_boot(get_absolute_time());
}

void IoPassthroughExample::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.update_inputs();
	brain_.update_leds();

	const int32_t in_mv = brain_.inputs.get_voltage_millivolts_channel_a();
	const bool ok = brain_.outputs.set_voltage_millivolts(kOutputsChannelA, in_mv);
	if (!ok) {
		brain_.leds.on(0);
	} else {
		brain_.leds.off(0);
	}

	const uint32_t now = to_ms_since_boot(get_absolute_time());
	if ((now - last_print_ms_) >= 160) {
		last_print_ms_ = now;
		printf(
			"\rInA=%6ldmV -> OutA (%s)      ",
			static_cast<long>(in_mv),
			ok ? "ok" : "out-of-range");
		fflush(stdout);
	}
}
