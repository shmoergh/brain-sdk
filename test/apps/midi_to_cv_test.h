#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

/**
 * @brief Manual sandbox app for testing MidiToCV.
 */
class MidiToCvTest {
public:
	void init();
	void update();

private:
	Brain brain_;
	bool initialized_ = false;
};

}  // namespace sandbox::apps
