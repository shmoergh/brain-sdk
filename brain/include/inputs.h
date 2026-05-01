#pragma once

#include <cstdint>
#include <functional>

#include "common.h"
#include "gpio-setup.h"
#include "pico/types.h"

enum AudioCvInChannel { kChannelA = 0, kChannelB = 1 };

// Convenience aliases for clearer call sites when both input/output channels are used.
constexpr AudioCvInChannel kInputsChannelA = kChannelA;
constexpr AudioCvInChannel kInputsChannelB = kChannelB;

class Inputs {
public:
	/**
	 * @brief Creates an `Inputs` instance for Brain audio/CV ADC channels and pulse input handling.
	 * @param pulse_in_gpio GPIO pin used for digital pulse input (expected active-low with pull-up).
	 */
	explicit Inputs(uint pulse_in_gpio = GPIO_BRAIN_PULSE_INPUT);

	/**
	 * @brief Releases resources owned by `Inputs`.
	 */
	~Inputs();

	/**
	 * @brief Copy construction is disabled for this type.
	 */
	Inputs(const Inputs&) = delete;

	/**
	 * @brief Copy assignment is disabled for this type.
	 */
	Inputs& operator=(const Inputs&) = delete;

	/**
	 * @brief Move construction is disabled for this type.
	 */
	Inputs(Inputs&&) = delete;

	/**
	 * @brief Move assignment is disabled for this type.
	 */
	Inputs& operator=(Inputs&&) = delete;

	/**
	 * @brief Initializes CV/audio ADC channels (A and B) and prepares voltage conversion parameters.
	 *
	 * If DMA sampling is enabled, this also tries to allocate and configure a DMA channel for ADC reads.
	 * On DMA allocation failure, `Inputs` transparently falls back to direct ADC reads.
	 *
	 * @return Always returns `true` (the function degrades gracefully to non-DMA mode on DMA failure).
	 */
	bool init_audio_cv();

	/**
	 * @brief Initializes the pulse input GPIO, enables pull-up, and resets pulse edge state.
	 * @return `true` when pulse input state is ready for polling/interrupt usage.
	 */
	bool init_pulse();

	/**
	 * @brief Deinitializes pulse input handling and detaches IRQ routing for this instance.
	 */
	void pulse_end();

	/**
	 * @brief Convenience initializer for both pulse input and audio/CV input paths.
	 * @return `true` when both `init_pulse()` and `init_audio_cv()` succeed.
	 */
	bool init();

	/**
	 * @brief Refreshes latest ADC samples for input channels A and B.
	 *
	 * Uses DMA path when enabled and active; otherwise performs direct ADC reads.
	 */
	void update_audio_cv();

	/**
	 * @brief Polls pulse GPIO, applies optional glitch filter, and fires rise/fall callbacks on debounced edges.
	 */
	void pulse_poll();

	/**
	 * @brief Performs one full input update tick (`update_audio_cv()` + `pulse_poll()`).
	 */
	void update();

	/**
	 * @brief Legacy API: requested DMA-backed CV/audio sampling. Now a no-op.
	 *
	 * Under the unified `AdcEngine`, audio CV channels are always DMA-sampled.
	 * This setter is retained for source compatibility and has no runtime effect.
	 */
	void set_audio_cv_dma_enabled(bool enabled);

	/**
	 * @brief Legacy API: always returns `true` under the unified `AdcEngine`.
	 */
	bool is_audio_cv_dma_enabled() const;

	/**
	 * @brief Legacy API: returns whether `AdcEngine` is sampling the audio CV channels.
	 */
	bool is_audio_cv_dma_active() const;

	/**
	 * @brief Returns the latest raw ADC reading for the selected input channel.
	 * @param channel Input channel selector:
	 * - `kInputsChannelA`
	 * - `kInputsChannelB`
	 * @return Latest 12-bit ADC sample for that channel (0..4095), or `0` if channel is invalid.
	 */
	uint16_t get_raw(int channel) const;

	/**
	 * @brief Returns the latest raw ADC sample for input channel A.
	 * @return Latest raw ADC reading for input channel A.
	 */
	uint16_t get_raw_channel_a() const;

	/**
	 * @brief Returns the latest raw ADC sample for input channel B.
	 * @return Latest raw ADC reading for input channel B.
	 */
	uint16_t get_raw_channel_b() const;

	/**
	 * @brief Returns the converted input voltage for the selected channel, in millivolts.
	 * @param channel Input channel selector (`kInputsChannelA` or `kInputsChannelB`).
	 * @return Converted signal voltage in mV using Brain hardware scaling constants, or `0` for invalid channel.
	 */
	int32_t get_voltage_millivolts(int channel) const;

	/**
	 * @brief Returns converted signal voltage for input channel A.
	 * @return Latest converted input voltage for channel A, in millivolts.
	 */
	int32_t get_voltage_millivolts_channel_a() const;

	/**
	 * @brief Returns converted signal voltage for input channel B.
	 * @return Latest converted input voltage for channel B, in millivolts.
	 */
	int32_t get_voltage_millivolts_channel_b() const;

	/**
	 * @brief Reads logical pulse state using active-low interpretation.
	 * @return `true` when the pulse input is logically HIGH (GPIO is low because signal is active-low).
	 */
	bool pulse_read() const;

	/**
	 * @brief Reads raw GPIO level from the pulse input pin without inversion.
	 * @return `true` when physical GPIO level is high (inactive for active-low wiring).
	 */
	bool pulse_read_raw() const;

	/**
	 * @brief Registers callback for logical pulse rising edge.
	 * @param cb Function called when pulse transitions from logical LOW to logical HIGH after filtering.
	 */
	void pulse_on_rise(std::function<void()> cb);

	/**
	 * @brief Registers callback for logical pulse falling edge.
	 * @param cb Function called when pulse transitions from logical HIGH to logical LOW after filtering.
	 */
	void pulse_on_fall(std::function<void()> cb);

	/**
	 * @brief Sets pulse glitch filter time.
	 * @param us Minimum stable edge time in microseconds before a pulse level change is accepted.
	 * `0` disables the glitch filter.
	 */
	void pulse_set_input_glitch_filter_us(uint32_t us);

	/**
	 * @brief Enables GPIO edge interrupts for the pulse pin.
	 *
	 * This sets up rise/fall IRQ events and uses the class IRQ handler to timestamp edges.
	 */
	void pulse_enable_interrupts();

	/**
	 * @brief Disables GPIO edge interrupts for the pulse pin.
	 */
	void pulse_disable_interrupts();

private:
	int32_t adc_to_millivolts(uint16_t adc_value) const;
	void calculate_conversion_parameters();

	static void gpio_irq_handler(uint gpio, uint32_t events);
	void handle_edge(bool raw_state);

	int32_t adc_low_millivolts_ = 0;
	int32_t adc_span_millivolts_ = 1;
	int32_t signal_min_millivolts_ = 0;
	int32_t signal_span_millivolts_ = 0;
	bool audio_cv_enabled_ = false;

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
