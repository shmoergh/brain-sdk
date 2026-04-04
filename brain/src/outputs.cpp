#include "outputs.h"

#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include <cmath>
#include <cstdio>

#include "storage.h"

namespace {

int16_t round_to_int16(float value) {
	if (value >= 0.0f) {
		return static_cast<int16_t>(value + 0.5f);
	}
	return static_cast<int16_t>(value - 0.5f);
}

}  // namespace

Outputs::Outputs(uint pulse_out_gpio)
	: pulse_out_gpio_(pulse_out_gpio) {}

bool Outputs::init_audio_cv(spi_inst_t* spi_instance, uint cs_pin, uint sck_pin, uint tx_pin,
	uint coupling_pin_a, uint coupling_pin_b) {
	if (spi_instance != spi0 && spi_instance != spi1) {
		fprintf(stderr, "Outputs: Invalid SPI instance\n");
		return false;
	}

	spi_instance_ = spi_instance;
	cs_pin_ = cs_pin;
	sck_pin_ = sck_pin;
	tx_pin_ = tx_pin;
	coupling_pin_a_ = coupling_pin_a;
	coupling_pin_b_ = coupling_pin_b;

	spi_init(spi_instance_, kSpiFrequency);
	spi_set_format(spi_instance_, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

	gpio_set_function(sck_pin_, GPIO_FUNC_SPI);
	gpio_set_function(tx_pin_, GPIO_FUNC_SPI);

	gpio_init(cs_pin_);
	gpio_set_dir(cs_pin_, GPIO_OUT);
	gpio_put(cs_pin_, 1);

	gpio_init(coupling_pin_a_);
	gpio_set_dir(coupling_pin_a_, GPIO_OUT);
	gpio_put(coupling_pin_a_, 0);

	gpio_init(coupling_pin_b_);
	gpio_set_dir(coupling_pin_b_, GPIO_OUT);
	gpio_put(coupling_pin_b_, 0);

	return true;
}

void Outputs::init_pulse() {
	gpio_init(pulse_out_gpio_);
	gpio_put(pulse_out_gpio_, true);
	gpio_set_dir(pulse_out_gpio_, GPIO_OUT);
	pulse_initialized_ = true;
	pulse_state_ = false;
}

bool Outputs::init() {
	init_pulse();
	return init_audio_cv();
}

bool Outputs::set_voltage(AudioCvOutChannel channel, float voltage) {
	if (voltage < 0.0f || voltage > kMaxVoltage) {
		fprintf(stderr, "Outputs: Voltage %.2fV out of range (0-%.1fV)\n", voltage, kMaxVoltage);
		return false;
	}

	uint16_t dac_value = voltage_to_dac(voltage);
	write_dac_channel(channel, dac_value);
	return true;
}

bool Outputs::set_voltage_calibrated(AudioCvOutChannel channel, float target_voltage) {
	float clamped_voltage = clamp_voltage(target_voltage);
	uint16_t raw_dac_value = voltage_to_dac(clamped_voltage);

	int32_t calibrated_dac_value = raw_dac_value;
	if (calibration_loaded_) {
		calibrated_dac_value += interpolated_offset_lsb(channel, clamped_voltage);
	}

	if (calibrated_dac_value < 0) {
		calibrated_dac_value = 0;
	}
	if (calibrated_dac_value > kMaxDacValue) {
		calibrated_dac_value = kMaxDacValue;
	}

	write_dac_channel(channel, static_cast<uint16_t>(calibrated_dac_value));
	return true;
}

bool Outputs::set_calibration(const CvCalibrationV1& cal) {
	for (int i = 0; i < 10; i++) {
		calibration_a_offset_lsb_[i] = cal.a_offset_lsb[i];
		calibration_b_offset_lsb_[i] = cal.b_offset_lsb[i];
	}
	calibration_loaded_ = true;
	return true;
}

void Outputs::clear_calibration() {
	for (int i = 0; i < 10; i++) {
		calibration_a_offset_lsb_[i] = 0;
		calibration_b_offset_lsb_[i] = 0;
	}
	calibration_loaded_ = false;
}

bool Outputs::load_calibration_from_flash() {
	brain::storage::CvCalibrationV1 calibration{};
	brain::storage::StorageStatus status = brain::storage::read_cv_calibration(&calibration);
	if (status != brain::storage::StorageStatus::kOk) {
		clear_calibration();
		return false;
	}

	return set_calibration(calibration);
}

bool Outputs::has_calibration() const {
	return calibration_loaded_;
}

uint16_t Outputs::get_last_dac_value(AudioCvOutChannel channel) const {
	return (channel == AudioCvOutChannel::kChannelA) ? last_dac_value_a_ : last_dac_value_b_;
}

bool Outputs::set_coupling(AudioCvOutChannel channel, AudioCvOutCoupling coupling) {
	uint coupling_pin =
		(channel == AudioCvOutChannel::kChannelA) ? coupling_pin_a_ : coupling_pin_b_;

	gpio_put(coupling_pin, static_cast<bool>(coupling));
	return true;
}

void Outputs::pulse_set(bool on) {
	if (!pulse_initialized_) {
		init_pulse();
	}
	if (on != pulse_state_) {
		pulse_state_ = on;
		gpio_put(pulse_out_gpio_, !on);
	}
}

bool Outputs::pulse_get() const {
	return pulse_state_;
}

void Outputs::write_dac_channel(AudioCvOutChannel channel, uint16_t dac_value) {
	if (channel == AudioCvOutChannel::kChannelA) {
		last_dac_value_a_ = dac_value;
	} else {
		last_dac_value_b_ = dac_value;
	}

	uint8_t config =
		(channel == AudioCvOutChannel::kChannelA ? kMCP4822_CHANNEL_A : kMCP4822_CHANNEL_B) << 3 |
		0 << 2 | kMCP4822_GAIN << 1 | kMCP4822_ACTIVE;

	uint8_t data[2];
	data[0] = config << 4 | (dac_value & 0xf00) >> 8;
	data[1] = dac_value & 0xff;

	asm volatile("nop \n nop \n nop");
	gpio_put(cs_pin_, 0);
	asm volatile("nop \n nop \n nop");

	spi_write_blocking(spi_instance_, data, 2);

	asm volatile("nop \n nop \n nop");
	gpio_put(cs_pin_, 1);
	asm volatile("nop \n nop \n nop");
}

uint16_t Outputs::voltage_to_dac(float voltage) {
	float normalized = voltage / kMaxVoltage;
	uint16_t dac_value = static_cast<uint16_t>(normalized * kMaxDacValue + 0.5f);
	return (dac_value > kMaxDacValue) ? kMaxDacValue : dac_value;
}

float Outputs::clamp_voltage(float voltage) const {
	if (voltage < 0.0f) {
		return 0.0f;
	}
	if (voltage > kMaxVoltage) {
		return kMaxVoltage;
	}
	return voltage;
}

int16_t Outputs::interpolated_offset_lsb(AudioCvOutChannel channel, float clamped_voltage) const {
	const int16_t* offsets = (channel == AudioCvOutChannel::kChannelA)
		? calibration_a_offset_lsb_
		: calibration_b_offset_lsb_;

	if (clamped_voltage <= 0.0f) {
		return 0;
	}
	if (clamped_voltage < 1.0f) {
		return round_to_int16(static_cast<float>(offsets[0]) * clamped_voltage);
	}
	if (clamped_voltage >= kMaxVoltage) {
		return offsets[9];
	}

	const int lower_whole_volt = static_cast<int>(clamped_voltage);
	const int lower_idx = lower_whole_volt - 1;
	const int upper_idx = lower_idx + 1;
	const float t = clamped_voltage - static_cast<float>(lower_whole_volt);

	const float lower_offset = static_cast<float>(offsets[lower_idx]);
	const float upper_offset = static_cast<float>(offsets[upper_idx]);
	return round_to_int16(lower_offset + (upper_offset - lower_offset) * t);
}
