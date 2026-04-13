#pragma once

#include <cstdint>

#include "brain.h"

class InputsExample {
public:
	void init();
	void update();

private:
	Brain brain_;
	bool initialized_ = false;
	uint32_t last_print_ms_ = 0;
};
