#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

/**
 * @brief Manual test app for verifying buffered pot reads through AdcEngine.
 *
 * Initializes Pots only and prints three buffered values continuously. Used to
 * verify that the engine's settle/average state machine produces independent,
 * stable readings — no cross-channel bleed when pots are still.
 */
class BasicPotReadsTest {
public:
	void init();
	void update();

private:
	Brain brain_{};
	bool initialized_ = false;
	uint32_t last_print_us_ = 0;
};

}  // namespace sandbox::apps
