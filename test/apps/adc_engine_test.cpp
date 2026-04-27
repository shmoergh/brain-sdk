#include "adc_engine_test.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"

namespace {

void print_check_result(const char* label, bool pass) {
	printf("[%-4s] %s\n", pass ? "PASS" : "FAIL", label);
}

int16_t passthrough_clamp(int32_t value) {
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return static_cast<int16_t>(value);
}

}  // namespace

namespace sandbox::apps {

int16_t AdcEngineTest::process_sample(
	int16_t input_sample,
	const AudioProcessorFrame* frame,
	void* user_ctx) {
	EffectState* state = static_cast<EffectState*>(user_ctx);
	if (state == nullptr) {
		return input_sample;
	}

	// Light lowpass driven by pot 1 so we can hear that the audio path is
	// alive and that pot snapshots are being consumed inside the callback.
	uint8_t cutoff = 64;
	if (frame != nullptr && frame->pot_count > 1) {
		cutoff = frame->pot_raw_u8[1];
	}
	const int32_t coeff = 4 + (cutoff >> 1);  // 4..131
	state->lowpass_state += ((static_cast<int32_t>(input_sample) - state->lowpass_state) * coeff) >> 8;
	return passthrough_clamp(state->lowpass_state);
}

void AdcEngineTest::init() {
	stdio_init_all();
	sleep_ms(800);

	printf("\n\r========================================\n");
	printf("AdcEngine 2.1 Test\n");
	printf("========================================\n\n");

	// 1. Concurrent init checks (replaces the old "must not mix" guardrail).
	const bool concurrent_init_ok = run_concurrent_init_checks();

	// brain_ now has Pots + Inputs + PotMultiFunction + AudioProcessor all up.
	if (!concurrent_init_ok) {
		printf("\n[ABORT] Concurrent init checks failed; cannot continue.\n");
		initialized_ = false;
		return;
	}

	// 2. Legacy shims must compile and be safe to call. No effect on behavior.
	run_legacy_shim_checks();

	// 3. Probe the cached read paths for blocking behavior.
	run_latency_probes();

	// 4. Verify pot/CV values change without us ever calling scan/update.
	run_freshness_check();

	// Snapshot overruns now so the live loop can report drift only.
	baseline_overruns_ = brain_.audio_processor.get_stats().overrun_count;

	printf("\nAll automated checks complete.\n");
	printf("Live loop now running. Turn knobs and apply CV to verify:\n");
	printf("  - Pot/audio_processor pot values match and respond.\n");
	printf("  - Input voltages update.\n");
	printf("  - Overrun delta stays at 0.\n\n");

	initialized_ = true;
}

void AdcEngineTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) < 250000) {
		sleep_ms(1);
		return;
	}
	last_print_us_ = now_us;

	const AudioProcessorStats stats = brain_.audio_processor.get_stats();
	const uint64_t overrun_delta = stats.overrun_count - baseline_overruns_;

	const uint16_t pots_p0 = brain_.pots.get(0);
	const uint16_t pots_p1 = brain_.pots.get(1);
	const uint16_t pots_p2 = brain_.pots.get(2);
	const uint16_t ap_p0 = brain_.audio_processor.get_pot_raw_u8(0);
	const uint16_t ap_p1 = brain_.audio_processor.get_pot_raw_u8(1);
	const uint16_t ap_p2 = brain_.audio_processor.get_pot_raw_u8(2);

	const int32_t cv_a_mv = brain_.inputs.get_voltage_millivolts_channel_a();
	const int32_t cv_b_mv = brain_.inputs.get_voltage_millivolts_channel_b();

	printf(
		"\rTicks=%llu OvrDelta=%llu Pots=[%3u %3u %3u] APPots=[%3u %3u %3u] CV=[%+5ld %+5ld]mV     ",
		static_cast<unsigned long long>(stats.tick_count),
		static_cast<unsigned long long>(overrun_delta),
		static_cast<unsigned>(pots_p0),
		static_cast<unsigned>(pots_p1),
		static_cast<unsigned>(pots_p2),
		static_cast<unsigned>(ap_p0),
		static_cast<unsigned>(ap_p1),
		static_cast<unsigned>(ap_p2),
		static_cast<long>(cv_a_mv),
		static_cast<long>(cv_b_mv));
	fflush(stdout);

	sleep_ms(1);
}

bool AdcEngineTest::run_concurrent_init_checks() {
	printf("Concurrent-init checks (must all PASS - AdcEngine allows mixing):\n");

	// Order A: pots -> inputs -> pot_multi -> audio.
	const bool a_pots = brain_init_succeeded(brain_.init_pots(create_default_pots_config(3, 8)));
	const bool a_inputs = brain_init_succeeded(brain_.init_inputs());
	const bool a_pot_multi = brain_init_succeeded(brain_.init_pot_multi());

	AudioProcessorConfig audio_cfg{};
	audio_cfg.sample_period_us = 23;	 // ~43.5 kHz
	const bool a_audio = brain_init_succeeded(
		brain_.init_audio_processor(audio_cfg, &AdcEngineTest::process_sample, &effect_state_));

	print_check_result("pots init", a_pots);
	print_check_result("inputs init (after pots)", a_inputs);
	print_check_result("pot_multi init (after inputs)", a_pot_multi);
	print_check_result("audio init (after pots/inputs/pot_multi)", a_audio);

	// Order B: fresh Brain instance, audio first, then everything else.
	// Verifies the reverse direction of what the old guardrails forbade.
	Brain order_b{};
	const bool b_audio = brain_init_succeeded(
		order_b.init_audio_processor(audio_cfg, &AdcEngineTest::process_sample, &effect_state_));
	const bool b_pots = brain_init_succeeded(order_b.init_pots(create_default_pots_config(3, 8)));
	const bool b_inputs = brain_init_succeeded(order_b.init_inputs());
	const bool b_pot_multi = brain_init_succeeded(order_b.init_pot_multi());

	print_check_result("audio init (fresh brain)", b_audio);
	print_check_result("pots init (after audio)", b_pots);
	print_check_result("inputs init (after audio)", b_inputs);
	print_check_result("pot_multi init (after audio)", b_pot_multi);

	return a_pots && a_inputs && a_pot_multi && a_audio &&
		   b_audio && b_pots && b_inputs && b_pot_multi;
}

void AdcEngineTest::run_legacy_shim_checks() {
	printf("\nLegacy shim checks (must compile and not crash):\n");

	// Pots legacy shims.
	brain_.pots.set_simple(true);
	brain_.pots.set_simple(false);
	brain_.pots.set_optimized_sampling_enabled(false);
	const bool pots_optim_true = brain_.pots.is_optimized_sampling_enabled();
	brain_.pots.set_optimized_sampling_enabled(true);
	brain_.pots.set_settling_delay_us(500);
	brain_.pots.scan();	 // expected no-op
	print_check_result("pots.is_optimized_sampling_enabled() == true", pots_optim_true);

	// Inputs legacy shims.
	brain_.inputs.set_audio_cv_dma_enabled(false);
	brain_.inputs.set_audio_cv_dma_enabled(true);
	const bool inputs_dma_enabled = brain_.inputs.is_audio_cv_dma_enabled();
	const bool inputs_dma_active = brain_.inputs.is_audio_cv_dma_active();
	brain_.inputs.update_audio_cv();  // expected no-op
	print_check_result("inputs.is_audio_cv_dma_enabled() == true", inputs_dma_enabled);
	print_check_result("inputs.is_audio_cv_dma_active() == true", inputs_dma_active);

	// Brain-level legacy shims.
	brain_.enable_adc_optimization(false);
	brain_.enable_adc_optimization(true);
	brain_.set_audio_cv_dma_enabled(false);
	brain_.set_audio_cv_dma_enabled(true);
	brain_.set_shared_pot_sampling_enabled(false);
	brain_.set_shared_pot_sampling_enabled(true);
	print_check_result("brain legacy switches callable without crash", true);
}

void AdcEngineTest::run_latency_probes() {
	printf("\nLatency probes (cached reads should be << 10us):\n");

	constexpr uint32_t kIters = 1000;

	auto probe = [&](const char* label, auto&& fn) {
		const absolute_time_t start = get_absolute_time();
		uint32_t max_us = 0;
		for (uint32_t i = 0; i < kIters; ++i) {
			const absolute_time_t call_start = get_absolute_time();
			fn();
			const int64_t delta = absolute_time_diff_us(call_start, get_absolute_time());
			if (delta > 0 && static_cast<uint32_t>(delta) > max_us) {
				max_us = static_cast<uint32_t>(delta);
			}
		}
		const int64_t total_us = absolute_time_diff_us(start, get_absolute_time());
		const float avg_us = static_cast<float>(total_us) / static_cast<float>(kIters);
		printf("  %-32s avg=%.2f us  max=%lu us  (%lu iters)\n",
			   label, static_cast<double>(avg_us),
			   static_cast<unsigned long>(max_us),
			   static_cast<unsigned long>(kIters));
	};

	// volatile sinks defeat compiler dead-code elimination of the call.
	volatile uint16_t sink_u16 = 0;
	volatile int32_t sink_i32 = 0;
	(void)sink_u16;
	(void)sink_i32;

	probe("pots.get(0)", [&] { sink_u16 = brain_.pots.get(0); });
	probe("pots.get_raw(0)", [&] { sink_u16 = brain_.pots.get_raw(0); });
	probe("inputs.get_raw(kInputsChannelA)", [&] {
		sink_u16 = brain_.inputs.get_raw(kInputsChannelA);
	});
	probe("inputs.get_voltage_millivolts(A)", [&] {
		sink_i32 = brain_.inputs.get_voltage_millivolts(kInputsChannelA);
	});
	probe("audio_processor.get_pot_raw_u8(0)", [&] {
		sink_u16 = brain_.audio_processor.get_pot_raw_u8(0);
	});
}

void AdcEngineTest::run_freshness_check() {
	printf("\nFreshness check (no scan/update calls; AdcEngine should keep values fresh):\n");
	printf("Sampling for 2s without ever calling pots.scan() or inputs.update_audio_cv()...\n");

	uint16_t pot_min[3] = {0xFFFF, 0xFFFF, 0xFFFF};
	uint16_t pot_max[3] = {0, 0, 0};
	int32_t cv_a_min = INT32_MAX, cv_a_max = INT32_MIN;
	int32_t cv_b_min = INT32_MAX, cv_b_max = INT32_MIN;

	uint32_t samples = 0;
	const absolute_time_t deadline = make_timeout_time_ms(2000);
	while (absolute_time_diff_us(get_absolute_time(), deadline) > 0) {
		for (int p = 0; p < 3; ++p) {
			const uint16_t v = brain_.pots.get(p);
			if (v < pot_min[p]) pot_min[p] = v;
			if (v > pot_max[p]) pot_max[p] = v;
		}
		const int32_t cv_a = brain_.inputs.get_voltage_millivolts_channel_a();
		const int32_t cv_b = brain_.inputs.get_voltage_millivolts_channel_b();
		if (cv_a < cv_a_min) cv_a_min = cv_a;
		if (cv_a > cv_a_max) cv_a_max = cv_a;
		if (cv_b < cv_b_min) cv_b_min = cv_b;
		if (cv_b > cv_b_max) cv_b_max = cv_b;
		++samples;
		sleep_ms(2);
	}

	printf("  Iterations:           %lu\n", static_cast<unsigned long>(samples));
	printf("  Pot 0 range observed: %u..%u\n", pot_min[0], pot_max[0]);
	printf("  Pot 1 range observed: %u..%u\n", pot_min[1], pot_max[1]);
	printf("  Pot 2 range observed: %u..%u\n", pot_min[2], pot_max[2]);
	printf("  CV A range observed:  %+ld..%+ld mV\n",
		   static_cast<long>(cv_a_min), static_cast<long>(cv_a_max));
	printf("  CV B range observed:  %+ld..%+ld mV\n",
		   static_cast<long>(cv_b_min), static_cast<long>(cv_b_max));
	printf("  (Move a knob or apply CV during this window to see ranges expand.)\n");
}

}  // namespace sandbox::apps
