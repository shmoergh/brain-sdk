#pragma once

#include "brain/brain.h"

namespace sandbox::apps {
class Blink {
public:
	explicit Blink(unsigned int interval_ms = 500);

	void init();
	void update();

private:
	Brain brain_;
	unsigned int interval_ms_;
	bool led_on_;
};
} // namespace sandbox::apps
