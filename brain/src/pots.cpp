// pots.cpp
// Time-driven multiplexed pot reader. The mux flip and the post-flip read
// are separated by a wall-clock interval (`settle_us`), which is the only
// requirement for the read to reflect the new pot's voltage rather than
// the previous one. ADC sampling is done by `AdcEngine`; this class polls
// the latest sample at known times.
#include "pots-core.h"

#include <hardware/gpio.h>
#include <pico/stdlib.h>

#include "adc-engine.h"
#include "common.h"

namespace {

uint16_t output_max_for_resolution(uint8_t resolution) {
	if (resolution == 0) return 0;
	const uint8_t clamped = (resolution > 15) ? 15 : resolution;
	return static_cast<uint16_t>((1u << clamped) - 1u);
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
	cfg.settle_discard_samples = 0;
	cfg.settle_us = 1000;	 // 1 ms — huge for analog, trivial for human pots.
	return cfg;
}

Pots::Pots() = default;

Pots::~Pots() {
	if (timer_running_) {
		cancel_repeating_timer(&timer_);
		timer_running_ = false;
	}
}

void Pots::init(const PotsConfig& cfg) {
	if (timer_running_) {
		cancel_repeating_timer(&timer_);
		timer_running_ = false;
	}

	config_ = cfg;
	if (config_.num_pots > kMaxPots) config_.num_pots = kMaxPots;
	if (config_.samples_per_read == 0) config_.samples_per_read = 1;
	if (config_.settle_us == 0) config_.settle_us = 1000;

	gpio_init(config_.s0_gpio);
	gpio_set_dir(config_.s0_gpio, GPIO_OUT);
	gpio_init(config_.s1_gpio);
	gpio_set_dir(config_.s1_gpio, GPIO_OUT);

	for (uint8_t i = 0; i < kMaxPots; ++i) {
		buffered_raw_[i] = 0;
		buffered_values_[i] = 0;
		last_values_[i] = 0;
	}

	// Subscribe to the pot mux ADC channel so AdcEngine includes it in the
	// round-robin and keeps `latest_[pot_mux_channel]` fresh.
	const uint8_t adc_channel = static_cast<uint8_t>(config_.adc_gpio - 26);
	AdcEngine::instance().enable_channel(adc_channel);

	// Tick rate: a small fraction of `settle_us` so the state machine
	// progresses smoothly. 250 µs gives ~4 ticks per 1 ms settle window
	// and ~250 µs per accumulation sample — total ~3 ms per pot, ~9 ms
	// per full 3-pot cycle, which is plenty for human knob movement.
	constexpr int64_t kTickPeriodUs = 250;

	start_settling_for(0);
	initialized_ = true;

	timer_running_ = add_repeating_timer_us(
		-kTickPeriodUs,
		&Pots::timer_callback,
		this,
		&timer_);
}

void Pots::reconfigure(const PotsConfig& cfg) {
	init(cfg);
}

void Pots::set_output_resolution(uint8_t resolution) {
	config_.output_resolution = resolution;
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

void Pots::start_settling_for(uint8_t pot_index) {
	active_pot_index_ = pot_index;
	samples_collected_ = 0;
	accumulator_ = 0;
	set_mux_channel(config_.channel_map[pot_index]);
	settle_until_ = make_timeout_time_us(config_.settle_us);
	phase_ = Phase::kSettling;
}

bool Pots::timer_callback(repeating_timer_t* timer) {
	auto* self = static_cast<Pots*>(timer->user_data);
	if (self == nullptr || !self->timer_running_) return false;
	self->on_tick();
	return self->timer_running_;
}

void Pots::on_tick() {
	if (config_.num_pots == 0) return;

	if (phase_ == Phase::kSettling) {
		if (absolute_time_diff_us(get_absolute_time(), settle_until_) > 0) {
			return;	 // still settling
		}
		phase_ = Phase::kSampling;
		// Fall through to take the first sample this tick.
	}

	const uint8_t adc_channel = static_cast<uint8_t>(config_.adc_gpio - 26);
	const uint16_t raw = AdcEngine::instance().get_latest(adc_channel);
	accumulator_ += raw;
	++samples_collected_;

	if (samples_collected_ < config_.samples_per_read) return;

	// Average is complete for this pot.
	const uint16_t averaged_raw = static_cast<uint16_t>(accumulator_ / samples_collected_);
	const uint16_t mapped = map_to_output(averaged_raw);
	const uint8_t pot_idx = active_pot_index_;

	buffered_raw_[pot_idx] = averaged_raw;
	buffered_values_[pot_idx] = mapped;

	if (mapped > last_values_[pot_idx] + config_.change_threshold ||
		mapped + config_.change_threshold < last_values_[pot_idx]) {
		last_values_[pot_idx] = mapped;
		if (on_change_) on_change_(pot_idx, mapped);
	}

	// Advance to next pot (wrap), flip mux, start settling again.
	const uint8_t next = (config_.num_pots > 1)
		? static_cast<uint8_t>((pot_idx + 1) % config_.num_pots)
		: pot_idx;
	start_settling_for(next);
}

uint16_t Pots::get(uint8_t index) {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	return buffered_values_[index];
}

uint16_t Pots::get_buffered(uint8_t index) const {
	if (index >= config_.num_pots || index >= kMaxPots) return 0;
	return buffered_values_[index];
}

uint16_t Pots::get_single(uint8_t index) {
	return get(index);
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

void Pots::set_on_change(std::function<void(uint8_t, uint16_t)> cb) {
	on_change_ = cb;
}

uint8_t Pots::get_num_pots() const {
	return config_.num_pots;
}
