#pragma once

#include <cstdint>
#include <functional>

#include <pico/time.h>

#include "gpio-setup.h"

static constexpr uint8_t kMaxPots = 4;
static constexpr uint8_t kDefaultPotsOutputResolution = 8;

struct PotsConfig {
	bool simple;  // Legacy: ignored.
	uint8_t adc_gpio;
	uint8_t s0_gpio;
	uint8_t s1_gpio;
	uint8_t num_pots;
	uint8_t channel_map[kMaxPots];
	uint8_t output_resolution;
	uint32_t settling_delay_us;	 // Legacy: ignored.
	uint8_t samples_per_read;
	uint16_t change_threshold;
	uint8_t settle_discard_samples;	 // Legacy: ignored — see `settle_us` below.
	// Time the mux is given to settle after a flip before sampling resumes.
	// Must comfortably exceed (mux + S/H + DMA pipeline) settle time. Defaults
	// to 1 ms which is huge for analog settling but trivial for human pots.
	uint32_t settle_us;
};

PotsConfig create_default_config(
	uint8_t num_pots = 3,
	uint8_t output_resolution = kDefaultPotsOutputResolution);

/**
 * @brief Reader for multiplexed analog potentiometers (74HC4051 + one ADC pin).
 *
 * `Pots` runs an internal periodic timer that drives a tiny state machine:
 *
 *     [SETTLING]  → wait `settle_us` after a mux flip (cache catches up)
 *           ↓
 *     [SAMPLING]  → read AdcEngine::get_latest() once per tick, accumulate
 *           ↓ (after `samples_per_read` reads)
 *     commit average, advance pot, flip mux, back to SETTLING
 *
 * No callbacks. The mux flip happens at a known time and the next read is
 * scheduled `settle_us` later, so cross-bleed cannot happen regardless of
 * how the ADC is being shared with `AudioProcessor` or `Inputs`.
 */
class Pots {
public:
	Pots();
	~Pots();

	void init(const PotsConfig& cfg);
	void reconfigure(const PotsConfig& cfg);

	void set_output_resolution(uint8_t resolution);
	void set_samples_per_read(uint8_t samples);
	void set_change_threshold(uint16_t threshold);

	uint16_t get(uint8_t index);
	uint16_t get_buffered(uint8_t index) const;
	// Same as get_buffered — kept for source compat with earlier SDK.
	uint16_t get_single(uint8_t index);
	uint16_t get_raw(uint8_t index);

	uint8_t get_output_resolution() const;
	uint16_t get_output_max() const;
	uint8_t get_num_pots() const;

	void set_on_change(std::function<void(uint8_t, uint16_t)> cb);

	// ---- Legacy no-op shims (kept so old apps keep compiling) ----
	void set_simple(bool /*simple*/) {}
	void set_optimized_sampling_enabled(bool /*enabled*/) {}
	bool is_optimized_sampling_enabled() const { return true; }
	void set_settling_delay_us(uint32_t /*delay*/) {}
	void scan() {}

private:
	enum class Phase : uint8_t { kSettling, kSampling };

	static bool timer_callback(repeating_timer_t* timer);
	void on_tick();

	void set_mux_channel(uint8_t ch);
	void start_settling_for(uint8_t pot_index);
	uint16_t map_to_output(uint16_t raw) const;

	PotsConfig config_{};
	uint16_t buffered_raw_[kMaxPots] = {0, 0, 0, 0};
	uint16_t buffered_values_[kMaxPots] = {0, 0, 0, 0};
	uint16_t last_values_[kMaxPots] = {0, 0, 0, 0};

	Phase phase_ = Phase::kSettling;
	uint8_t active_pot_index_ = 0;
	uint8_t samples_collected_ = 0;
	uint32_t accumulator_ = 0;
	absolute_time_t settle_until_{};

	std::function<void(uint8_t, uint16_t)> on_change_;

	repeating_timer_t timer_{};
	bool timer_running_ = false;
	bool initialized_ = false;
};
