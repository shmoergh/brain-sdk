#pragma once

#include <cstdint>

#include "brain.h"

class MidiToCvExample {
public:
	void init();
	void update();

private:
	Brain brain_;
	bool initialized_ = false;
};
