#pragma once

#include <cstdint>

#include "brain.h"

class MidiParserExample {
public:
	void init();
	void update();

private:
	Brain brain_;
	bool initialized_ = false;
};
