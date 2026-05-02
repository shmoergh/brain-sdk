#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

/**
 * @brief Simple Pot + AudioProcessor sanity test.
 *
 * Wires:
 *   - Input 1  (audio/CV In A)  -> DSP callback
 *   - Pot 1    (knob index 0)   -> volume (0..1)
 *   - Output 1 (audio/CV Out A) <- DSP callback output
 *
 * Audio is a pure passthrough scaled by `pot 1`'s mapped value. Use this
 * to verify that pots and AudioProcessor work together: turning pot 1
 * should fade the audio from silent (full CCW) to unity (full CW)
 * without artifacts.
 */
class AudioVolumeTest {
public:
	void init();
	void update();

private:
	struct State {
		volatile int16_t last_input_sample = 0;
		volatile uint32_t sample_count = 0;
		volatile uint32_t spike_count = 0;
		volatile uint32_t max_abs_delta = 0;
	};

	static int16_t process_sample(
		int16_t input_sample,
		const AudioProcessorFrame* frame,
		void* user_ctx);

	Brain brain_{};
	bool initialized_ = false;
	State state_{};
	uint32_t last_print_us_ = 0;
};

}  // namespace sandbox::apps
