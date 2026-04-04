#include "outputs.h"

#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include <cstdio>

#include "storage.h"

namespace {

int32_t div_round_nearest(int32_t numerator, int32_t denominator) {
	if (denominator == 0) return 0;
	if (numerator >= 0) {
		return (numerator + denominator / 2) / denominator;
	}
	return (numerator - denominator / 2) / denominator;
}

int16_t saturate_int16(int32_t value) {
	if (value > 32767) return 32767;
	if (value < -32768) return -32768;
	return static_cast<int16_t>(value);
}

}  // namespace

Outputs::Outputs(uint pulse_out_gpio)
	: pulse_out_gpio_(pulse_out_gpio) {}

bool Outputs::init_audio_cv(spi_inst_t* spi_instance, uint cs_pin, uint sck_pin, uint tx_pin,
	uint coupling_pin_a, uint coupling_pin_b, AudioCvOutRange range_a, AudioCvOutRange range_b) {
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
	gpio_init(coupling_pin_b_);
	gpio_set_dir(coupling_pin_b_, GPIO_OUT);

	set_output_range(AudioCvOutChannel::kChannelA, range_a);
	set_output_range(AudioCvOutChannel::kChannelB, range_b);

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

bool Outputs::set_voltage_millivolts(AudioCvOutChannel channel, int32_t millivolts) {
	int32_t dac_input_millivolts = 0;
	if (!to_dac_input_millivolts(channel, millivolts, false, &dac_input_millivolts)) {
		AudioCvOutRange range = get_output_range(channel);
		fprintf(
			stderr,
			"Outputs: Voltage %ldmV out of range for %s\n",
			static_cast<long>(millivolts),
			range_to_string(range));
		return false;
	}

	uint16_t dac_value = millivolts_to_dac(dac_input_millivolts);
	write_dac_channel(channel, dac_value);

	if (channel == AudioCvOutChannel::kChannelA) {
		last_set_millivolts_a_ = millivolts;
	} else {
		last_set_millivolts_b_ = millivolts;
	}
	return true;
}

bool Outputs::set_voltage_calibrated_millivolts(AudioCvOutChannel channel, int32_t target_millivolts) {
	int32_t clamped_dac_input_millivolts = 0;
	to_dac_input_millivolts(channel, target_millivolts, true, &clamped_dac_input_millivolts);

	int32_t calibrated_dac_value = millivolts_to_dac(clamped_dac_input_millivolts);
	if (calibration_loaded_) {
		calibrated_dac_value += interpolated_offset_lsb(channel, clamped_dac_input_millivolts);
	}

	if (calibrated_dac_value < 0) {
		calibrated_dac_value = 0;
	}
	if (calibrated_dac_value > kMaxDacValue) {
		calibrated_dac_value = kMaxDacValue;
	}

	write_dac_channel(channel, static_cast<uint16_t>(calibrated_dac_value));

	if (channel == AudioCvOutChannel::kChannelA) {
		last_set_millivolts_a_ = target_millivolts;
	} else {
		last_set_millivolts_b_ = target_millivolts;
	}
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
	CvCalibrationV1 calibration{};
	StorageStatus status = read_cv_calibration(&calibration);
	if (status != StorageStatus::kOk) {
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

int32_t Outputs::get_last_set_millivolts(AudioCvOutChannel channel) const {
	return (channel == AudioCvOutChannel::kChannelA) ? last_set_millivolts_a_ : last_set_millivolts_b_;
}

bool Outputs::set_output_range(AudioCvOutChannel channel, AudioCvOutRange range) {
	uint coupling_pin =
		(channel == AudioCvOutChannel::kChannelA) ? coupling_pin_a_ : coupling_pin_b_;

	if (channel == AudioCvOutChannel::kChannelA) {
		range_a_ = range;
	} else {
		range_b_ = range;
	}

	// Hardware "AC" means subtracting 5V from the DAC/amplified signal.
	const bool subtract_5v = (range == AudioCvOutRange::kRangeMinus5To5V);
	gpio_put(coupling_pin, subtract_5v);
	return true;
}

AudioCvOutRange Outputs::get_output_range(AudioCvOutChannel channel) const {
	return (channel == AudioCvOutChannel::kChannelA) ? range_a_ : range_b_;
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

bool Outputs::to_dac_input_millivolts(
	AudioCvOutChannel channel,
	int32_t requested_millivolts,
	bool clamp_to_range,
	int32_t* dac_input_millivolts) const {
	const AudioCvOutRange range = get_output_range(channel);

	int32_t min_mv = kUnipolarMinMillivolts;
	int32_t max_mv = kUnipolarMaxMillivolts;
	int32_t offset_mv = 0;
	if (range == AudioCvOutRange::kRangeMinus5To5V) {
		min_mv = kBipolarMinMillivolts;
		max_mv = kBipolarMaxMillivolts;
		offset_mv = kBipolarOffsetMillivolts;
	}

	int32_t clamped = requested_millivolts;
	if (clamped < min_mv || clamped > max_mv) {
		if (!clamp_to_range) {
			return false;
		}
		if (clamped < min_mv) clamped = min_mv;
		if (clamped > max_mv) clamped = max_mv;
	}

	*dac_input_millivolts = clamped + offset_mv;
	return true;
}

uint16_t Outputs::millivolts_to_dac(int32_t dac_input_millivolts) const {
	if (dac_input_millivolts <= 0) return 0;
	if (dac_input_millivolts >= kMaxOutputMillivolts) return kMaxDacValue;

	const int32_t scaled = dac_input_millivolts * static_cast<int32_t>(kMaxDacValue);
	const int32_t rounded = div_round_nearest(scaled, kMaxOutputMillivolts);
	if (rounded < 0) return 0;
	if (rounded > static_cast<int32_t>(kMaxDacValue)) return kMaxDacValue;
	return static_cast<uint16_t>(rounded);
}

int16_t Outputs::interpolated_offset_lsb(AudioCvOutChannel channel, int32_t dac_input_millivolts) const {
	const int16_t* offsets = (channel == AudioCvOutChannel::kChannelA)
		? calibration_a_offset_lsb_
		: calibration_b_offset_lsb_;

	if (dac_input_millivolts <= 0) {
		return 0;
	}
	if (dac_input_millivolts < 1000) {
		const int32_t scaled = static_cast<int32_t>(offsets[0]) * dac_input_millivolts;
		return saturate_int16(div_round_nearest(scaled, 1000));
	}
	if (dac_input_millivolts >= kMaxOutputMillivolts) {
		return offsets[9];
	}

	const int32_t lower_whole_volt = dac_input_millivolts / 1000;
	const int32_t lower_idx = lower_whole_volt - 1;
	const int32_t upper_idx = lower_idx + 1;
	const int32_t within_volt_millivolts = dac_input_millivolts - (lower_whole_volt * 1000);

	const int32_t lower_offset = offsets[lower_idx];
	const int32_t upper_offset = offsets[upper_idx];
	const int32_t delta = upper_offset - lower_offset;
	const int32_t interpolated =
		lower_offset + div_round_nearest(delta * within_volt_millivolts, 1000);
	return saturate_int16(interpolated);
}

const char* Outputs::range_to_string(AudioCvOutRange range) const {
	if (range == AudioCvOutRange::kRangeMinus5To5V) {
		return "-5000..5000mV";
	}
	return "0..10000mV";
}
