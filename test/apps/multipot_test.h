#pragma once

#include <cstdint>

#include "brain-ui/button.h"
#include "brain-ui/leds.h"
#include "brain-ui/pot-multi-function.h"
#include "brain-ui/pots.h"

namespace sandbox::apps {

/**
 * @brief Manual sandbox app for testing PotMultiFunction modes.
 *
 * Uses one source pot with button-selected functions, LED feedback,
 * and periodic serial status output.
 * PotMultiFunction mode defines how physical pot movement maps to the
 * logical value: Direct tracks pot position immediately, Pickup waits until
 * the pot crosses the stored value, and ValueScale changes value incrementally
 * relative to movement direction.
 */
class MultipotTest {
public:
	/**
	 * @brief Construct the sandbox app and initialize local state.
	 */
	MultipotTest();

	/**
	 * @brief Initialize board I/O and register PotMultiFunction functions.
	 *
	 * Sets up buttons, LEDs, pot scanner, and logical functions.
	 */
	void init();

	/**
	 * @brief Execute one app update tick.
	 *
	 * Scans inputs, resolves active function, updates PotMultiFunction,
	 * refreshes LEDs, and emits periodic debug output.
	 */
	void update();

private:
	brain::ui::Button button_a_;
	brain::ui::Button button_b_;
	brain::ui::Leds leds_;
	brain::ui::Pots pots_;
	brain::ui::PotMultiFunction multi_;

	bool button_a_pressed_;
	bool button_b_pressed_;
	uint32_t last_print_us_;
	uint16_t last_status_len_;

	/**
	 * @brief Register function-specific PotMultiFunction mappings.
	 */
	void register_functions();

	/**
	 * @brief Resolve active function from button state.
	 *
	 * @return Function index (velocity/tempo/scale)
	 */
	uint8_t resolve_function() const;

	/**
	 * @brief Render current mode/function state on LED mask.
	 *
	 * @param function_index Active function index
	 */
	void render_leds(uint8_t function_index);

	/**
	 * @brief Print throttled serial status line for current function/value.
	 *
	 * @param function_index Active function index
	 */
	void print_status(uint8_t function_index);
};

}  // namespace sandbox::apps
