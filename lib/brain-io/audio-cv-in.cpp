#include "brain-io/audio-cv-in.h"

#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include <cstdio>

namespace brain::io {

using namespace brain::constants;

bool AudioCvIn::init() {
	// Initialize ADC hardware
	adc_init();

	// Configure GPIO pins for ADC use
	adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_A);  // GPIO 27 -> ADC1
	adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_B);  // GPIO 28 -> ADC2

	// Calculate conversion parameters from calibration constants
	calculate_conversion_parameters();

	// Take initial readings
	update();

	return true;
}

void AudioCvIn::update() {
	// Read channel A (GPIO 27 = ADC1)
	adc_select_input(1);
	channel_raw_[AudioCvInChannel::kChannelA] = adc_read();

	// Read channel B (GPIO 28 = ADC2)
	adc_select_input(2);
	channel_raw_[AudioCvInChannel::kChannelB] = adc_read();
}

uint16_t AudioCvIn::get_raw(int channel) const {
	if (channel == AudioCvInChannel::kChannelA || channel == AudioCvInChannel::kChannelB) {
		return channel_raw_[channel];
	}
	return 0;
}

uint16_t AudioCvIn::get_raw_channel_a() const {
	return channel_raw_[AudioCvInChannel::kChannelA];
}

uint16_t AudioCvIn::get_raw_channel_b() const {
	return channel_raw_[AudioCvInChannel::kChannelB];
}

float AudioCvIn::get_voltage(int channel) const {
	if (channel == AudioCvInChannel::kChannelA || channel == AudioCvInChannel::kChannelB) {
		return adc_to_voltage(channel_raw_[channel]);
	}
	return 0.0f;
}

float AudioCvIn::get_voltage_channel_a() const {
	return adc_to_voltage(channel_raw_[AudioCvInChannel::kChannelA]);
}

float AudioCvIn::get_voltage_channel_b() const {
	return adc_to_voltage(channel_raw_[AudioCvInChannel::kChannelB]);
}

float AudioCvIn::adc_to_voltage(uint16_t adc_value) const {
	// Convert ADC reading to voltage
	float adc_voltage = (static_cast<float>(adc_value) / kAdcMaxValue) * kAdcVoltageRef;

	// Apply calibration to get original signal voltage
	return (adc_voltage * voltage_scale_) + voltage_offset_;
}

void AudioCvIn::calculate_conversion_parameters() {
	// Calculate linear conversion from measured ADC voltages to original signal voltages
	// Two known points: (kAudioCvInVoltageAtMinus5V, kAudioCvInMinVoltage)
	//                   (kAudioCvInVoltageAtPlus5V, kAudioCvInMaxVoltage)

	float voltage_span = kAudioCvInVoltageAtPlus5V - kAudioCvInVoltageAtMinus5V;
	float signal_span = kAudioCvInMaxVoltage - kAudioCvInMinVoltage;

	// Scale factor: change in output per unit change in input
	voltage_scale_ = signal_span / voltage_span;

	// Offset: output value when input is zero
	voltage_offset_ = kAudioCvInMinVoltage - (kAudioCvInVoltageAtMinus5V * voltage_scale_);
}

}  // namespace brain::io