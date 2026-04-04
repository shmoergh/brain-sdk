#pragma once

#include <hardware/adc.h>

#include <cstdint>

#include "common.h"

enum AudioCvInChannel { kChannelA = 0, kChannelB = 1 };

class AudioCvIn {
public:
	bool init();
	void update();
	uint16_t get_raw(int channel) const;
	uint16_t get_raw_channel_a() const;
	uint16_t get_raw_channel_b() const;
	float get_voltage(int channel) const;
	float get_voltage_channel_a() const;
	float get_voltage_channel_b() const;

private:
	float adc_to_voltage(uint16_t adc_value) const;
	void calculate_conversion_parameters();

	uint16_t channel_raw_[2] = {0, 0};
	float voltage_scale_ = 1.0f;
	float voltage_offset_ = 0.0f;
};

namespace brain {
namespace io {
using ::AudioCvInChannel;
using ::AudioCvIn;
}  // namespace io
}  // namespace brain
