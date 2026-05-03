#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

class AudioProcessorTest {
public:
	void init();
	void update();

private:
	struct EffectState {
		int32_t lowpass_state = 0;
	};

	static int16_t process_sample(
		int16_t input_sample,
		const AudioProcessorFrame* frame,
		void* user_ctx);

	Brain brain_{};
	bool initialized_ = false;
	EffectState effect_state_{};
	uint32_t last_print_us_ = 0;
};

}  // namespace sandbox::apps
