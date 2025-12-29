// Multiplexed potentiometer reader for Hog Moduleur Brain
// Requires: 74HC4051 multiplexer, ADC GPIO, and S0/S1 selector GPIOs.
#pragma once

#include <cstdint>
#include <functional>

#include "brain-common/brain-gpio-setup.h"

namespace brain::ui {

static constexpr uint8_t kMaxPots = 4;	// 4-channel multiplexer

/**
 * @brief Configuration structure for PotMultiplexer
 *
 * Defines hardware connections and sampling parameters for the multiplexed
 * potentiometer reader using a 74HC4051 analog multiplexer.
 */
struct PotsConfig {
	bool simple;
	uint8_t adc_gpio;  ///< ADC GPIO pin number (typically 26-29)
	uint8_t s0_gpio;  ///< Multiplexer S0 select line GPIO
	uint8_t s1_gpio;  ///< Multiplexer S1 select line GPIO
	uint8_t num_pots;  ///< Number of active potentiometers (1-4)
	uint8_t channel_map[kMaxPots];	///< Logical-to-physical channel mapping
	uint8_t output_resolution;	///< Output resolution in bits (e.g., 7 for 0-127)
	uint32_t settling_delay_us;	 ///< Settling time after mux channel change (µs)
	uint8_t samples_per_read;  ///< Number of samples to average per reading
	uint16_t change_threshold;	///< Minimum change to trigger callback
};

/**
 * @brief Create default configuration for Brain module pot multiplexer
 *
 * Returns a PotMultiplexerConfig with default GPIO assignments and
 * reasonable timing parameters for the Brain module hardware.
 *
 * @param num_pots Number of active potentiometers (1-3, defaults to 3)
 * @param output_resolution Output resolution in bits (defaults to 7 for 0-127 range)
 * @return Default configuration structure
 */
PotsConfig create_default_config(uint8_t num_pots = 3, uint8_t output_resolution = 7);

/**
 * @brief Multiplexed potentiometer reader for Brain module
 *
 * Provides reading of up to 4 potentiometers through a 74HC4051 analog
 * multiplexer connected to the Pico's ADC. Handles channel switching,
 * settling delays, and change detection with configurable thresholds.
 *
 * Typical settling time: ~200µs per channel for stable readings.
 */
class Pots {
	public:
	/**
	 * @brief Construct a new PotMultiplexer object
	 *
	 * Initializes internal state but does not configure hardware.
	 * Call init() to set up GPIO and ADC.
	 */
	Pots();

	/**
	 * @brief Initialize hardware and configure multiplexer
	 *
	 * Sets up ADC, multiplexer select GPIOs, and internal state.
	 * Must be called before any other operations.
	 *
	 * @param cfg Configuration structure with hardware and timing parameters
	 */
	void init(const PotsConfig& cfg);

	/**
	 * Config setters
	 */
	void set_simple(bool simple);
	void set_output_resolution(uint8_t resolution);
	void set_settling_delay_us(uint32_t delay);
	void set_samples_per_read(uint8_t samples);
	void set_change_threshold (uint16_t threshold);

	/**
	 * @brief Scan all configured potentiometers for changes
	 *
	 * Reads all active channels and triggers callbacks for values that
	 * have changed beyond the configured threshold. Call regularly in
	 * main loop for responsive UI updates.
	 */
	void scan();

	/**
	 * @brief Get scaled potentiometer value
	 *
	 * Returns the current value scaled to the configured output resolution.
	 * For example, with 7-bit resolution, returns 0-127 regardless of
	 * the 12-bit ADC input range.
	 *
	 * @param index Logical potentiometer index (0 to num_pots-1)
	 * @return Scaled value, or 0 if index is invalid
	 */
	uint16_t get(uint8_t index);

	/**
	 * @brief Get raw 12-bit ADC value
	 *
	 * Returns the unscaled ADC reading (0-4095) for debugging or
	 * applications requiring full resolution.
	 *
	 * @param index Logical potentiometer index (0 to num_pots-1)
	 * @return Raw ADC value (0-4095), or 0 if index is invalid
	 */
	uint16_t get_raw(uint8_t index);

	/**
	 * @brief Set callback for potentiometer value changes
	 *
	 * The callback is invoked during scan() when any potentiometer
	 * value changes by more than the configured threshold.
	 *
	 * @param cb Callback function: void(uint8_t pot_index, uint16_t new_value)
	 */
	void set_on_change(std::function<void(uint8_t, uint16_t)> cb);

	private:
	/**
	 * @brief Set multiplexer channel selection
	 *
	 * @param ch Physical channel number (0-3)
	 */
	void set_mux_channel(uint8_t ch);

	/**
	 * @brief Read ADC value for specific channel with proper settling
	 *
	 * @param ch Physical channel number to read
	 * @return Averaged ADC reading
	 */
	uint16_t read_channel_once(uint8_t ch);

	PotsConfig config_;  ///< Hardware configuration
	uint16_t last_values_[kMaxPots];  ///< Last known values for change detection
	std::function<void(uint8_t, uint16_t)> on_change_;	///< Change callback function
};

}  // namespace brain::ui
