#include "inputs.h"

#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#include "adc-engine.h"
#include "common.h"

namespace {

static Inputs* pulse_irq_instances[NUM_BANK0_GPIOS] = {nullptr};

int32_t round_div_i64(int64_t numerator, int64_t denominator) {
	if (denominator == 0) return 0;
	if (numerator >= 0) {
		return static_cast<int32_t>((numerator + denominator / 2) / denominator);
	}
	return static_cast<int32_t>((numerator - denominator / 2) / denominator);
}

}  // namespace

Inputs::Inputs(uint pulse_in_gpio)
	: pulse_in_gpio_(pulse_in_gpio) {}

Inputs::~Inputs() {
	release_audio_cv_subscriptions();
}

bool Inputs::init_audio_cv() {
	calculate_conversion_parameters();

	if (adc_token_a_ == 0) {
		const uint8_t channel_a = GPIO_BRAIN_AUDIO_CV_IN_A - 26;
		adc_token_a_ = AdcEngine::instance().register_channel(
			channel_a,
			[this](uint16_t raw) { channel_raw_[AudioCvInChannel::kChannelA] = raw; });
	}
	if (adc_token_b_ == 0) {
		const uint8_t channel_b = GPIO_BRAIN_AUDIO_CV_IN_B - 26;
		adc_token_b_ = AdcEngine::instance().register_channel(
			channel_b,
			[this](uint16_t raw) { channel_raw_[AudioCvInChannel::kChannelB] = raw; });
	}

	return adc_token_a_ != 0 && adc_token_b_ != 0;
}

bool Inputs::init_pulse() {
	gpio_init(pulse_in_gpio_);
	gpio_set_dir(pulse_in_gpio_, GPIO_IN);
	gpio_pull_up(pulse_in_gpio_);

	pulse_last_logical_state_ = pulse_read();
	pulse_filtered_state_ = pulse_last_logical_state_;
	pulse_last_change_time_us_ = time_us_32();
	pulse_initialized_ = true;

	if (pulse_in_gpio_ < NUM_BANK0_GPIOS) {
		pulse_irq_instances[pulse_in_gpio_] = this;
	}

	return true;
}

void Inputs::pulse_end() {
	if (pulse_interrupts_enabled_) {
		pulse_disable_interrupts();
	}

	if (pulse_in_gpio_ < NUM_BANK0_GPIOS && pulse_irq_instances[pulse_in_gpio_] == this) {
		pulse_irq_instances[pulse_in_gpio_] = nullptr;
	}

	gpio_set_dir(pulse_in_gpio_, GPIO_IN);
	gpio_disable_pulls(pulse_in_gpio_);
	pulse_initialized_ = false;
}

bool Inputs::init() {
	bool pulse_ok = init_pulse();
	bool audio_ok = init_audio_cv();
	return pulse_ok && audio_ok;
}

void Inputs::update_audio_cv() {
	// AdcEngine keeps `channel_raw_` fresh via callbacks; nothing to do here.
}

void Inputs::pulse_poll() {
	bool current_logical = pulse_read();

	if (pulse_glitch_filter_us_ > 0) {
		uint32_t now = time_us_32();

		if (current_logical != pulse_filtered_state_) {
			if (current_logical != pulse_last_logical_state_) {
				pulse_last_change_time_us_ = now;
			} else if ((now - pulse_last_change_time_us_) >= pulse_glitch_filter_us_) {
				pulse_filtered_state_ = current_logical;
			}
		}

		current_logical = pulse_filtered_state_;
	}

	if (current_logical != pulse_last_logical_state_) {
		if (current_logical && pulse_on_rise_callback_) {
			pulse_on_rise_callback_();
		} else if (!current_logical && pulse_on_fall_callback_) {
			pulse_on_fall_callback_();
		}

		pulse_last_logical_state_ = current_logical;
	}
}

void Inputs::update() {
	update_audio_cv();
	pulse_poll();
}

void Inputs::set_audio_cv_dma_enabled(bool /*enabled*/) {
	// Legacy no-op: AdcEngine always DMA-samples audio CV channels.
}

bool Inputs::is_audio_cv_dma_enabled() const {
	return true;
}

bool Inputs::is_audio_cv_dma_active() const {
	return adc_token_a_ != 0 && adc_token_b_ != 0;
}

uint16_t Inputs::get_raw(int channel) const {
	if (channel == AudioCvInChannel::kChannelA || channel == AudioCvInChannel::kChannelB) {
		return channel_raw_[channel];
	}
	return 0;
}

uint16_t Inputs::get_raw_channel_a() const {
	return channel_raw_[AudioCvInChannel::kChannelA];
}

uint16_t Inputs::get_raw_channel_b() const {
	return channel_raw_[AudioCvInChannel::kChannelB];
}

int32_t Inputs::get_voltage_millivolts(int channel) const {
	if (channel == AudioCvInChannel::kChannelA || channel == AudioCvInChannel::kChannelB) {
		return adc_to_millivolts(channel_raw_[channel]);
	}
	return 0;
}

int32_t Inputs::get_voltage_millivolts_channel_a() const {
	return adc_to_millivolts(channel_raw_[AudioCvInChannel::kChannelA]);
}

int32_t Inputs::get_voltage_millivolts_channel_b() const {
	return adc_to_millivolts(channel_raw_[AudioCvInChannel::kChannelB]);
}

bool Inputs::pulse_read() const {
	return !gpio_get(pulse_in_gpio_);
}

bool Inputs::pulse_read_raw() const {
	return gpio_get(pulse_in_gpio_);
}

void Inputs::pulse_on_rise(std::function<void()> cb) {
	pulse_on_rise_callback_ = cb;
}

void Inputs::pulse_on_fall(std::function<void()> cb) {
	pulse_on_fall_callback_ = cb;
}

void Inputs::pulse_set_input_glitch_filter_us(uint32_t us) {
	pulse_glitch_filter_us_ = us;
}

void Inputs::pulse_enable_interrupts() {
	if (!pulse_initialized_) {
		init_pulse();
	}
	if (!pulse_interrupts_enabled_) {
		gpio_set_irq_enabled_with_callback(
			pulse_in_gpio_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, true, &Inputs::gpio_irq_handler);
		pulse_interrupts_enabled_ = true;
	}
}

void Inputs::pulse_disable_interrupts() {
	if (pulse_interrupts_enabled_) {
		gpio_set_irq_enabled(pulse_in_gpio_, GPIO_IRQ_EDGE_RISE | GPIO_IRQ_EDGE_FALL, false);
		pulse_interrupts_enabled_ = false;
	}
}

int32_t Inputs::adc_to_millivolts(uint16_t adc_value) const {
	const int32_t adc_millivolts =
		round_div_i64(static_cast<int64_t>(adc_value) * 3300, kAdcMaxValue);
	const int32_t signal_delta_millivolts =
		round_div_i64(
			static_cast<int64_t>(adc_millivolts - adc_low_millivolts_) * signal_span_millivolts_,
			adc_span_millivolts_);
	return signal_min_millivolts_ + signal_delta_millivolts;
}

void Inputs::calculate_conversion_parameters() {
	const int32_t adc_low_mv = static_cast<int32_t>(kAudioCvInVoltageAtMinus5V * 1000.0f + 0.5f);
	const int32_t adc_high_mv = static_cast<int32_t>(kAudioCvInVoltageAtPlus5V * 1000.0f + 0.5f);
	const int32_t signal_min_mv = static_cast<int32_t>(kAudioCvInMinVoltage * 1000.0f);
	const int32_t signal_max_mv = static_cast<int32_t>(kAudioCvInMaxVoltage * 1000.0f);

	adc_low_millivolts_ = adc_low_mv;
	adc_span_millivolts_ = adc_high_mv - adc_low_mv;
	if (adc_span_millivolts_ <= 0) {
		adc_span_millivolts_ = 1;
	}
	signal_min_millivolts_ = signal_min_mv;
	signal_span_millivolts_ = signal_max_mv - signal_min_mv;
}

void Inputs::release_audio_cv_subscriptions() {
	if (adc_token_a_ != 0) {
		AdcEngine::instance().unregister(adc_token_a_);
		adc_token_a_ = 0;
	}
	if (adc_token_b_ != 0) {
		AdcEngine::instance().unregister(adc_token_b_);
		adc_token_b_ = 0;
	}
}

void Inputs::gpio_irq_handler(uint gpio, uint32_t events) {
	(void)events;
	if (gpio < NUM_BANK0_GPIOS && pulse_irq_instances[gpio] != nullptr) {
		bool raw_state = gpio_get(gpio);
		pulse_irq_instances[gpio]->handle_edge(raw_state);
	}
}

void Inputs::handle_edge(bool raw_state) {
	(void)raw_state;
	pulse_last_change_time_us_ = time_us_32();
}
