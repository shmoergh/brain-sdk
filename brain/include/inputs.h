#pragma once

#include <cstdint>
#include <functional>

#include "common.h"
#include "gpio-setup.h"
#include "pico/types.h"

enum AudioCvInChannel { kChannelA = 0, kChannelB = 1 };

class Inputs {
public:
	explicit Inputs(uint pulse_in_gpio = GPIO_BRAIN_PULSE_INPUT);
	~Inputs();
	Inputs(const Inputs&) = delete;
	Inputs& operator=(const Inputs&) = delete;
	Inputs(Inputs&&) = delete;
	Inputs& operator=(Inputs&&) = delete;

	bool init_audio_cv();
	bool init_pulse();
	void pulse_end();
	bool init();

	void update_audio_cv();
	void pulse_poll();
	void update();
	void set_audio_cv_dma_enabled(bool enabled);
	bool is_audio_cv_dma_enabled() const;
	bool is_audio_cv_dma_active() const;

	uint16_t get_raw(int channel) const;
	uint16_t get_raw_channel_a() const;
	uint16_t get_raw_channel_b() const;
	float get_voltage(int channel) const;
	float get_voltage_channel_a() const;
	float get_voltage_channel_b() const;

	bool pulse_read() const;
	bool pulse_read_raw() const;
	void pulse_on_rise(std::function<void()> cb);
	void pulse_on_fall(std::function<void()> cb);
	void pulse_set_input_glitch_filter_us(uint32_t us);
	void pulse_enable_interrupts();
	void pulse_disable_interrupts();

private:
	float adc_to_voltage(uint16_t adc_value) const;
	void calculate_conversion_parameters();
	bool init_audio_cv_dma();
	void release_audio_cv_dma();
	void sample_audio_cv_dma();

	static void gpio_irq_handler(uint gpio, uint32_t events);
	void handle_edge(bool raw_state);

	uint16_t channel_raw_[2] = {0, 0};
	float voltage_scale_ = 1.0f;
	float voltage_offset_ = 0.0f;
	bool audio_cv_dma_enabled_ = true;
	bool audio_cv_dma_active_ = false;
	int audio_cv_dma_channel_ = -1;
	uint16_t audio_cv_dma_samples_[2] = {0, 0};

	uint pulse_in_gpio_ = 0;
	bool pulse_initialized_ = false;
	bool pulse_last_logical_state_ = false;
	uint32_t pulse_glitch_filter_us_ = 0;
	bool pulse_interrupts_enabled_ = false;
	std::function<void()> pulse_on_rise_callback_;
	std::function<void()> pulse_on_fall_callback_;
	uint32_t pulse_last_change_time_us_ = 0;
	bool pulse_filtered_state_ = false;
};

using AudioCvIn = Inputs;
