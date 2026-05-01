#pragma once

#include <cstdint>
#include <functional>

#include "gpio-setup.h"

static constexpr uint8_t kMaxPots = 4;
static constexpr uint8_t kDefaultPotsOutputResolution = 8;

struct PotsConfig {
	bool simple;  // Legacy: ignored under DMA-driven AdcEngine path.
	uint8_t adc_gpio;
	uint8_t s0_gpio;
	uint8_t s1_gpio;
	uint8_t num_pots;
	uint8_t channel_map[kMaxPots];
	uint8_t output_resolution;
	uint32_t settling_delay_us;	 // Legacy: ignored. Settle is now `samples_per_read`-driven.
	uint8_t samples_per_read;
	uint16_t change_threshold;
	uint8_t settle_discard_samples;	 // Samples to drop after each mux switch (default 2).
};

PotsConfig create_default_config(
	uint8_t num_pots = 3,
	uint8_t output_resolution = kDefaultPotsOutputResolution);

class Pots {
public:
	/**
	 * @brief Creates a `Pots` reader for multiplexed analog potentiometers.
	 */
	Pots();

	/**
	 * @brief Releases the underlying `AdcEngine` subscription.
	 */
	~Pots();

	/**
	 * @brief Initializes mux GPIOs and subscribes to the pot mux ADC channel via `AdcEngine`.
	 * @param cfg Pot acquisition config:
	 * - `adc_gpio`: ADC input pin connected to pot mux output.
	 * - `s0_gpio`, `s1_gpio`: mux select lines.
	 * - `num_pots`: number of pots to read (clamped to `kMaxPots`).
	 * - `channel_map`: maps logical pot index to mux channel.
	 * - `output_resolution`: bit depth for returned values (mapped from 12-bit ADC).
	 * - `samples_per_read`: averaging depth.
	 * - `settle_discard_samples`: samples discarded after each mux switch (default 2).
	 * - `change_threshold`: minimum delta that triggers `on_change` callback.
	 * - `simple`, `settling_delay_us`: legacy fields, ignored.
	 */
	void init(const PotsConfig& cfg);

	/**
	 * @brief Applies a new configuration without creating a new `Pots` object.
	 */
	void reconfigure(const PotsConfig& cfg);

	/**
	 * @brief Legacy API: ignored. The DMA-driven state machine has a single read path.
	 */
	void set_simple(bool simple);

	/**
	 * @brief Legacy API: ignored. Optimized sampling is the only path under `AdcEngine`.
	 */
	void set_optimized_sampling_enabled(bool enabled);

	/**
	 * @brief Legacy API: always returns `true` under the unified `AdcEngine`.
	 */
	bool is_optimized_sampling_enabled() const;

	void set_output_resolution(uint8_t resolution);

	/**
	 * @brief Legacy API: ignored. Settle time is governed by `settle_discard_samples`.
	 */
	void set_settling_delay_us(uint32_t delay);

	void set_samples_per_read(uint8_t samples);
	void set_change_threshold(uint16_t threshold);

	/**
	 * @brief Legacy API: no-op. Pot values are kept fresh by `AdcEngine` callbacks.
	 *
	 * Retained so that existing callers (e.g. `Brain::update_pots()`) keep compiling.
	 */
	void scan();

	/**
	 * @brief Returns the latest mapped pot value for the selected index.
	 * @param index Logical pot index.
	 * @return Latest cached mapped value, or `0` for invalid index.
	 */
	uint16_t get_single(uint8_t index);

	/**
	 * @brief Returns the latest mapped pot value for the selected index.
	 */
	uint16_t get(uint8_t index);

	/**
	 * @brief Returns the latest cached mapped value without touching hardware.
	 */
	uint16_t get_buffered(uint8_t index) const;

	/**
	 * @brief Returns the latest averaged raw 12-bit ADC value for the selected pot.
	 */
	uint16_t get_raw(uint8_t index);

	uint8_t get_output_resolution() const;
	uint16_t get_output_max() const;

	void set_on_change(std::function<void(uint8_t, uint16_t)> cb);

	uint8_t get_num_pots() const;

private:
	void set_mux_channel(uint8_t ch);
	void on_adc_sample(uint16_t raw);
	void release_subscription();
	uint16_t map_to_output(uint16_t raw) const;

	PotsConfig config_;
	uint16_t buffered_raw_[kMaxPots];
	uint16_t buffered_values_[kMaxPots];
	uint16_t last_values_[kMaxPots];

	uint32_t pot_accumulator_ = 0;
	uint8_t pot_samples_collected_ = 0;
	// Held in a 16-bit field because the post-flip discard count includes the
	// full ADC + DMA pipeline depth, which can briefly exceed 255 if drains
	// are infrequent (e.g. only `Pots` registered with no audio inline drain).
	uint16_t pot_discard_remaining_ = 0;
	uint8_t active_pot_index_ = 0;

	std::function<void(uint8_t, uint16_t)> on_change_;
	uint32_t adc_token_ = 0;
};
