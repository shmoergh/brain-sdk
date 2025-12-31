// Audio/CV input via RP2040 ADC with configurable calibration
// Dependencies: ADC, GPIO. Hardware: Two analog channels on GPIO 27/28
// Reads ±5V signals that have been level-shifted to ~240mV-3V range
// Pin ownership: GPIO 27 (ADC1), GPIO 28 (ADC2)
// Author: Brain SDK
#pragma once

#include <hardware/adc.h>

#include <cstdint>

#include "brain-common/brain-common.h"

namespace brain::io {

enum AudioCvInChannel { kChannelA = 0, kChannelB = 1 };

/**
 * Audio/CV input controller for two-channel analog input via RP2040 ADC
 *
 * Handles reading analog signals that have been level-shifted from ±5V range
 * to the RP2040 ADC input range. Provides both raw ADC values and converted
 * voltage values using configurable calibration constants.
 */
class AudioCvIn {
	public:
	/**
	 * Initialize ADC hardware and configure input channels
	 * @return true if initialization successful, false on error
	 */
	bool init();

	/**
	 * Update ADC readings (call in main loop for continuous operation)
	 * Refreshes internal readings for both channels
	 */
	void update();

	/**
	 * Get raw ADC value for specified channel
	 * @param channel Channel number (use BRAIN_AUDIO_CV_IN_CHANNEL_A/B constants)
	 * @return Raw 12-bit ADC value (0-4095)
	 */
	uint16_t get_raw(int channel) const;

	/**
	 * Get raw ADC value for channel A
	 * @return Raw 12-bit ADC value (0-4095)
	 */
	uint16_t get_raw_channel_a() const;

	/**
	 * Get raw ADC value for channel B
	 * @return Raw 12-bit ADC value (0-4095)
	 */
	uint16_t get_raw_channel_b() const;

	/**
	 * Get converted voltage for specified channel
	 * @param channel Channel number (use BRAIN_AUDIO_CV_IN_CHANNEL_A/B constants)
	 * @return Original signal voltage (-5.0V to +5.0V range)
	 */
	float get_voltage(int channel) const;

	/**
	 * Get converted voltage for channel A
	 * @return Original signal voltage (-5.0V to +5.0V range)
	 */
	float get_voltage_channel_a() const;

	/**
	 * Get converted voltage for channel B
	 * @return Original signal voltage (-5.0V to +5.0V range)
	 */
	float get_voltage_channel_b() const;

	private:
	/** Convert ADC reading to original signal voltage using calibration */
	float adc_to_voltage(uint16_t adc_value) const;

	/** Calculate conversion parameters from calibration constants */
	void calculate_conversion_parameters();

	// Current ADC readings for both channels
	uint16_t channel_raw_[2] = {0, 0};

	// Conversion parameters calculated from calibration constants
	float voltage_scale_ = 1.0f;
	float voltage_offset_ = 0.0f;
};

}  // namespace brain::io