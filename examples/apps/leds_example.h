#pragma once

#include <cstdint>

#include "brain.h"

namespace examples::apps {

class LedsExample {
public:
	void init();
	void update();

private:
	Brain brain_;
	bool initialized_ = false;
	uint8_t index_ = 0;
	uint32_t last_step_ms_ = 0;
};

}  // namespace examples::apps
