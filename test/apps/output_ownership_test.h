#pragma once

#include <cstdint>

namespace sandbox::apps {

/**
 * @brief Manual test app for verifying per-channel ownership semantics in
 * Outputs + OutputEngine.
 *
 * Phased self-test:
 *   Phase A: both channels kManual (default). set_voltage_* on A and B succeed.
 *   Phase B: flip A to kAudio. set_voltage_*(A, ...) must return false.
 *            set_voltage_*(B, ...) must still succeed. write_audio_sample(A, ...)
 *            drives a slow triangle on A while B holds steady. Operator scopes
 *            both outputs and confirms.
 *   Phase C: flip A back to kManual. set_voltage_*(A, 5000) must succeed and
 *            snap A to 5 V.
 *
 * Each phase prints PASS / FAIL based on return-value assertions and pauses
 * a few seconds for visual verification on the scope.
 */
class OutputOwnershipTest {
public:
	void init();
	void update();

private:
	enum class Phase : uint8_t {
		kPhaseA = 0,
		kPhaseBSetup = 1,
		kPhaseBDrive = 2,
		kPhaseC = 3,
		kIdle = 4,
	};

	void enter_phase_a();
	void enter_phase_b_setup();
	void run_phase_b_drive_step();
	void enter_phase_c();
	void enter_idle();

	bool initialized_ = false;
	Phase phase_ = Phase::kPhaseA;
	uint32_t phase_started_us_ = 0;
	uint32_t last_audio_step_us_ = 0;
	uint16_t triangle_value_ = 0;
	bool triangle_up_ = true;
};

}  // namespace sandbox::apps
