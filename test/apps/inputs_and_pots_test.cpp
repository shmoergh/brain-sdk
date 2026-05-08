#include "inputs_and_pots_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace sandbox::apps {

void InputsAndPotsTest::init() {
	stdio_init_all();
	sleep_ms(200);

	printf("\n\r--------\n\r");
	printf("Inputs + Pots concurrent test (engine-driven)\n");
	printf("Wiggle IN1/IN2 (CV inputs) and pots; values should be independent and stable.\n\n");

	BrainInitStatus inputs_status = brain_.init_inputs();
	BrainInitStatus pots_status = brain_.init_pots(create_default_pots_config(3, 8));

	initialized_ = brain_init_succeeded(inputs_status) && brain_init_succeeded(pots_status);
	if (!initialized_) {
		printf("[ERROR] init_inputs=%d init_pots=%d\n",
			static_cast<int>(inputs_status),
			static_cast<int>(pots_status));
	}
}

void InputsAndPotsTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.update_inputs();
	brain_.update_pots();

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) < 100000) {
		return;
	}
	last_print_us_ = now_us;

	const int32_t in1_mv = brain_.inputs.get_voltage_millivolts_channel_a();
	const int32_t in2_mv = brain_.inputs.get_voltage_millivolts_channel_b();
	const uint16_t pot1 = brain_.pots.get_buffered(0);
	const uint16_t pot2 = brain_.pots.get_buffered(1);
	const uint16_t pot3 = brain_.pots.get_buffered(2);

	printf(
		"\rIN1=%+5ldmV IN2=%+5ldmV  POT1=%3u POT2=%3u POT3=%3u    ",
		static_cast<long>(in1_mv),
		static_cast<long>(in2_mv),
		static_cast<unsigned>(pot1),
		static_cast<unsigned>(pot2),
		static_cast<unsigned>(pot3));
	fflush(stdout);
}

}  // namespace sandbox::apps
