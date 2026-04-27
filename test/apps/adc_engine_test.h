#pragma once

#include <cstdint>

#include "brain.h"

namespace sandbox::apps {

/**
 * @brief Manual hardware test app for the 2.1 AdcEngine refactor.
 *
 * Verifies, in one pass:
 * - `Pots`, `Inputs`, `PotMultiFunction`, and `AudioProcessor` can all be
 *   initialized in any order on the same `Brain` instance.
 * - Legacy no-op shims still compile and don't crash.
 * - `pots.get(i)`, `inputs.get_raw(i)`, and the audio processor's
 *   `get_pot_raw_u8(i)` are non-blocking (well under 10 µs per call).
 * - Pot and CV values stay fresh without ever calling `update_pots()`,
 *   `pots.scan()`, or `inputs.update_audio_cv()`.
 * - All four components run concurrently under audio load with no
 *   `AudioProcessorStats::overrun_count` accumulation.
 */
class AdcEngineTest {
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

	bool run_concurrent_init_checks();
	void run_legacy_shim_checks();
	void run_latency_probes();
	void run_freshness_check();

	Brain brain_{};
	bool initialized_ = false;
	EffectState effect_state_{};
	uint32_t last_print_us_ = 0;
	uint64_t baseline_overruns_ = 0;
};

}  // namespace sandbox::apps
