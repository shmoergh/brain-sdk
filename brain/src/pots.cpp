// pots.cpp
// Pots is a thin client over brain::internal::AdcEngine. The engine owns the ADC
// and runs the mux/settle/average state machine; Pots reads stable averaged values
// from snapshots and applies the configured output-resolution mapping.

#include "pots-core.h"

#include "adc-engine.h"
#include "common.h"
#include "gpio-setup.h"

namespace {

uint16_t output_max_for_resolution(uint8_t resolution) {
	if (resolution == 0) {
		return 0;
	}

	const uint8_t clamped_resolution = (resolution > 15) ? 15 : resolution;
	return static_cast<uint16_t>((1u << clamped_resolution) - 1u);
}

uint16_t scale_raw_to_output(uint16_t raw, uint16_t output_max) {
	if (output_max == 0) {
		return 0;
	}
	return static_cast<uint16_t>((static_cast<uint32_t>(raw) * output_max) / kAdcMaxValue);
}

}  // namespace


PotsConfig create_default_config(uint8_t num_pots, uint8_t output_resolution) {
	PotsConfig cfg = {};
	cfg.simple = false;
	cfg.adc_gpio = GPIO_BRAIN_POTMUX_ADC;
	cfg.s0_gpio = GPIO_BRAIN_POTMUX_S0;
	cfg.s1_gpio = GPIO_BRAIN_POTMUX_S1;
	cfg.num_pots = (num_pots > 3) ? 3 : num_pots;  // Brain module has 3 pots
	for (int i = 0; i < cfg.num_pots; ++i) {
		cfg.channel_map[i] = i;	 // Direct mapping: pot 0 -> mux channel 0, etc.
	}
	cfg.output_resolution = output_resolution;
	cfg.settling_delay_us = 200;  // Translated to a discard-sample count by AdcEngine.
	cfg.samples_per_read = 6;	  // Averaging depth used by AdcEngine.
	cfg.change_threshold = 1;	  // Sensitive change detection.
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
	if (config_.num_pots > kMaxPots) {
		config_.num_pots = kMaxPots;
	}

	for (uint8_t i = 0; i < kMaxPots; ++i) {
		last_values_[i] = 0;
		buffered_values_[i] = 0;
	}

	brain::internal::AdcEngine::instance().start(config_);
	buffer_valid_ = true;
}

void Pots::reconfigure(const PotsConfig& cfg) {
	config_ = cfg;
	if (config_.num_pots > kMaxPots) {
		config_.num_pots = kMaxPots;
	}
	brain::internal::AdcEngine::instance().reconfigure_pots(config_);
}

void Pots::set_simple(bool simple) {
	// Advisory only: AdcEngine has a single deterministic scan path. No effect.
	config_.simple = simple;
}

void Pots::set_optimized_sampling_enabled(bool enabled) {
	// Advisory only: AdcEngine has a single deterministic scan path. No effect.
	optimized_sampling_enabled_ = enabled;
}

bool Pots::is_optimized_sampling_enabled() const {
	return optimized_sampling_enabled_;
}

void Pots::set_output_resolution(uint8_t resolution) {
	config_.output_resolution = resolution;
}

void Pots::set_settling_delay_us(uint32_t delay) {
	config_.settling_delay_us = delay;
	brain::internal::AdcEngine::instance().reconfigure_pots(config_);
}

void Pots::set_samples_per_read(uint8_t samples) {
	config_.samples_per_read = samples;
	brain::internal::AdcEngine::instance().reconfigure_pots(config_);
}

void Pots::set_change_threshold(uint16_t threshold) {
	config_.change_threshold = threshold;
}

uint16_t Pots::get_raw(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	const auto snapshot = brain::internal::AdcEngine::instance().get_snapshot();
	return snapshot.pot_raw[config_.channel_map[index]];
}

uint8_t Pots::get_output_resolution() const {
	return config_.output_resolution;
}

uint16_t Pots::get_output_max() const {
	return output_max_for_resolution(config_.output_resolution);
}

uint16_t Pots::get(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	if (!buffer_valid_) {
		scan();
	}
	return buffered_values_[index];
}

uint16_t Pots::get_single(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	const uint16_t raw = get_raw(index);
	return scale_raw_to_output(raw, output_max_for_resolution(config_.output_resolution));
}

uint16_t Pots::get_buffered(uint8_t index) const {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	return buffered_values_[index];
}

void Pots::scan() {
	const auto snapshot = brain::internal::AdcEngine::instance().get_snapshot();
	const uint16_t output_max = output_max_for_resolution(config_.output_resolution);

	for (uint8_t i = 0; i < config_.num_pots && i < kMaxPots; ++i) {
		const uint16_t raw = snapshot.pot_raw[config_.channel_map[i]];
		const uint16_t val = scale_raw_to_output(raw, output_max);
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
