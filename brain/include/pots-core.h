#pragma once

#include <cstdint>
#include <functional>

#include "gpio-setup.h"

static constexpr uint8_t kMaxPots = 4;
static constexpr uint8_t kDefaultPotsOutputResolution = 8;

struct PotsConfig {
	bool simple;
	uint8_t adc_gpio;
	uint8_t s0_gpio;
	uint8_t s1_gpio;
	uint8_t num_pots;
	// Brain hardware always wires pot N to mux channel N. The engine ignores
	// this field; it is preserved only for source compatibility.
	[[deprecated("PotsConfig::channel_map is ignored: pot N is always mux N on Brain hardware")]]
	uint8_t channel_map[kMaxPots];
	uint8_t output_resolution;
	uint32_t settling_delay_us;
	uint8_t samples_per_read;
	uint16_t change_threshold;
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
	 * @brief Initializes pot mux GPIO/ADC and seeds buffered values from hardware.
	 * @param cfg Pot acquisition config:
	 * - `simple`: `true` for fastest single-read behavior, `false` for stabilized sampling.
	 * - `adc_gpio`: ADC input pin connected to pot mux output.
	 * - `s0_gpio`, `s1_gpio`: mux select lines.
	 * - `num_pots`: number of pots to read (clamped to `kMaxPots`).
	 * - `channel_map`: deprecated/ignored; pot N is always mux N on Brain hardware.
	 * - `output_resolution`: bit depth for returned values (mapped from 12-bit ADC).
	 * - `settling_delay_us`: delay after mux switch before reading.
	 * - `samples_per_read`: averaging depth for stabilized mode.
	 * - `change_threshold`: minimum delta that triggers `on_change` callback.
	 */
	void init(const PotsConfig& cfg);

	/**
	 * @brief Applies a new configuration without creating a new `Pots` object.
	 * @param cfg Same semantics as `init(...)`; this call reinitializes hardware and buffers.
	 */
	void reconfigure(const PotsConfig& cfg);

	/**
	 * @brief Switches between fast simple sampling and stabilized sampling strategy.
	 * @param simple `true` uses minimal settle + single-sample fast path.
	 * `false` uses longer settle and averaged reads for better stability.
	 */
	void set_simple(bool simple);

	/**
	 * @brief Enables/disables optimized sampling behavior.
	 * @param enabled `true` allows optimized stabilized path.
	 * `false` forces the fast direct-like read behavior even when `simple` is false.
	 */
	void set_optimized_sampling_enabled(bool enabled);

	/**
	 * @brief Reports optimized sampling flag state.
	 * @return `true` when optimized sampling path is enabled.
	 */
	bool is_optimized_sampling_enabled() const;

	/**
	 * @brief Sets output bit resolution used by `get*()` mapped pot values.
	 * @param resolution Target resolution in bits. Effective max is capped internally (up to 15 bits for output max math).
	 */
	void set_output_resolution(uint8_t resolution);

	/**
	 * @brief Sets mux settling delay after channel switch.
	 * @param delay Settling delay in microseconds before sampling selected channel.
	 */
	void set_settling_delay_us(uint32_t delay);

	/**
	 * @brief Sets averaging depth used in stabilized sampling mode.
	 * @param samples Number of ADC samples to average per channel read (`0` is treated as `1` internally).
	 */
	void set_samples_per_read(uint8_t samples);

	/**
	 * @brief Sets callback deadband for change detection.
	 * @param threshold Minimum absolute value delta required to trigger `on_change` callback.
	 */
	void set_change_threshold(uint16_t threshold);

	/**
	 * @brief Scans all configured pots and refreshes buffered values.
	 *
	 * Also runs change detection and triggers `set_on_change()` callback for pots that moved beyond threshold.
	 */
	void scan();

	/**
	 * @brief Returns a one-shot sampled value for the selected pot.
	 * @param index Logical pot index.
	 * @return Freshly sampled value mapped to configured output resolution, or `0` for invalid index.
	 */
	uint16_t get_single(uint8_t index);

	/**
	 * @brief Returns the standard pot value for the selected index.
	 * @param index Logical pot index.
	 * @return Buffered value. If buffer is stale, this method triggers a scan first.
	 */
	uint16_t get(uint8_t index);

	/**
	 * @brief Returns the most recently buffered scan value for the selected pot.
	 * @param index Logical pot index.
	 * @return Last scanned mapped value without forcing a new read, or `0` for invalid index.
	 */
	uint16_t get_buffered(uint8_t index) const;

	/**
	 * @brief Returns the latest raw ADC reading for the selected input channel.
	 * @param index Logical pot index.
	 * @return Immediate 12-bit ADC sample from mapped mux channel (0..4095), or `0` for invalid index.
	 */
	uint16_t get_raw(uint8_t index);

	/**
	 * @brief Returns current mapped output resolution.
	 * @return Current output resolution in bits.
	 */
	uint8_t get_output_resolution() const;

	/**
	 * @brief Returns maximum representable mapped value for current output resolution.
	 * @return Maximum representable pot value for the current output resolution.
	 */
	uint16_t get_output_max() const;

	/**
	 * @brief Registers callback for pot movement events detected by `scan()`.
	 * @param cb Callback with `(index, value)` where `index` is pot index and `value` is mapped pot value.
	 */
	void set_on_change(std::function<void(uint8_t, uint16_t)> cb);

	/**
	 * @brief Returns number of logical pots currently configured.
	 * @return Number of physical pots configured in this `Pots` instance.
	 */
	uint8_t get_num_pots() const;

private:
	PotsConfig config_;
	uint16_t last_values_[kMaxPots];
	uint16_t buffered_values_[kMaxPots];
	bool buffer_valid_ = false;
	bool optimized_sampling_enabled_ = true;
	std::function<void(uint8_t, uint16_t)> on_change_;
};
