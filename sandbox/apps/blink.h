#pragma once

namespace sandbox::apps {
class Blink {
public:
	explicit Blink(unsigned int interval_ms = 500);

	void init();
	void update();

private:
	unsigned int interval_ms_;
	bool led_on_;
	bool led_available_;
};
} // namespace sandbox::apps
