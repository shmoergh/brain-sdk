#pragma once

#include "include/buttons.h"
#include "include/constants.h"
#include "include/inputs.h"
#include "include/leds.h"
#include "include/outputs.h"
#include "include/pots.h"
#include "include/storage.h"

class Brain {
public:
	Brain()
		: leds(false),
		  buttons(),
		  outputs(),
		  inputs(),
		  pots() {}

	bool init() {
		leds.init(LedMode::kPwm);
		buttons.init();
		outputs.init();
		inputs.init();
		pots.init(create_default_pots_config());
		return true;
	}

	void update() {
		leds.update();
		buttons.update();
		inputs.update();
		pots.scan();
	}

	Leds leds;
	Buttons buttons;
	Outputs outputs;
	Inputs inputs;
	Pots pots;
};
