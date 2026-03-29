// Audio/CV output via MCP4822 DAC with DC/AC coupling control
// Dependencies: SPI, GPIO. Hardware: MCP4822 dual DAC, CD4053 analog switch
// Controls voltage output 0-10V on channels A/B with switchable DC/AC coupling
// Pin ownership: SPI SCK/TX, SPI CS, two GPIO for coupling control
// Author: Brain SDK
#pragma once

#include <hardware/spi.h>

#include <cstdint>

#include "brain-common/brain-gpio-setup.h"

namespace brain::storage {
struct CvCalibrationV1;
}

namespace brain::io {

/** DAC output channel selection */
enum class AudioCvOutChannel { kChannelA = 0, kChannelB = 1 };

/** Coupling mode for output stage */
enum class AudioCvOutCoupling {
	kDcCoupled = 0,	 // Direct coupling - full DC range
	kAcCoupled = 1	// AC coupling - blocks DC component
};

/** Audio/CV output controller for MCP4822 DAC with coupling switches */
class AudioCvOut {
	public:

		// MCP4822 command bits
		static constexpr uint8_t kMCP4822_CHANNEL_A = 0;  // A/B = 0
		static constexpr uint8_t kMCP4822_CHANNEL_B = 1;  // A/B = 1
		static constexpr uint8_t kMCP4822_GAIN = 0;
		static constexpr uint8_t kMCP4822_ACTIVE = 1;

		// Voltage conversion constants
		static constexpr float kMaxVoltage = 10.0f;
		static constexpr uint16_t kMaxDacValue = 4095;
		static constexpr uint32_t kSpiFrequency = 1000000;	// 1 MHz

		/**
		 * Initialize SPI interface and GPIO pins for DAC and coupling control
		 * @param spi_instance SPI peripheral instance (default: spi0)
		 * @param cs_pin Chip select GPIO pin for MCP4822 (default: GPIO 5)
		 * @param sck_pin SPI clock (SCK) GPIO pin (default: GPIO 2)
		 * @param tx_pin SPI TX/MOSI GPIO pin (default: GPIO 3)
		 * @param coupling_pin_a CD4053 control pin for channel A coupling (default: GPIO 6)
		 * @param coupling_pin_b CD4053 control pin for channel B coupling (default: GPIO 7)
		 * @return true if initialization successful, false on error
		 */
		bool init(spi_inst_t* spi_instance = spi0, uint cs_pin = GPIO_BRAIN_AUDIO_CV_OUT_CS,
			uint sck_pin = GPIO_BRAIN_AUDIO_CV_OUT_SCK, uint tx_pin = GPIO_BRAIN_AUDIO_CV_OUT_TX,
			uint coupling_pin_a = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A,
			uint coupling_pin_b = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B);

		/**
		 * Set output voltage on specified channel
		 * @param channel Target output channel (A or B)
		 * @param voltage Output voltage in range 0.0V to 10.0V
		 * @return true if voltage set successfully, false on error
		 */
		bool set_voltage(AudioCvOutChannel channel, float voltage);

		/**
		 * Set output voltage with optional loaded calibration offsets.
		 * Input voltage is clamped to 0.0V..10.0V before conversion.
		 */
		bool set_voltage_calibrated(AudioCvOutChannel channel, float target_voltage);

		/** Load an in-memory calibration table used by set_voltage_calibrated(). */
		bool set_calibration(const brain::storage::CvCalibrationV1& cal);

		/** Clear in-memory calibration table; calibrated writes become raw writes. */
		void clear_calibration();

		/** Load calibration from reserved flash through brain::storage. */
		bool load_calibration_from_flash();

		/** True when an in-memory calibration table is active. */
		bool has_calibration() const;

		/** Last 12-bit DAC code written for each channel (diagnostics/testing). */
		uint16_t get_last_dac_value(AudioCvOutChannel channel) const;

		/**
		 * Configure DC/AC coupling for specified channel
		 * @param channel Target output channel (A or B)
		 * @param coupling Desired coupling mode
		 * @return true if coupling set successfully, false on error
		 */
		bool set_coupling(AudioCvOutChannel channel, AudioCvOutCoupling coupling);

	private:
		/** Send 16-bit command to MCP4822 via SPI */
		void write_dac_channel(AudioCvOutChannel channel, uint16_t dac_value);

		/** Convert voltage (0-10V) to 12-bit DAC value (0-4095) */
		uint16_t voltage_to_dac(float voltage);

		/** Clamp arbitrary voltage into supported 0V..10V output range. */
		float clamp_voltage(float voltage) const;

		/** Interpolate calibration offset table for clamped target voltage. */
		int16_t interpolated_offset_lsb(AudioCvOutChannel channel, float clamped_voltage) const;

		// Hardware configuration
		uint cs_pin_ = 0;
		uint sck_pin_ = 0;
		uint tx_pin_ = 0;
		uint coupling_pin_a_ = 0;
		uint coupling_pin_b_ = 0;
		spi_inst_t* spi_instance_ = nullptr;

		// In-memory calibration state (A/B offsets for 1V..10V)
		bool calibration_loaded_ = false;
		int16_t calibration_a_offset_lsb_[10] = {0};
		int16_t calibration_b_offset_lsb_[10] = {0};

		// Last written DAC codes for diagnostics
		uint16_t last_dac_value_a_ = 0;
		uint16_t last_dac_value_b_ = 0;
};

}  // namespace brain::io
