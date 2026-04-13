#pragma once

#include <cstdint>

#include "pot-multi-function-core.h"
#include "pots-core.h"

inline PotsConfig create_default_pots_config(
	uint8_t num_pots = 3,
	uint8_t output_resolution = kDefaultPotsOutputResolution) {
	return create_default_config(num_pots, output_resolution);
}
