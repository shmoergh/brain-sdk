#pragma once

#include "brain.h"

class SampleAndHold {

public:
	void init();
	void update();

private:
	void on_pulse_rise();
	Brain brain_;
	bool initialized_ = false;
};