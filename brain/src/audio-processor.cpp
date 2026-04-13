#include "audio-processor.h"

#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>

#include <cstdio>

#include "adc-arbiter.h"

namespace {

uint16_t clamp_u16(int32_t value, uint16_t min_value, uint16_t max_value) {
	if (value < static_cast<int32_t>(min_value)) return min_value;
	if (value > static_cast<int32_t>(max_value)) return max_value;
	return static_cast<uint16_t>(value);
}

}  // namespace

namespace brain::utils {

AudioProcessor::~AudioProcessor() {
	stop();
}

BrainInitStatus AudioProcessor::init(
	const AudioProcessorConfig& config,
	ProcessSampleFn process_sample_fn,
	void* user_ctx) {
	if (initialized_) return BrainInitStatus::kAlreadyInitialized;
	if (process_sample_fn == nullptr) {
		fprintf(stderr, "AudioProcessor: process callback is required\n");
		return BrainInitStatus::kFailed;
	}
	if (config.sample_period_us == 0) {
		fprintf(stderr, "AudioProcessor: sample_period_us must be > 0\n");
		return BrainInitStatus::kFailed;
	}

	config_ = config;
	if (config_.pot_count > AudioProcessorFrame::kMaxPots) {
		config_.pot_count = AudioProcessorFrame::kMaxPots;
	}
	if (config_.pot_count == 0) {
		config_.enable_pot_mux = false;
	}
	if (config_.pot_average_samples == 0) {
		config_.pot_average_samples = 1;
	}
	if (config_.max_dma_drain_samples_per_tick == 0) {
		config_.max_dma_drain_samples_per_tick = 1;
	}
	if (config_.spi_baud_hz == 0) {
		config_.spi_baud_hz = 1000000;
	}

	process_sample_fn_ = process_sample_fn;
	user_ctx_ = user_ctx;
	reset_runtime_state();

	if (!init_spi_dac()) {
		stop();
		return BrainInitStatus::kFailed;
	}
	if (!init_adc_dma()) {
		stop();
		return BrainInitStatus::kFailed;
	}

	initialized_ = true;
	timer_running_ = true;
	if (!add_repeating_timer_us(
			-static_cast<int64_t>(config_.sample_period_us),
			&AudioProcessor::timer_callback,
			this,
			&timer_)) {
		fprintf(stderr, "AudioProcessor: failed to start sample timer\n");
		stop();
		return BrainInitStatus::kFailed;
	}

	return BrainInitStatus::kOk;
}

void AudioProcessor::stop() {
	if (!initialized_ && !timer_running_ && !spi_initialized_ && !adc_dma_initialized_) {
		return;
	}

	timer_running_ = false;
	cancel_repeating_timer(&timer_);
	deinit_adc_dma();
	deinit_spi_dac();
	initialized_ = false;
	process_sample_fn_ = nullptr;
	user_ctx_ = nullptr;
}

bool AudioProcessor::is_initialized() const {
	return initialized_;
}

AudioProcessorStats AudioProcessor::get_stats() const {
	AudioProcessorStats stats{};

	const uint32_t irq_state = save_and_disable_interrupts();
	stats.tick_count = tick_count_;
	stats.overrun_count = overrun_count_;
	stats.pot_mux_switch_count = pot_mux_switch_count_;
	stats.pot_settle_discard_count = pot_settle_discard_count_;
	restore_interrupts(irq_state);

	return stats;
}

uint16_t AudioProcessor::get_pot_raw_u8(uint8_t index) const {
	if (index >= AudioProcessorFrame::kMaxPots) return 0;

	const uint32_t irq_state = save_and_disable_interrupts();
	const uint16_t value = pot_raw_u8_[index];
	restore_interrupts(irq_state);

	return value;
}

bool AudioProcessor::timer_callback(repeating_timer_t* timer) {
	AudioProcessor* self = static_cast<AudioProcessor*>(timer->user_data);
	if (self == nullptr || !self->timer_running_) {
		return false;
	}

	self->process_tick();
	return self->timer_running_;
}

void AudioProcessor::process_tick() {
	const absolute_time_t tick_start = get_absolute_time();
	bool tick_overrun = false;

	drain_dma_ring(&tick_overrun);

	AudioProcessorFrame frame{};
	frame.tick = tick_count_ + 1;
	if (config_.enable_pot_mux) {
		frame.pot_count = config_.pot_count;
	}
	for (uint8_t i = 0; i < AudioProcessorFrame::kMaxPots; ++i) {
		frame.pot_raw_u8[i] = pot_raw_u8_[i];
	}
	if (tick_overrun) {
		frame.flags |= AudioProcessorFrame::kFlagOverrun;
	}

	const int16_t input_sample = adc_raw_to_audio_sample(latest_audio_raw_);
	int16_t output_sample = input_sample;
	if (process_sample_fn_ != nullptr) {
		output_sample = process_sample_fn_(input_sample, &frame, user_ctx_);
	}

	write_dac_channel_a(audio_sample_to_dac_value(output_sample));
	++tick_count_;

	const int64_t elapsed_us = absolute_time_diff_us(tick_start, get_absolute_time());
	if (elapsed_us > static_cast<int64_t>(config_.sample_period_us)) {
		tick_overrun = true;
	}
	if (tick_overrun) {
		++overrun_count_;
	}
}

bool AudioProcessor::init_spi_dac() {
	spi_init(spi_instance_, config_.spi_baud_hz);
	spi_set_format(spi_instance_, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

	gpio_set_function(sck_pin_, GPIO_FUNC_SPI);
	gpio_set_function(tx_pin_, GPIO_FUNC_SPI);

	gpio_init(cs_pin_);
	gpio_set_dir(cs_pin_, GPIO_OUT);
	gpio_put(cs_pin_, 1);

	gpio_init(coupling_pin_a_);
	gpio_set_dir(coupling_pin_a_, GPIO_OUT);
	// AudioProcessor writes signed samples around 0, use -5..+5V range.
	gpio_put(coupling_pin_a_, true);
	spi_initialized_ = true;

	return true;
}

bool AudioProcessor::init_adc_dma() {
	BrainAdcLockGuard guard;

	adc_includes_pot_channel_ = config_.enable_pot_mux;

	const uint8_t audio_adc_channel = GPIO_BRAIN_AUDIO_CV_IN_A - 26;
	const uint8_t pot_adc_channel = GPIO_BRAIN_POTMUX_ADC - 26;
	const uint32_t channels_per_round = adc_includes_pot_channel_ ? 2u : 1u;

	adc_init();
	adc_gpio_init(GPIO_BRAIN_AUDIO_CV_IN_A);
	if (adc_includes_pot_channel_) {
		adc_gpio_init(GPIO_BRAIN_POTMUX_ADC);
		gpio_init(GPIO_BRAIN_POTMUX_S0);
		gpio_set_dir(GPIO_BRAIN_POTMUX_S0, GPIO_OUT);
		gpio_put(GPIO_BRAIN_POTMUX_S0, 0);
		gpio_init(GPIO_BRAIN_POTMUX_S1);
		gpio_set_dir(GPIO_BRAIN_POTMUX_S1, GPIO_OUT);
		gpio_put(GPIO_BRAIN_POTMUX_S1, 0);
		set_pot_mux_channel(active_pot_index_);
	}

	const float target_adc_sample_rate_hz =
		(static_cast<float>(kMicrosPerSecond) * static_cast<float>(channels_per_round)) /
		static_cast<float>(config_.sample_period_us);
	float clkdiv = 0.0f;
	if (target_adc_sample_rate_hz > 0.0f) {
		clkdiv = (48000000.0f / target_adc_sample_rate_hz) - 1.0f;
		if (clkdiv < 0.0f) {
			clkdiv = 0.0f;
		}
	}

	adc_fifo_setup(
		true,	// enable FIFO
		true,	// enable DMA request
		1,		// request whenever >=1 sample present
		false,	// ignore ERR bit
		false); // 12-bit samples
	adc_set_clkdiv(clkdiv);

	uint32_t round_robin_mask = (1u << audio_adc_channel);
	if (adc_includes_pot_channel_) {
		round_robin_mask |= (1u << pot_adc_channel);
	}
	adc_set_round_robin(round_robin_mask);
	adc_select_input(audio_adc_channel);

	dma_channel_ = dma_claim_unused_channel(false);
	if (dma_channel_ < 0) {
		fprintf(stderr, "AudioProcessor: no DMA channel available\n");
		return false;
	}

	dma_channel_config cfg = dma_channel_get_default_config(dma_channel_);
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
	channel_config_set_read_increment(&cfg, false);
	channel_config_set_write_increment(&cfg, true);
	channel_config_set_dreq(&cfg, DREQ_ADC);
	channel_config_set_ring(&cfg, true, kDmaRingBits);

	dma_channel_configure(
		dma_channel_,
		&cfg,
		dma_ring_,
		&adc_hw->fifo,
		0xffffffffu,
		true);
	adc_run(true);
	adc_dma_initialized_ = true;
	return true;
}

void AudioProcessor::deinit_spi_dac() {
	if (!spi_initialized_) {
		return;
	}
	gpio_put(cs_pin_, 1);
	spi_initialized_ = false;
}

void AudioProcessor::deinit_adc_dma() {
	if (!adc_dma_initialized_) {
		return;
	}

	BrainAdcLockGuard guard;
	adc_run(false);
	adc_set_round_robin(0);
	adc_fifo_drain();

	if (dma_channel_ >= 0) {
		dma_channel_abort(dma_channel_);
		dma_channel_unclaim(dma_channel_);
		dma_channel_ = -1;
	}
	adc_dma_initialized_ = false;
}

void AudioProcessor::set_pot_mux_channel(uint8_t channel) {
	channel &= 0x03;
	gpio_put(GPIO_BRAIN_POTMUX_S0, channel & 0x01);
	gpio_put(GPIO_BRAIN_POTMUX_S1, (channel >> 1) & 0x01);
}

void AudioProcessor::process_pot_sample(uint16_t raw) {
	if (!config_.enable_pot_mux || config_.pot_count == 0) {
		return;
	}

	if (pot_discard_remaining_ > 0) {
		--pot_discard_remaining_;
		++pot_settle_discard_count_;
		return;
	}

	pot_accumulator_ += raw;
	++pot_samples_collected_;
	if (pot_samples_collected_ < config_.pot_average_samples) {
		return;
	}

	const uint16_t averaged_raw = static_cast<uint16_t>(pot_accumulator_ / pot_samples_collected_);
	const uint8_t mapped_u8 = static_cast<uint8_t>((averaged_raw * 255u + (kAdcMaxValue / 2)) / kAdcMaxValue);
	pot_raw_u8_[active_pot_index_] = mapped_u8;

	pot_accumulator_ = 0;
	pot_samples_collected_ = 0;

	if (config_.pot_count > 1) {
		active_pot_index_ = static_cast<uint8_t>((active_pot_index_ + 1) % config_.pot_count);
		set_pot_mux_channel(active_pot_index_);
		pot_discard_remaining_ = config_.pot_settle_discard_samples;
		++pot_mux_switch_count_;
	}
}

void AudioProcessor::drain_dma_ring(bool* tick_overrun) {
	if (dma_channel_ < 0) {
		return;
	}

	uint16_t available = (read_dma_write_index() - dma_read_index_) & kDmaRingMask;
	if (available == 0) {
		return;
	}

	uint16_t max_drain = config_.max_dma_drain_samples_per_tick;
	if (max_drain == 0) {
		max_drain = 1;
	}

	if (available > max_drain) {
		const uint16_t drop_count = static_cast<uint16_t>(available - max_drain);
		dma_read_index_ = static_cast<uint16_t>((dma_read_index_ + drop_count) & kDmaRingMask);
		if (adc_includes_pot_channel_ && (drop_count & 1u) != 0u) {
			next_dma_sample_is_audio_ = !next_dma_sample_is_audio_;
		}
		available = max_drain;
		if (tick_overrun != nullptr) {
			*tick_overrun = true;
		}
	}

	for (uint16_t i = 0; i < available; ++i) {
		const uint16_t raw = dma_ring_[dma_read_index_] & kAdcMaxValue;
		dma_read_index_ = static_cast<uint16_t>((dma_read_index_ + 1) & kDmaRingMask);

		if (!adc_includes_pot_channel_ || next_dma_sample_is_audio_) {
			latest_audio_raw_ = raw;
		} else {
			process_pot_sample(raw);
		}

		if (adc_includes_pot_channel_) {
			next_dma_sample_is_audio_ = !next_dma_sample_is_audio_;
		}
	}
}

void AudioProcessor::reset_runtime_state() {
	const uint32_t irq_state = save_and_disable_interrupts();

	tick_count_ = 0;
	overrun_count_ = 0;
	pot_mux_switch_count_ = 0;
	pot_settle_discard_count_ = 0;
	latest_audio_raw_ = 2048;

	for (uint8_t i = 0; i < AudioProcessorFrame::kMaxPots; ++i) {
		pot_raw_u8_[i] = 0;
	}

	dma_read_index_ = 0;
	next_dma_sample_is_audio_ = true;
	active_pot_index_ = 0;
	pot_discard_remaining_ = config_.pot_settle_discard_samples;
	pot_samples_collected_ = 0;
	pot_accumulator_ = 0;

	restore_interrupts(irq_state);
}

uint16_t AudioProcessor::read_dma_write_index() const {
	if (dma_channel_ < 0) return dma_read_index_;

	const uintptr_t base = reinterpret_cast<uintptr_t>(dma_ring_);
	const uintptr_t write_addr = dma_hw->ch[dma_channel_].write_addr;
	const uintptr_t byte_delta = (write_addr - base) & (kDmaRingBytes - 1u);
	return static_cast<uint16_t>(byte_delta / sizeof(uint16_t));
}

int16_t AudioProcessor::adc_raw_to_audio_sample(uint16_t raw) const {
	const int32_t centered = static_cast<int32_t>(raw & kAdcMaxValue) - 2048;
	return static_cast<int16_t>(centered << 4);
}

uint16_t AudioProcessor::audio_sample_to_dac_value(int16_t sample) const {
	const int32_t unipolar = static_cast<int32_t>(sample) + 32768;
	const uint16_t clamped = clamp_u16(unipolar, 0, 65535);
	return static_cast<uint16_t>(clamped >> 4);
}

void AudioProcessor::write_dac_channel_a(uint16_t dac_value) {
	const uint16_t clamped = clamp_u16(dac_value, 0, kDacMaxValue);
	const uint8_t config = (0u << 3) | (0u << 2) | (0u << 1) | 1u;

	uint8_t data[2];
	data[0] = static_cast<uint8_t>((config << 4) | ((clamped >> 8) & 0x0F));
	data[1] = static_cast<uint8_t>(clamped & 0xFF);

	asm volatile("nop \n nop \n nop");
	gpio_put(cs_pin_, 0);
	asm volatile("nop \n nop \n nop");

	spi_write_blocking(spi_instance_, data, 2);

	asm volatile("nop \n nop \n nop");
	gpio_put(cs_pin_, 1);
	asm volatile("nop \n nop \n nop");
}

}  // namespace brain::utils
