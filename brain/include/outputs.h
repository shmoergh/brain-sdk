#pragma once

#include <hardware/spi.h>

#include <cstdint>

#include "gpio-setup.h"
#include "storage.h"

enum class AudioCvOutChannel { kChannelA = 0, kChannelB = 1 };

enum class AudioCvOutRange {
	kRange0To10V = 0,
	kRangeMinus5To5V = 1
};

// Convenience aliases for less verbose call sites.
constexpr AudioCvOutChannel kOutputsChannelA = AudioCvOutChannel::kChannelA;
constexpr AudioCvOutChannel kOutputsChannelB = AudioCvOutChannel::kChannelB;

constexpr AudioCvOutRange kOutputsRange0To10V = AudioCvOutRange::kRange0To10V;
constexpr AudioCvOutRange kOutputsRangeMinus5To5V = AudioCvOutRange::kRangeMinus5To5V;

class Outputs {
public:
	static constexpr uint8_t kMCP4822_CHANNEL_A = 0;
	static constexpr uint8_t kMCP4822_CHANNEL_B = 1;
	static constexpr uint8_t kMCP4822_GAIN = 0;
	static constexpr uint8_t kMCP4822_ACTIVE = 1;

	static constexpr int32_t kUnipolarMinMillivolts = 0;
	static constexpr int32_t kUnipolarMaxMillivolts = 10000;
	static constexpr int32_t kBipolarMinMillivolts = -5000;
	static constexpr int32_t kBipolarMaxMillivolts = 5000;
	static constexpr int32_t kBipolarOffsetMillivolts = 5000;
	static constexpr int32_t kMaxOutputMillivolts = 10000;
	static constexpr uint16_t kMaxDacValue = 4095;
	static constexpr uint32_t kSpiFrequency = 1000000;

	explicit Outputs(uint pulse_out_gpio = GPIO_BRAIN_PULSE_OUTPUT);

	bool init_audio_cv(spi_inst_t* spi_instance = spi0, uint cs_pin = GPIO_BRAIN_AUDIO_CV_OUT_CS,
		uint sck_pin = GPIO_BRAIN_AUDIO_CV_OUT_SCK, uint tx_pin = GPIO_BRAIN_AUDIO_CV_OUT_TX,
		uint coupling_pin_a = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A,
		uint coupling_pin_b = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B,
		AudioCvOutRange range_a = AudioCvOutRange::kRange0To10V,
		AudioCvOutRange range_b = AudioCvOutRange::kRange0To10V);
	void set_dependencies(Storage* storage);

	void init_pulse();
	bool init();

	bool set_voltage_millivolts(AudioCvOutChannel channel, int32_t millivolts);
	bool set_voltage_calibrated_millivolts(AudioCvOutChannel channel, int32_t target_millivolts);
	bool set_calibration(const CvCalibrationV1& cal);
	void clear_calibration();
	bool load_calibration_from_flash();
	bool has_calibration() const;
	bool is_audio_cv_initialized() const;
	bool is_pulse_initialized() const;
	bool is_initialized() const;
	uint16_t get_last_dac_value(AudioCvOutChannel channel) const;
	int32_t get_last_set_millivolts(AudioCvOutChannel channel) const;
	bool set_output_range(AudioCvOutChannel channel, AudioCvOutRange range);
	AudioCvOutRange get_output_range(AudioCvOutChannel channel) const;

	void pulse_set(bool on);
	bool pulse_get() const;

private:
	void write_dac_channel(AudioCvOutChannel channel, uint16_t dac_value);
	bool to_dac_input_millivolts(
		AudioCvOutChannel channel,
		int32_t requested_millivolts,
		bool clamp_to_range,
		int32_t* dac_input_millivolts) const;
	uint16_t millivolts_to_dac(int32_t dac_input_millivolts) const;
	int16_t interpolated_offset_lsb(AudioCvOutChannel channel, int32_t dac_input_millivolts) const;
	const char* range_to_string(AudioCvOutRange range) const;

	uint pulse_out_gpio_ = 0;
	bool pulse_initialized_ = false;
	bool pulse_state_ = false;

	uint cs_pin_ = 0;
	uint sck_pin_ = 0;
	uint tx_pin_ = 0;
	uint coupling_pin_a_ = 0;
	uint coupling_pin_b_ = 0;
	spi_inst_t* spi_instance_ = nullptr;
	bool audio_cv_initialized_ = false;

	bool calibration_loaded_ = false;
	int16_t calibration_a_offset_lsb_[10] = {0};
	int16_t calibration_b_offset_lsb_[10] = {0};
	Storage* storage_ = nullptr;
	Storage default_storage_{};

	AudioCvOutRange range_a_ = AudioCvOutRange::kRange0To10V;
	AudioCvOutRange range_b_ = AudioCvOutRange::kRange0To10V;
	int32_t last_set_millivolts_a_ = 0;
	int32_t last_set_millivolts_b_ = 0;
	uint16_t last_dac_value_a_ = 0;
	uint16_t last_dac_value_b_ = 0;
};

using AudioCvOut = Outputs;
