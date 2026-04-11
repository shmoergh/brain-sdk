#pragma once

#include <cstdint>

#include "brain.h"

namespace examples::apps {

class MidiToCvExample {
public:
	void init();
	void update();

private:
	Brain brain_;
	bool initialized_ = false;
};

}  // namespace examples::apps
