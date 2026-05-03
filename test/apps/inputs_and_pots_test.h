#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

/**
 * @brief Manual test app exercising Inputs + Pots concurrently through AdcEngine.
 *
 * Initializes both subsystems and prints IN1/IN2 voltages alongside the three
 * buffered pot values. Used to verify that engine-shared sampling is stable
 * when both clients pull from the same snapshot.
 */
class InputsAndPotsTest {
public:
	void init();
	void update();

private:
	Brain brain_{};
	bool initialized_ = false;
	uint32_t last_print_us_ = 0;
};

}  // namespace sandbox::apps
