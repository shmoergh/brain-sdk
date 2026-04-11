#include "pots_multi_example.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace examples::apps {

void PotsMultiExample::init() {
	printf("\n--------\n");
	printf("Example: PotMultiFunction (pot0 switches cutoff/resonance target every 2s)\n");

	if (!brain_init_succeeded(brain_.init_pot_multi())) {
		printf("[ERROR] init_pot_multi failed\n");
		return;
	}

	PotFunctionConfig cutoff{};
	cutoff.function_id = kCutoff;
	cutoff.pot_index = 0;
	cutoff.min_value = 20;
	cutoff.max_value = 20000;
	cutoff.initial_value = 800;
	cutoff.mode = PotMode::kValueScale;
	cutoff.pickup_hysteresis = 1;

	PotFunctionConfig resonance{};
	resonance.function_id = kResonance;
	resonance.pot_index = 0;
	resonance.min_value = 0;
	resonance.max_value = 100;
	resonance.initial_value = 30;
	resonance.mode = PotMode::kValueScale;
	resonance.pickup_hysteresis = 1;

	const bool ok_a = brain_.pot_multi.register_function(cutoff);
	const bool ok_b = brain_.pot_multi.register_function(resonance);
	if (!ok_a || !ok_b) {
		printf("[ERROR] register_function failed\n");
		return;
	}

	initialized_ = true;
	const uint32_t now = to_ms_since_boot(get_absolute_time());
	last_print_ms_ = now;
	last_switch_ms_ = now;
}

void PotsMultiExample::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now = to_ms_since_boot(get_absolute_time());
	if ((now - last_switch_ms_) >= 2000) {
		last_switch_ms_ = now;
		edit_cutoff_ = !edit_cutoff_;
	}

	brain_.pot_multi.set_active_function(0, edit_cutoff_ ? kCutoff : kResonance);
	brain_.update_pot_multi(true);

	if ((now - last_print_ms_) >= 120) {
		last_print_ms_ = now;
		printf(
			"\ractive=%s cutoff=%ld resonance=%ld      ",
			edit_cutoff_ ? "cutoff" : "resonance",
			static_cast<long>(brain_.pot_multi.get_value(kCutoff)),
			static_cast<long>(brain_.pot_multi.get_value(kResonance)));
		fflush(stdout);
	}
}

}  // namespace examples::apps
