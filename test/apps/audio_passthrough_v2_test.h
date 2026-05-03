#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

class AudioPassthroughV2Test {
public:
	void init();
	void update();

private:
	static void process_frame(
		int16_t in1,
		int16_t in2,
		const AudioProcessorFrameV2* frame,
		int16_t* out_a,
		int16_t* out_b,
		void* user_ctx);

	Brain brain_{};
	bool initialized_ = false;
	uint32_t last_print_us_ = 0;
};

}  // namespace sandbox::apps
