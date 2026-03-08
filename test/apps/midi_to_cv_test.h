#pragma once

#include <cstdint>

#include "brain-utils/midi-to-cv.h"

namespace sandbox::apps {

/**
 * @brief Manual sandbox app for testing MidiToCV.
 */
class MidiToCvTest {
public:
	void init();
	void update();

private:
	brain::utils::MidiToCV midi_to_cv_;
	bool initialized_ = false;
};

}  // namespace sandbox::apps
