#include "output_ownership_test.h"

#include <stdio.h>

#include <pico/stdlib.h>

#include "brain.h"
#include "output-engine.h"

namespace sandbox::apps {

namespace {

constexpr uint32_t kPhaseBDriveDurationUs = 5'000'000;
constexpr uint32_t kPhaseHoldUs = 2'000'000;
constexpr uint32_t kAudioStepUs = 200;       // 5 kHz update => keeps render IRQ fed.
constexpr uint16_t kTriangleStep = 24;       // ~85 ms top-to-bottom at 5 kHz.

Brain brain;

void print_pass_fail(const char* label, bool ok) {
	printf("  %s ... %s\n", label, ok ? "PASS" : "FAIL");
}

}  // namespace

void OutputOwnershipTest::init() {
	stdio_init_all();
	sleep_ms(200);

	printf("\n\r--------\n\r");
	printf("Output ownership test (Slice 3)\n");
	printf("Verifies per-channel kManual / kAudio ownership and the\n");
	printf("set_voltage_* / write_audio_sample contract.\n\n");

	const auto status = brain.init_outputs();
	initialized_ = brain_init_succeeded(status);
	if (!initialized_) {
		printf("[ERROR] Brain::init_outputs failed (%d)\n", static_cast<int>(status));
		return;
	}

	enter_phase_a();
}

void OutputOwnershipTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	const uint32_t in_phase_us = now_us - phase_started_us_;

	switch (phase_) {
		case Phase::kPhaseA:
			if (in_phase_us >= kPhaseHoldUs) {
				enter_phase_b_setup();
			}
			break;
		case Phase::kPhaseBSetup:
			// Setup runs once in enter_phase_b_setup; transition immediately.
			phase_ = Phase::kPhaseBDrive;
			phase_started_us_ = now_us;
			last_audio_step_us_ = now_us;
			triangle_value_ = 0;
			triangle_up_ = true;
			break;
		case Phase::kPhaseBDrive:
			if ((now_us - last_audio_step_us_) >= kAudioStepUs) {
				last_audio_step_us_ = now_us;
				run_phase_b_drive_step();
			}
			if (in_phase_us >= kPhaseBDriveDurationUs) {
				enter_phase_c();
			}
			break;
		case Phase::kPhaseC:
			if (in_phase_us >= kPhaseHoldUs) {
				enter_idle();
			}
			break;
		case Phase::kIdle:
			sleep_ms(50);
			break;
	}
}

void OutputOwnershipTest::enter_phase_a() {
	printf("[Phase A] Both channels kManual (default).\n");

	const bool default_a_ok =
		brain.outputs.get_channel_owner(kOutputsChannelA) == kOutputsOwnerManual;
	const bool default_b_ok =
		brain.outputs.get_channel_owner(kOutputsChannelB) == kOutputsOwnerManual;
	print_pass_fail("default owner A == kManual", default_a_ok);
	print_pass_fail("default owner B == kManual", default_b_ok);

	const bool wrote_a = brain.outputs.set_voltage_millivolts(kOutputsChannelA, 2500);
	const bool wrote_b = brain.outputs.set_voltage_millivolts(kOutputsChannelB, 7500);
	print_pass_fail("set_voltage A=2500mV returns true", wrote_a);
	print_pass_fail("set_voltage B=7500mV returns true", wrote_b);
	printf("  -> Scope: A ~= 2.5V, B ~= 7.5V (holding steady)\n\n");

	phase_ = Phase::kPhaseA;
	phase_started_us_ = to_us_since_boot(get_absolute_time());
}

void OutputOwnershipTest::enter_phase_b_setup() {
	printf("[Phase B] Flip A to kAudio. B stays manual.\n");

	brain.outputs.set_channel_owner(kOutputsChannelA, kOutputsOwnerAudio);

	const bool a_now_audio =
		brain.outputs.get_channel_owner(kOutputsChannelA) == kOutputsOwnerAudio;
	const bool b_still_manual =
		brain.outputs.get_channel_owner(kOutputsChannelB) == kOutputsOwnerManual;
	print_pass_fail("owner A == kAudio after flip", a_now_audio);
	print_pass_fail("owner B unchanged == kManual", b_still_manual);

	// Manual write to A must now refuse.
	const bool a_refused = !brain.outputs.set_voltage_millivolts(kOutputsChannelA, 1000);
	print_pass_fail("set_voltage_millivolts(A) returns false when audio-owned", a_refused);

	// Manual write to B must still succeed.
	const bool b_still_writable =
		brain.outputs.set_voltage_millivolts(kOutputsChannelB, 5000);
	print_pass_fail("set_voltage_millivolts(B, 5000mV) still returns true", b_still_writable);

	// Audio sample push to A must succeed.
	const bool a_audio_pushed =
		brain::internal::OutputEngine::instance().write_audio_sample(kOutputsChannelA, 0x800);
	print_pass_fail("write_audio_sample(A, 0x800) returns true", a_audio_pushed);

	// Audio sample push to B must refuse (B still manual).
	const bool b_audio_refused =
		!brain::internal::OutputEngine::instance().write_audio_sample(kOutputsChannelB, 0x800);
	print_pass_fail("write_audio_sample(B) returns false when manual-owned", b_audio_refused);

	printf("  -> Driving A with a slow triangle for %lu seconds.\n",
		static_cast<unsigned long>(kPhaseBDriveDurationUs / 1'000'000ul));
	printf("  -> Scope A: triangle wave; Scope B: steady at 5V.\n\n");

	phase_ = Phase::kPhaseBSetup;
	phase_started_us_ = to_us_since_boot(get_absolute_time());
}

void OutputOwnershipTest::run_phase_b_drive_step() {
	if (triangle_up_) {
		if (triangle_value_ + kTriangleStep >= 0x0FFF) {
			triangle_value_ = 0x0FFF;
			triangle_up_ = false;
		} else {
			triangle_value_ = static_cast<uint16_t>(triangle_value_ + kTriangleStep);
		}
	} else {
		if (triangle_value_ < kTriangleStep) {
			triangle_value_ = 0;
			triangle_up_ = true;
		} else {
			triangle_value_ = static_cast<uint16_t>(triangle_value_ - kTriangleStep);
		}
	}
	brain::internal::OutputEngine::instance().write_audio_sample(
		kOutputsChannelA, triangle_value_);
}

void OutputOwnershipTest::enter_phase_c() {
	printf("[Phase C] Flip A back to kManual. Manual writes resume.\n");

	brain.outputs.set_channel_owner(kOutputsChannelA, kOutputsOwnerManual);

	const bool a_now_manual =
		brain.outputs.get_channel_owner(kOutputsChannelA) == kOutputsOwnerManual;
	print_pass_fail("owner A == kManual after flip back", a_now_manual);

	const bool a_writable_again =
		brain.outputs.set_voltage_millivolts(kOutputsChannelA, 5000);
	print_pass_fail("set_voltage_millivolts(A, 5000mV) returns true after flip back", a_writable_again);

	printf("  -> Scope: A snaps to ~5V and holds steady.\n\n");

	phase_ = Phase::kPhaseC;
	phase_started_us_ = to_us_since_boot(get_absolute_time());
}

void OutputOwnershipTest::enter_idle() {
	const auto snapshot = brain::internal::OutputEngine::instance().get_snapshot();
	printf("[Done] Test complete. Engine counters:\n");
	printf("  total_blocks=%llu total_frames=%llu\n",
		static_cast<unsigned long long>(snapshot.total_blocks),
		static_cast<unsigned long long>(snapshot.total_frames));
	printf("  audio_underrun_a=%lu audio_underrun_b=%lu\n",
		static_cast<unsigned long>(snapshot.audio_underrun_a),
		static_cast<unsigned long>(snapshot.audio_underrun_b));
	printf("  (Underrun A > 0 is expected — main loop pace is below block rate.)\n");

	phase_ = Phase::kIdle;
}

}  // namespace sandbox::apps
