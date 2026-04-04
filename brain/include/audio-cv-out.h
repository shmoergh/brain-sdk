#pragma once

#include <hardware/spi.h>

#include <cstdint>

#include "gpio-setup.h"

struct CvCalibrationV1;

enum class AudioCvOutChannel { kChannelA = 0, kChannelB = 1 };

enum class AudioCvOutCoupling {
	kDcCoupled = 0,
	kAcCoupled = 1
};

class AudioCvOut {
public:
	static constexpr uint8_t kMCP4822_CHANNEL_A = 0;
	static constexpr uint8_t kMCP4822_CHANNEL_B = 1;
	static constexpr uint8_t kMCP4822_GAIN = 0;
	static constexpr uint8_t kMCP4822_ACTIVE = 1;

	static constexpr float kMaxVoltage = 10.0f;
	static constexpr uint16_t kMaxDacValue = 4095;
	static constexpr uint32_t kSpiFrequency = 1000000;

	bool init(spi_inst_t* spi_instance = spi0, uint cs_pin = GPIO_BRAIN_AUDIO_CV_OUT_CS,
		uint sck_pin = GPIO_BRAIN_AUDIO_CV_OUT_SCK, uint tx_pin = GPIO_BRAIN_AUDIO_CV_OUT_TX,
		uint coupling_pin_a = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A,
		uint coupling_pin_b = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B);

	bool set_voltage(AudioCvOutChannel channel, float voltage);
	bool set_voltage_calibrated(AudioCvOutChannel channel, float target_voltage);
	bool set_calibration(const CvCalibrationV1& cal);
	void clear_calibration();
	bool load_calibration_from_flash();
	bool has_calibration() const;
	uint16_t get_last_dac_value(AudioCvOutChannel channel) const;
	bool set_coupling(AudioCvOutChannel channel, AudioCvOutCoupling coupling);

private:
	void write_dac_channel(AudioCvOutChannel channel, uint16_t dac_value);
	uint16_t voltage_to_dac(float voltage);
	float clamp_voltage(float voltage) const;
	int16_t interpolated_offset_lsb(AudioCvOutChannel channel, float clamped_voltage) const;

	uint cs_pin_ = 0;
	uint sck_pin_ = 0;
	uint tx_pin_ = 0;
	uint coupling_pin_a_ = 0;
	uint coupling_pin_b_ = 0;
	spi_inst_t* spi_instance_ = nullptr;

	bool calibration_loaded_ = false;
	int16_t calibration_a_offset_lsb_[10] = {0};
	int16_t calibration_b_offset_lsb_[10] = {0};

	uint16_t last_dac_value_a_ = 0;
	uint16_t last_dac_value_b_ = 0;
};

namespace brain {
namespace io {
using ::AudioCvOutChannel;
using ::AudioCvOutCoupling;
using ::AudioCvOut;
}  // namespace io
}  // namespace brain
