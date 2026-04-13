#pragma once

#include <cstdint>

#include "brain.h"

class AudioProcessorExample {
public:
	void init();
	void update();

private:
	struct EffectState {
		int32_t lp = 0;
	};

	static int16_t process_sample(int16_t in, const AudioProcessorFrame* frame, void* user_ctx);

	Brain brain_;
	EffectState state_{};
	bool initialized_ = false;
	uint32_t last_print_ms_ = 0;
};
