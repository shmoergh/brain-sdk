#pragma once

#include <cstdint>
#include <functional>

#include "gpio-setup.h"
#include "pico/types.h"

class Pulse {
public:
	Pulse(uint in_gpio = GPIO_BRAIN_PULSE_INPUT, uint out_gpio = GPIO_BRAIN_PULSE_OUTPUT);

	void begin();
	void end();
	bool read() const;
	bool read_raw() const;
	void set(bool on);
	bool get() const;
	void on_rise(std::function<void()> cb);
	void on_fall(std::function<void()> cb);
	void poll();
	void set_input_glitch_filter_us(uint32_t us);
	void enable_interrupts();
	void disable_interrupts();

private:
	uint in_gpio_;
	uint out_gpio_;
	bool last_logical_state_;
	bool current_output_state_;
	uint32_t glitch_filter_us_;
	bool interrupts_enabled_;

	std::function<void()> on_rise_callback_;
	std::function<void()> on_fall_callback_;

	uint32_t last_change_time_us_;
	bool filtered_state_;

	static void gpio_irq_handler(uint gpio, uint32_t events);
	void handle_edge(bool raw_state);
};

namespace brain {
namespace io {
using ::Pulse;
}  // namespace io
}  // namespace brain
