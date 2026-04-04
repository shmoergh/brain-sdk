#include "inputs.h"

#include <hardware/adc.h>
#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <pico/time.h>

namespace {

static Inputs* pulse_irq_instances[NUM_BANK0_GPIOS] = {nullptr};

}  // namespace

Inputs::Inputs(uint pulse_in_gpio)
	: pulse_in_gpio_(pulse_in_gpio) {}

bool Inputs::init_audio_cv() {
	adc_init();
	adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_A);
	adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_B);

	calculate_conversion_parameters();
	update_audio_cv();
	return true;
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
	adc_select_input(1);
	channel_raw_[AudioCvInChannel::kChannelA] = adc_read();

	adc_select_input(2);
	channel_raw_[AudioCvInChannel::kChannelB] = adc_read();
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

float Inputs::get_voltage(int channel) const {
	if (channel == AudioCvInChannel::kChannelA || channel == AudioCvInChannel::kChannelB) {
		return adc_to_voltage(channel_raw_[channel]);
	}
	return 0.0f;
}

float Inputs::get_voltage_channel_a() const {
	return adc_to_voltage(channel_raw_[AudioCvInChannel::kChannelA]);
}

float Inputs::get_voltage_channel_b() const {
	return adc_to_voltage(channel_raw_[AudioCvInChannel::kChannelB]);
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

float Inputs::adc_to_voltage(uint16_t adc_value) const {
	float adc_voltage = (static_cast<float>(adc_value) / kAdcMaxValue) * kAdcVoltageRef;
	return (adc_voltage * voltage_scale_) + voltage_offset_;
}

void Inputs::calculate_conversion_parameters() {
	float voltage_span = kAudioCvInVoltageAtPlus5V - kAudioCvInVoltageAtMinus5V;
	float signal_span = kAudioCvInMaxVoltage - kAudioCvInMinVoltage;

	voltage_scale_ = signal_span / voltage_span;
	voltage_offset_ = kAudioCvInMinVoltage - (kAudioCvInVoltageAtMinus5V * voltage_scale_);
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
