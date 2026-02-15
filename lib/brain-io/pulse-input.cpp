// Implementation of PulseInput class for hardware-inverted digital input.
// Handles GPIO setup, edge detection, and interrupt-driven callbacks.

#include "brain-io/pulse-input.h"
#include <stdio.h>

#include <algorithm>

#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "pico/time.h"
#include "pico/types.h"

namespace brain::io {

// Static instance tracking for IRQ handling
static PulseInput* irq_instances[NUM_BANK0_GPIOS] = {nullptr};

PulseInput::PulseInput(uint in_gpio) :
	in_gpio_(in_gpio),
	last_logical_state_(false),
	glitch_filter_us_(0),
	interrupts_enabled_(false),
	last_change_time_us_(0),
	filtered_state_(false) {}

void PulseInput::begin() {
	printf("[PulseInput] Initializing GPIO%d as INPUT\n", in_gpio_);

	// Configure input pin with pull-up
	gpio_init(in_gpio_);
	gpio_set_dir(in_gpio_, GPIO_IN);
	gpio_pull_up(in_gpio_);

	// Initialize state
	last_logical_state_ = read();
	filtered_state_ = last_logical_state_;
	last_change_time_us_ = time_us_32();

	// Register this instance for IRQ handling
	if (in_gpio_ < NUM_BANK0_GPIOS) {
		irq_instances[in_gpio_] = this;
	}
}

void PulseInput::end() {
	// Disable interrupts if enabled
	if (interrupts_enabled_) {
		disable_interrupts();
	}

	// Clear IRQ instance
	if (in_gpio_ < NUM_BANK0_GPIOS) {
		irq_instances[in_gpio_] = nullptr;
	}

	// Return pin to input/high-impedance
	gpio_set_dir(in_gpio_, GPIO_IN);
	gpio_disable_pulls(in_gpio_);
}

bool PulseInput::read() const {
	// Input is inverted by hardware transistor
	return !gpio_get(in_gpio_);
}

bool PulseInput::read_raw() const {
	return gpio_get(in_gpio_);
}

void PulseInput::on_rise(std::function<void()> cb) {
	on_rise_callback_ = cb;
}

void PulseInput::on_fall(std::function<void()> cb) {
	on_fall_callback_ = cb;
}

void PulseInput::poll() {
	bool current_logical = read();

	// Apply glitch filtering if enabled
	if (glitch_filter_us_ > 0) {
		uint32_t now = time_us_32();

		if (current_logical != filtered_state_) {
			// State changed - start/continue filtering
			if (current_logical != last_logical_state_) {
				// New change - reset timer
				last_change_time_us_ = now;
			} else if ((now - last_change_time_us_) >= glitch_filter_us_) {
				// Change has been stable long enough
				filtered_state_ = current_logical;
			}
		}

		current_logical = filtered_state_;
	}

	// Detect edges and fire callbacks
	if (current_logical != last_logical_state_) {
		if (current_logical && on_rise_callback_) {
			on_rise_callback_();
		} else if (!current_logical && on_fall_callback_) {
			on_fall_callback_();
		}

		last_logical_state_ = current_logical;
	}
}

void PulseInput::set_input_glitch_filter_us(uint32_t us) {
	glitch_filter_us_ = us;
}

void PulseInput::enable_interrupts() {
	if (!interrupts_enabled_) {
		gpio_set_irq_enabled_with_callback(
			in_gpio_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);
		interrupts_enabled_ = true;
	}
}

void PulseInput::disable_interrupts() {
	if (interrupts_enabled_) {
		gpio_set_irq_enabled(in_gpio_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
		interrupts_enabled_ = false;
	}
}

void PulseInput::gpio_irq_handler(uint gpio, uint32_t events) {
	if (gpio < NUM_BANK0_GPIOS && irq_instances[gpio] != nullptr) {
		// In ISR context - just record that an edge occurred
		// The actual edge processing happens in poll() in main loop
		bool raw_state = gpio_get(gpio);
		irq_instances[gpio]->handle_edge(raw_state);
	}
}

void PulseInput::handle_edge(bool raw_state) {
	// This is called from ISR - keep it minimal
	// The actual callback invocation happens in poll()
	// Just update timing for glitch filter
	last_change_time_us_ = time_us_32();
}

}  // namespace brain::io
