#pragma once

#include <cstdint>

#include "brain.h"

class StorageExample {
public:
	void init();
	void update();

private:
	struct DemoBlob {
		uint32_t magic;
		uint8_t mode;
		uint8_t brightness;
		uint16_t reserved;
	};

	Brain brain_;
	bool initialized_ = false;
	bool led_state_ = false;
	uint32_t last_toggle_ms_ = 0;
};
