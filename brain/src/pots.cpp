// pots.cpp
// Implementation for multiplexed potentiometer reader using 74HC4051
// Handles ADC sampling, channel switching, and change detection for Brain module
#include "pots-core.h"

#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include "adc-arbiter.h"
#include "gpio-setup.h"


PotsConfig create_default_config(uint8_t num_pots, uint8_t output_resolution) {
	PotsConfig cfg = {};
	cfg.simple = false;
	cfg.adc_gpio = GPIO_BRAIN_POTMUX_ADC;
	cfg.s0_gpio = GPIO_BRAIN_POTMUX_S0;
	cfg.s1_gpio = GPIO_BRAIN_POTMUX_S1;
	cfg.num_pots = (num_pots > 3) ? 3 : num_pots;  // Brain module has 3 pots
	for (int i = 0; i < cfg.num_pots; ++i) {
		cfg.channel_map[i] = i;	 // Direct mapping: pot 0 -> channel 0, etc.
	}
	cfg.output_resolution = output_resolution;
	cfg.settling_delay_us = 200;  // Reasonable default for 74HC4051
	cfg.samples_per_read = 6;  // Good balance of stability vs speed
	cfg.change_threshold = 1;  // Sensitive change detection
	return cfg;
}

Pots::Pots() {
	for (int i = 0; i < kMaxPots; ++i) {
		last_values_[i] = 0;
		buffered_values_[i] = 0;
	}
	buffer_valid_ = false;
}

void Pots::init(const PotsConfig& cfg) {
	config_ = cfg;
	// Ensure num_pots doesn't exceed our array size
	if (config_.num_pots > kMaxPots) {
		config_.num_pots = kMaxPots;
	}

	{
		BrainAdcLockGuard guard;
		adc_init();
		gpio_init(cfg.s0_gpio);
		gpio_set_dir(cfg.s0_gpio, GPIO_OUT);
		gpio_put(cfg.s0_gpio, 0);
		gpio_init(cfg.s1_gpio);
		gpio_set_dir(cfg.s1_gpio, GPIO_OUT);
		gpio_put(cfg.s1_gpio, 0);
		adc_gpio_init(cfg.adc_gpio);
		// Select ADC input (Pico SDK: ADC input = GPIO - 26)
		adc_select_input(cfg.adc_gpio - 26);
		// Small guard delay
		busy_wait_us_32(cfg.settling_delay_us);
	}

	for (uint8_t i = 0; i < kMaxPots; ++i) {
		last_values_[i] = 0;
		buffered_values_[i] = 0;
	}

	for (uint8_t i = 0; i < config_.num_pots && i < kMaxPots; ++i) {
		uint16_t val = get_single(i);
		last_values_[i] = val;
		buffered_values_[i] = val;
	}
	buffer_valid_ = true;
}

void Pots::reconfigure(const PotsConfig& cfg) {
	init(cfg);
}

void Pots::set_simple(bool simple) {
	config_.simple = simple;
	buffer_valid_ = false;
}

void Pots::set_optimized_sampling_enabled(bool enabled) {
	optimized_sampling_enabled_ = enabled;
	buffer_valid_ = false;
}

bool Pots::is_optimized_sampling_enabled() const {
	return optimized_sampling_enabled_;
}

void Pots::set_output_resolution(uint8_t resolution) {
	config_.output_resolution = resolution;
	buffer_valid_ = false;
}

void Pots::set_settling_delay_us(uint32_t delay) {
	config_.settling_delay_us = delay;
	buffer_valid_ = false;
}

void Pots::set_samples_per_read(uint8_t samples) {
	config_.samples_per_read = samples;
	buffer_valid_ = false;
}

void Pots::set_change_threshold (uint16_t threshold) {
	config_.change_threshold = threshold;
}

void Pots::set_mux_channel(uint8_t ch) {
	ch &= 0x03;
	gpio_put(config_.s0_gpio, ch & 0x01);
	gpio_put(config_.s1_gpio, (ch >> 1) & 0x01);
}

uint16_t Pots::read_channel_once(uint8_t ch) {
	BrainAdcLockGuard guard;
	set_mux_channel(ch);

	// Reselect ADC input to ensure proper synchronization
	adc_select_input(config_.adc_gpio - 26);

	// Simple read is just reading the ADC once and that's it. It's the fastest
	// but lacks precision
	if (config_.simple || !optimized_sampling_enabled_) {
		// Fast path still performs a small settle + one throwaway read to reduce
		// mux channel carry-over.
		busy_wait_us_32(config_.settling_delay_us > 20 ? 20 : config_.settling_delay_us);
		(void) adc_read();
		uint16_t adc_value = adc_read();
		return adc_value;

	} else {
		busy_wait_us_32(config_.settling_delay_us > 100 ? config_.settling_delay_us : 100);

		// Discard multiple samples to ensure ADC has settled
		for (int i = 0; i < 3; i++) {
			(void) adc_read();
		}

		// Take actual readings
		uint32_t sum = 0;
		uint8_t samples = config_.samples_per_read > 0 ? config_.samples_per_read : 1;
		for (uint8_t i = 0; i < samples; ++i) {
			sum += adc_read();
			// Small delay between samples
			busy_wait_us_32(10);
		}
		return sum / samples;
	}
}

uint16_t Pots::get_raw(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	return read_channel_once(config_.channel_map[index]);
}

uint16_t Pots::get(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	if (!buffer_valid_) {
		scan();
		buffer_valid_ = true;
	}
	return buffered_values_[index];
}

uint16_t Pots::get_single(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;

	uint16_t raw = get_raw(index);

	// Map from 12-bit ADC (0-4095) to desired output resolution
	static constexpr uint16_t kAdcMaxValue = 4095;	// 12-bit ADC
	uint16_t output_max = (1 << config_.output_resolution) - 1;

	return (raw * output_max) / kAdcMaxValue;
}

uint16_t Pots::get_buffered(uint8_t index) const {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	return buffered_values_[index];
}

void Pots::scan() {
	for (uint8_t i = 0; i < config_.num_pots && i < kMaxPots; ++i) {
		uint16_t val = get_single(i);
		buffered_values_[i] = val;
		if (val > last_values_[i] + config_.change_threshold ||
			val + config_.change_threshold < last_values_[i]) {
			last_values_[i] = val;
			if (on_change_) {
				on_change_(i, val);
			}
		}
	}
	buffer_valid_ = true;
}

void Pots::set_on_change(std::function<void(uint8_t, uint16_t)> cb) {
	on_change_ = cb;
}

uint8_t Pots::get_num_pots() const {
	return config_.num_pots;
}
