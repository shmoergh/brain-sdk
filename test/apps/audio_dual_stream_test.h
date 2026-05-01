#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

/**
 * @brief Dual-stream Pot + AudioProcessor sanity test.
 *
 * Wires:
 *   - Input 1 (audio/CV In A) -> dual DSP callback -> Output 1 (audio/CV Out A)
 *   - Input 2 (audio/CV In B) -> dual DSP callback -> Output 2 (audio/CV Out B)
 *   - Pot 1 (knob 0) controls Output 1 volume linearly (0..1).
 *   - Pot 2 (knob 1) controls Output 2 volume linearly (0..1).
 *
 * Each input passes through to its matching output, scaled by its own pot.
 * Use this to verify that the dual-stream `AudioProcessor::init(...)` works:
 * turning Pot 1 should fade only Output 1, turning Pot 2 should fade only
 * Output 2, with no audible bleed between the two streams.
 */
class AudioDualStreamTest {
public:
	void init();
	void update();

private:
	struct State {
		volatile uint8_t volume_q8_a = 0;  // 0..255 — read in audio ISR
		volatile uint8_t volume_q8_b = 0;
	};

	static void process_dual(
		DualStreamSamples* samples,
		const AudioProcessorFrame* frame,
		void* user_ctx);

	Brain brain_{};
	bool initialized_ = false;
	State state_{};
	uint32_t last_print_us_ = 0;
};

}  // namespace sandbox::apps
