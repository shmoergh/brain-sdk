#include "inputs.h"

#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/timer.h>
#include <pico/stdlib.h>
#include <pico/time.h>

#include "adc-arbiter.h"

namespace {

static Inputs* pulse_irq_instances[NUM_BANK0_GPIOS] = {nullptr};

}  // namespace

Inputs::Inputs(uint pulse_in_gpio)
	: pulse_in_gpio_(pulse_in_gpio) {}

Inputs::~Inputs() {
	release_audio_cv_dma();
}

bool Inputs::init_audio_cv() {
	{
		BrainAdcLockGuard guard;
		adc_init();
		adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_A);
		adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_B);
	}

	if (audio_cv_dma_enabled_) {
		bool dma_ok = init_audio_cv_dma();
		if (dma_ok) {
			sample_audio_cv_dma();
		} else {
			audio_cv_dma_enabled_ = false;
		}
	} else {
		release_audio_cv_dma();
	}

	calculate_conversion_parameters();
	if (!audio_cv_dma_enabled_) {
		update_audio_cv();
	}
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
	if (audio_cv_dma_enabled_) {
		if (!audio_cv_dma_active_) {
			if (!init_audio_cv_dma()) {
				audio_cv_dma_enabled_ = false;
			}
		}
		if (audio_cv_dma_active_) {
			sample_audio_cv_dma();
			return;
		}
	}

	{
		BrainAdcLockGuard guard;
		adc_select_input(1);
		channel_raw_[AudioCvInChannel::kChannelA] = adc_read();

		adc_select_input(2);
		channel_raw_[AudioCvInChannel::kChannelB] = adc_read();
	}
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

void Inputs::set_audio_cv_dma_enabled(bool enabled) {
	audio_cv_dma_enabled_ = enabled;
	if (!enabled) {
		release_audio_cv_dma();
	}
}

bool Inputs::is_audio_cv_dma_enabled() const {
	return audio_cv_dma_enabled_;
}

bool Inputs::is_audio_cv_dma_active() const {
	return audio_cv_dma_active_;
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

bool Inputs::init_audio_cv_dma() {
	BrainAdcLockGuard guard;
	if (audio_cv_dma_active_) {
		return true;
	}

	const uint8_t adc_channel_a = GPIO_BRAIN_AUDIO_CV_IN_A - 26;
	const uint8_t adc_channel_b = GPIO_BRAIN_AUDIO_CV_IN_B - 26;

	adc_fifo_setup(
		true,	// enable
		true,	// DMA request
		1,		// DREQ asserted when at least 1 sample is present
		false,	// no ERR bit
		false); // 12-bit samples
	adc_set_clkdiv(0.0f);
	adc_set_round_robin((1u << adc_channel_a) | (1u << adc_channel_b));
	adc_select_input(adc_channel_a);

	int dma_channel = dma_claim_unused_channel(false);
	if (dma_channel < 0) {
		return false;
	}

	dma_channel_config cfg = dma_channel_get_default_config(dma_channel);
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
	channel_config_set_read_increment(&cfg, false);
	channel_config_set_write_increment(&cfg, true);
	channel_config_set_dreq(&cfg, DREQ_ADC);
	dma_channel_configure(
		dma_channel,
		&cfg,
		audio_cv_dma_samples_,
		&adc_hw->fifo,
		2,
		false);

	audio_cv_dma_channel_ = dma_channel;
	audio_cv_dma_active_ = true;
	return true;
}

void Inputs::release_audio_cv_dma() {
	BrainAdcLockGuard guard;
	if (!audio_cv_dma_active_) {
		return;
	}

	adc_run(false);
	adc_set_round_robin(0);
	if (audio_cv_dma_channel_ >= 0) {
		dma_channel_abort(audio_cv_dma_channel_);
		dma_channel_unclaim(audio_cv_dma_channel_);
	}
	audio_cv_dma_channel_ = -1;
	audio_cv_dma_active_ = false;
}

void Inputs::sample_audio_cv_dma() {
	BrainAdcLockGuard guard;
	if (!audio_cv_dma_active_ || audio_cv_dma_channel_ < 0) {
		return;
	}

	const uint8_t adc_channel_a = GPIO_BRAIN_AUDIO_CV_IN_A - 26;
	const uint8_t adc_channel_b = GPIO_BRAIN_AUDIO_CV_IN_B - 26;

	adc_run(false);
	adc_fifo_drain();
	adc_set_round_robin((1u << adc_channel_a) | (1u << adc_channel_b));
	adc_select_input(adc_channel_a);
	dma_channel_set_read_addr(audio_cv_dma_channel_, &adc_hw->fifo, false);
	dma_channel_set_write_addr(audio_cv_dma_channel_, audio_cv_dma_samples_, false);
	dma_channel_set_trans_count(audio_cv_dma_channel_, 2, true);
	adc_run(true);
	dma_channel_wait_for_finish_blocking(audio_cv_dma_channel_);
	adc_run(false);

	channel_raw_[AudioCvInChannel::kChannelA] = audio_cv_dma_samples_[0];
	channel_raw_[AudioCvInChannel::kChannelB] = audio_cv_dma_samples_[1];
}
