// Pulse output handler with hardware inversion support for Brain module.
// Provides digital output control with transistor-inverted signals.
// Requires: GPIO pin for output (active-low).

#ifndef BRAIN_IO_PULSE_OUTPUT_H_
#define BRAIN_IO_PULSE_OUTPUT_H_

#include <cstdint>

#include "brain-common/brain-gpio-setup.h"
#include "pico/types.h"

namespace brain::io {

/**
 * @brief Pulse output handler with hardware inversion support
 *
 * Provides a simple API for driving a transistor-inverted digital output.
 * The SDK handles inversion transparently.
 */
class PulseOutput {
	public:
	/**
	 * @brief Construct a new PulseOutput object
	 *
	 * @param out_gpio GPIO pin number for output (default: GPIO_BRAIN_PULSE_OUTPUT)
	 */
	PulseOutput(uint out_gpio = GPIO_BRAIN_PULSE_OUTPUT);

	/**
	 * @brief Initialize GPIO pin and set safe output state
	 */
	void begin();

	/**
	 * @brief Return pin to input/high-impedance state
	 */
	void end();

	/**
	 * @brief Set logical output state
	 *
	 * @param on true to assert output (active), false to de-assert (idle)
	 */
	void set(bool on);

	/**
	 * @brief Get last commanded logical output state
	 *
	 * @return true if output was set to active, false if idle
	 */
	bool get() const;

	private:
	uint out_gpio_;
	bool current_output_state_;
};

}  // namespace brain::io

#endif	// BRAIN_IO_PULSE_OUTPUT_H_
