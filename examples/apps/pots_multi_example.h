#pragma once

#include <cstdint>

#include "brain.h"

namespace examples::apps {

class PotsMultiExample {
public:
	void init();
	void update();

private:
	enum FunctionId : uint8_t {
		kCutoff = 1,
		kResonance = 2
	};

	Brain brain_;
	bool initialized_ = false;
	bool edit_cutoff_ = true;
	uint32_t last_print_ms_ = 0;
	uint32_t last_switch_ms_ = 0;
};

}  // namespace examples::apps
