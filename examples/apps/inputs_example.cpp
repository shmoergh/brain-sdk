#include "inputs_example.h"

#include <stdio.h>

#include "pico/stdlib.h"

void InputsExample::init() {
	printf("\n--------\n");
	printf("Example: Inputs monitor (A/B raw + mV, pulse state)\n");

	if (!brain_init_succeeded(brain_.init_inputs())) {
		printf("[ERROR] init_inputs failed\n");
		return;
	}

	brain_.inputs.pulse_set_input_glitch_filter_us(200);
	initialized_ = true;
	last_print_ms_ = to_ms_since_boot(get_absolute_time());
}

void InputsExample::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.update_inputs();

	const uint32_t now = to_ms_since_boot(get_absolute_time());
	if ((now - last_print_ms_) >= 120) {
		last_print_ms_ = now;
		printf(
			"\rA raw=%4u mv=%6ld | B raw=%4u mv=%6ld | pulse=%u      ",
			static_cast<unsigned>(brain_.inputs.get_raw_channel_a()),
			static_cast<long>(brain_.inputs.get_voltage_millivolts_channel_a()),
			static_cast<unsigned>(brain_.inputs.get_raw_channel_b()),
			static_cast<long>(brain_.inputs.get_voltage_millivolts_channel_b()),
			brain_.inputs.pulse_read() ? 1u : 0u);
		fflush(stdout);
	}
}
