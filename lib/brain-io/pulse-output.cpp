// Implementation of PulseOutput class for hardware-inverted digital output.
// Handles GPIO setup and safe initialization to prevent output glitches.

#include "brain-io/pulse-output.h"
#include <stdio.h>

#include "hardware/gpio.h"
#include "pico/types.h"

namespace brain::io {

PulseOutput::PulseOutput(uint out_gpio) :
	out_gpio_(out_gpio),
	current_output_state_(false) {}

void PulseOutput::begin() {
	printf("[PulseOutput] Initializing GPIO%d as OUTPUT\n", out_gpio_);

	// Configure output pin - set HIGH before switching to output to avoid glitch
	gpio_init(out_gpio_);
	gpio_put(out_gpio_, true);	// Set to idle (HIGH) first
	gpio_set_dir(out_gpio_, GPIO_OUT);
}

void PulseOutput::end() {
	// Return pin to input/high-impedance
	gpio_set_dir(out_gpio_, GPIO_IN);
	gpio_disable_pulls(out_gpio_);
}

void PulseOutput::set(bool on) {
	// Only change if different from current state (idempotent)
	if (on != current_output_state_) {
		current_output_state_ = on;
		// Output stage is active-low: true = LOW, false = HIGH
		gpio_put(out_gpio_, !on);
	}
}

bool PulseOutput::get() const {
	return current_output_state_;
}

}  // namespace brain::io
