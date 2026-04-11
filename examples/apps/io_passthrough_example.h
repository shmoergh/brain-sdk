#pragma once

#include <cstdint>

#include "brain.h"

namespace examples::apps {

class IoPassthroughExample {
public:
	void init();
	void update();

private:
	Brain brain_;
	bool initialized_ = false;
	uint32_t last_print_ms_ = 0;
};

}  // namespace examples::apps
