// pots.cpp
// Multiplexed potentiometer reader. ADC sampling is owned by `AdcEngine`;
// this class subscribes to the pot mux ADC channel and runs a small
// settle-and-average state machine driven by the engine's sample callbacks.
// Mux GPIO switching between pots happens between averaged readings.
#include "pots-core.h"

#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include "adc-engine.h"
#include "common.h"

namespace {

uint16_t output_max_for_resolution(uint8_t resolution) {
	if (resolution == 0) return 0;
	const uint8_t clamped_resolution = (resolution > 15) ? 15 : resolution;
	return static_cast<uint16_t>((1u << clamped_resolution) - 1u);
}

}  // namespace


PotsConfig create_default_config(uint8_t num_pots, uint8_t output_resolution) {
	PotsConfig cfg = {};
	cfg.simple = false;
	cfg.adc_gpio = GPIO_BRAIN_POTMUX_ADC;
	cfg.s0_gpio = GPIO_BRAIN_POTMUX_S0;
	cfg.s1_gpio = GPIO_BRAIN_POTMUX_S1;
	cfg.num_pots = (num_pots > 3) ? 3 : num_pots;
	for (int i = 0; i < cfg.num_pots; ++i) {
		cfg.channel_map[i] = i;
	}
	cfg.output_resolution = output_resolution;
	cfg.settling_delay_us = 0;
	cfg.samples_per_read = 6;
	cfg.change_threshold = 1;
	cfg.settle_discard_samples = 2;
	return cfg;
}

Pots::Pots() {
	for (int i = 0; i < kMaxPots; ++i) {
		buffered_raw_[i] = 0;
		buffered_values_[i] = 0;
		last_values_[i] = 0;
	}
}

Pots::~Pots() {
	release_subscription();
}

void Pots::init(const PotsConfig& cfg) {
	release_subscription();

	config_ = cfg;
	if (config_.num_pots > kMaxPots) config_.num_pots = kMaxPots;
	if (config_.samples_per_read == 0) config_.samples_per_read = 1;

	gpio_init(config_.s0_gpio);
	gpio_set_dir(config_.s0_gpio, GPIO_OUT);
	gpio_init(config_.s1_gpio);
	gpio_set_dir(config_.s1_gpio, GPIO_OUT);

	active_pot_index_ = 0;
	pot_accumulator_ = 0;
	pot_samples_collected_ = 0;
	pot_discard_remaining_ = config_.settle_discard_samples;
	set_mux_channel(config_.channel_map[active_pot_index_]);

	for (uint8_t i = 0; i < kMaxPots; ++i) {
		buffered_raw_[i] = 0;
		buffered_values_[i] = 0;
		last_values_[i] = 0;
	}

	const uint8_t adc_channel = static_cast<uint8_t>(config_.adc_gpio - 26);
	adc_token_ = AdcEngine::instance().register_channel(
		adc_channel,
		[this](uint16_t raw) { on_adc_sample(raw); });
}

void Pots::reconfigure(const PotsConfig& cfg) {
	init(cfg);
}

void Pots::set_simple(bool /*simple*/) {
	// Legacy no-op.
}

void Pots::set_optimized_sampling_enabled(bool /*enabled*/) {
	// Legacy no-op.
}

bool Pots::is_optimized_sampling_enabled() const {
	return true;
}

void Pots::set_output_resolution(uint8_t resolution) {
	config_.output_resolution = resolution;
}

void Pots::set_settling_delay_us(uint32_t /*delay*/) {
	// Legacy no-op.
}

void Pots::set_samples_per_read(uint8_t samples) {
	config_.samples_per_read = samples > 0 ? samples : 1;
}

void Pots::set_change_threshold(uint16_t threshold) {
	config_.change_threshold = threshold;
}

void Pots::set_mux_channel(uint8_t ch) {
	ch &= 0x03;
	gpio_put(config_.s0_gpio, ch & 0x01);
	gpio_put(config_.s1_gpio, (ch >> 1) & 0x01);
}

uint16_t Pots::map_to_output(uint16_t raw) const {
	const uint16_t output_max = output_max_for_resolution(config_.output_resolution);
	if (output_max == 0) return 0;
	return static_cast<uint16_t>((static_cast<uint32_t>(raw) * output_max) / kAdcMaxValue);
}

void Pots::on_adc_sample(uint16_t raw) {
	if (config_.num_pots == 0) return;

	if (pot_discard_remaining_ > 0) {
		--pot_discard_remaining_;
		return;
	}

	pot_accumulator_ += raw;
	++pot_samples_collected_;
	if (pot_samples_collected_ < config_.samples_per_read) return;

	const uint16_t averaged_raw = static_cast<uint16_t>(pot_accumulator_ / pot_samples_collected_);
	const uint16_t mapped = map_to_output(averaged_raw);
	const uint8_t pot_idx = active_pot_index_;

	buffered_raw_[pot_idx] = averaged_raw;
	buffered_values_[pot_idx] = mapped;

	if (mapped > last_values_[pot_idx] + config_.change_threshold ||
		mapped + config_.change_threshold < last_values_[pot_idx]) {
		last_values_[pot_idx] = mapped;
		if (on_change_) on_change_(pot_idx, mapped);
	}

	pot_accumulator_ = 0;
	pot_samples_collected_ = 0;

	if (config_.num_pots > 1) {
		active_pot_index_ = static_cast<uint8_t>((pot_idx + 1) % config_.num_pots);
		set_mux_channel(config_.channel_map[active_pot_index_]);

		// At the moment of the mux flip, samples already captured by the ADC
		// (sitting in the DMA ring or the ADC FIFO) still reflect the OLD mux
		// state. They will arrive at this callback before any post-flip sample
		// does. Discard the full pipeline depth so the new pot's average is
		// only built from samples captured AFTER the flip; then add the
		// configured analog-settle discard on top.
		//
		// Without this, `samples_per_read` averages mix in N stale samples from
		// the previous pot, producing a `prev_value / samples_per_read`
		// contamination term in the new pot's reading.
		const uint8_t adc_channel = static_cast<uint8_t>(config_.adc_gpio - 26);
		const uint16_t pipeline_pending = AdcEngine::instance()
			.pending_sample_count_for_channel_unlocked(adc_channel);
		pot_discard_remaining_ = static_cast<uint16_t>(
			pipeline_pending + config_.settle_discard_samples);
	} else {
		pot_discard_remaining_ = config_.settle_discard_samples;
	}
}

void Pots::release_subscription() {
	if (adc_token_ != 0) {
		AdcEngine::instance().unregister(adc_token_);
		adc_token_ = 0;
	}
}

uint16_t Pots::get_raw(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	return buffered_raw_[index];
}

uint8_t Pots::get_output_resolution() const {
	return config_.output_resolution;
}

uint16_t Pots::get_output_max() const {
	return output_max_for_resolution(config_.output_resolution);
}

uint16_t Pots::get(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	return buffered_values_[index];
}

uint16_t Pots::get_single(uint8_t index) {
	return get(index);
}

uint16_t Pots::get_buffered(uint8_t index) const {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	return buffered_values_[index];
}

void Pots::scan() {
	// No-op: AdcEngine keeps buffered values fresh via the on_adc_sample callback.
}

void Pots::set_on_change(std::function<void(uint8_t, uint16_t)> cb) {
	on_change_ = cb;
}

uint8_t Pots::get_num_pots() const {
	return config_.num_pots;
}
