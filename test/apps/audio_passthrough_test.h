#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

class AudioPassthroughTest {
public:
	void init();
	void update();

private:
	static int16_t process_sample(
		int16_t input_sample,
		const AudioProcessorFrame* frame,
		void* user_ctx);

	Brain brain_{};
	bool initialized_ = false;
	uint32_t last_print_us_ = 0;
};

}  // namespace sandbox::apps
