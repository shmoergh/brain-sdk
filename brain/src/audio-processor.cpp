#include "audio-processor.h"

#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>

#include <cstdio>

#include "adc-engine.h"
#include "audio-cv-out-spi-arbiter.h"
#include "common.h"
#include "pots.h"

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

void AudioProcessor::set_pots(::Pots* pots) {
	pots_ = pots;
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
	if (config_.spi_baud_hz == 0) config_.spi_baud_hz = 1000000;

	process_sample_fn_ = process_sample_fn;
	process_dual_fn_ = nullptr;
	dual_stream_mode_ = false;
	user_ctx_ = user_ctx;
	tick_count_ = 0;
	overrun_count_ = 0;

	if (!init_spi_dac()) {
		stop();
		return BrainInitStatus::kFailed;
	}

	audio_adc_channel_ = static_cast<uint8_t>(GPIO_BRAIN_AUDIO_CV_IN_A - 26);

	// Subscribe to the audio input channel; we don't use the per-sample callback
	// (process_tick polls get_latest), but registering keeps the channel in the
	// AdcEngine round-robin set.
	audio_adc_token_ = AdcEngine::instance().register_channel(
		audio_adc_channel_,
		[](uint16_t /*raw*/) {});
	if (audio_adc_token_ == 0) {
		fprintf(stderr, "AudioProcessor: failed to register audio ADC channel\n");
		stop();
		return BrainInitStatus::kFailed;
	}

	// Per-channel ADC rate must match the audio sample period. AdcEngine
	// multiplies by the number of active channels internally to compute clkdiv,
	// so this stays correct as more components register channels.
	const uint32_t target_per_channel_hz = kMicrosPerSecond / config_.sample_period_us;
	AdcEngine::instance().set_min_sample_rate_hz(target_per_channel_hz);

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

BrainInitStatus AudioProcessor::init(
	const AudioProcessorConfig& config,
	ProcessDualStreamFn process_dual_fn,
	void* user_ctx) {
	if (initialized_) return BrainInitStatus::kAlreadyInitialized;
	if (process_dual_fn == nullptr) {
		fprintf(stderr, "AudioProcessor: dual-stream callback is required\n");
		return BrainInitStatus::kFailed;
	}
	if (config.sample_period_us == 0) {
		fprintf(stderr, "AudioProcessor: sample_period_us must be > 0\n");
		return BrainInitStatus::kFailed;
	}

	config_ = config;
	if (config_.spi_baud_hz == 0) config_.spi_baud_hz = 1000000;

	process_sample_fn_ = nullptr;
	process_dual_fn_ = process_dual_fn;
	dual_stream_mode_ = true;
	user_ctx_ = user_ctx;
	tick_count_ = 0;
	overrun_count_ = 0;

	if (!init_spi_dac()) {
		stop();
		return BrainInitStatus::kFailed;
	}

	audio_adc_channel_ = static_cast<uint8_t>(GPIO_BRAIN_AUDIO_CV_IN_A - 26);
	audio_adc_channel_b_ = static_cast<uint8_t>(GPIO_BRAIN_AUDIO_CV_IN_B - 26);

	// Subscribe to both audio input channels. We don't use the per-sample
	// callback (process_tick polls get_latest), but registering keeps both
	// channels in the AdcEngine round-robin set.
	audio_adc_token_ = AdcEngine::instance().register_channel(
		audio_adc_channel_,
		[](uint16_t /*raw*/) {});
	audio_adc_token_b_ = AdcEngine::instance().register_channel(
		audio_adc_channel_b_,
		[](uint16_t /*raw*/) {});
	if (audio_adc_token_ == 0 || audio_adc_token_b_ == 0) {
		fprintf(stderr, "AudioProcessor: failed to register dual audio ADC channels\n");
		stop();
		return BrainInitStatus::kFailed;
	}

	const uint32_t target_per_channel_hz = kMicrosPerSecond / config_.sample_period_us;
	AdcEngine::instance().set_min_sample_rate_hz(target_per_channel_hz);

	initialized_ = true;
	timer_running_ = true;
	if (!add_repeating_timer_us(
			-static_cast<int64_t>(config_.sample_period_us),
			&AudioProcessor::timer_callback,
			this,
			&timer_)) {
		fprintf(stderr, "AudioProcessor: failed to start dual-stream sample timer\n");
		stop();
		return BrainInitStatus::kFailed;
	}

	return BrainInitStatus::kOk;
}

void AudioProcessor::stop() {
	if (!initialized_ && !timer_running_ && !spi_initialized_
		&& audio_adc_token_ == 0 && audio_adc_token_b_ == 0) {
		return;
	}

	timer_running_ = false;
	cancel_repeating_timer(&timer_);

	if (audio_adc_token_ != 0) {
		AdcEngine::instance().unregister(audio_adc_token_);
		audio_adc_token_ = 0;
	}
	if (audio_adc_token_b_ != 0) {
		AdcEngine::instance().unregister(audio_adc_token_b_);
		audio_adc_token_b_ = 0;
	}

	deinit_spi_dac();
	initialized_ = false;
	dual_stream_mode_ = false;
	process_sample_fn_ = nullptr;
	process_dual_fn_ = nullptr;
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
	restore_interrupts(irq_state);
	return stats;
}

uint16_t AudioProcessor::get_pot_raw_u8(uint8_t index) const {
	if (pots_ == nullptr || index >= AudioProcessorFrame::kMaxPots) return 0;
	const uint16_t raw = pots_->get_buffered(index);
	const uint16_t resolution_max = pots_->get_output_max();
	if (resolution_max == 0) return 0;
	return static_cast<uint16_t>((static_cast<uint32_t>(raw) * 255u + (resolution_max / 2)) / resolution_max);
}

bool AudioProcessor::timer_callback(repeating_timer_t* timer) {
	auto* self = static_cast<AudioProcessor*>(timer->user_data);
	if (self == nullptr || !self->timer_running_) return false;
	self->process_tick();
	return self->timer_running_;
}

void AudioProcessor::fill_pot_frame(AudioProcessorFrame& frame) const {
	if (pots_ == nullptr) return;

	const uint8_t pot_count = pots_->get_num_pots();
	frame.pot_count = (pot_count > AudioProcessorFrame::kMaxPots)
		? AudioProcessorFrame::kMaxPots : pot_count;
	for (uint8_t i = 0; i < frame.pot_count; ++i) {
		frame.pot_raw_u8[i] = static_cast<uint8_t>(get_pot_raw_u8(i));
	}
}

void AudioProcessor::process_tick() {
	const absolute_time_t tick_start = get_absolute_time();

	// Drain the ADC ring inline before reading. Without this, `get_latest()`
	// returns whatever the background drain timer last cached — typically
	// hundreds of microseconds stale, which sounds like a heavy bitcrusher
	// because the audio is effectively sampled at the drain rate, not the
	// audio rate.
	AdcEngine::instance().drain_now();

	AudioProcessorFrame frame{};
	frame.tick = tick_count_ + 1;
	fill_pot_frame(frame);

	if (dual_stream_mode_) {
		DualStreamSamples samples{};
		samples.in[kAudioStreamA] = adc_raw_to_audio_sample(
			AdcEngine::instance().get_latest(audio_adc_channel_));
		samples.in[kAudioStreamB] = adc_raw_to_audio_sample(
			AdcEngine::instance().get_latest(audio_adc_channel_b_));
		// Pass-through default if user callback decides not to overwrite.
		samples.out[kAudioStreamA] = samples.in[kAudioStreamA];
		samples.out[kAudioStreamB] = samples.in[kAudioStreamB];

		if (process_dual_fn_ != nullptr) {
			process_dual_fn_(&samples, &frame, user_ctx_);
		}

		// Two SPI writes per tick. The SPI mutex is taken inside write_dac_channel
		// so a concurrent CV write from `Outputs` can't collide with us.
		write_dac_channel(kMcp4822ChannelA, audio_sample_to_dac_value(samples.out[kAudioStreamA]));
		write_dac_channel(kMcp4822ChannelB, audio_sample_to_dac_value(samples.out[kAudioStreamB]));
	} else {
		const uint16_t raw_audio = AdcEngine::instance().get_latest(audio_adc_channel_);
		const int16_t input_sample = adc_raw_to_audio_sample(raw_audio);
		int16_t output_sample = input_sample;
		if (process_sample_fn_ != nullptr) {
			output_sample = process_sample_fn_(input_sample, &frame, user_ctx_);
		}
		write_dac_channel(kMcp4822ChannelA, audio_sample_to_dac_value(output_sample));
	}

	++tick_count_;

	const int64_t elapsed_us = absolute_time_diff_us(tick_start, get_absolute_time());
	if (elapsed_us > static_cast<int64_t>(config_.sample_period_us)) {
		++overrun_count_;
		frame.flags |= AudioProcessorFrame::kFlagOverrun;
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

	// Channel A coupling: -5..+5V range for signed audio output.
	gpio_init(coupling_pin_a_);
	gpio_set_dir(coupling_pin_a_, GPIO_OUT);
	gpio_put(coupling_pin_a_, true);

	if (dual_stream_mode_) {
		// Channel B coupling: same range as A so both streams behave identically.
		gpio_init(coupling_pin_b_);
		gpio_set_dir(coupling_pin_b_, GPIO_OUT);
		gpio_put(coupling_pin_b_, true);
	}

	spi_initialized_ = true;
	return true;
}

void AudioProcessor::deinit_spi_dac() {
	if (!spi_initialized_) return;
	gpio_put(cs_pin_, 1);
	spi_initialized_ = false;
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

void AudioProcessor::write_dac_channel(uint8_t channel, uint16_t dac_value) {
	const uint16_t clamped = clamp_u16(dac_value, 0, kDacMaxValue);
	// MCP4822 config byte (high nibble): bit 3 selects channel A (0) or B (1),
	// bit 2 unused (0), bit 1 sets gain (0 = 1x), bit 0 SHDN (1 = active).
	const uint8_t channel_bit = static_cast<uint8_t>((channel & 1u) << 3);
	const uint8_t config = channel_bit | (0u << 2) | (0u << 1) | 1u;

	uint8_t data[2];
	data[0] = static_cast<uint8_t>((config << 4) | ((clamped >> 8) & 0x0F));
	data[1] = static_cast<uint8_t>(clamped & 0xFF);

	// Mutex protects against `Outputs` simultaneously writing CV to the same
	// MCP4822 over the same SPI bus + CS line.
	BrainAudioDacSpiLockGuard guard;

	asm volatile("nop \n nop \n nop");
	gpio_put(cs_pin_, 0);
	asm volatile("nop \n nop \n nop");

	spi_write_blocking(spi_instance_, data, 2);

	asm volatile("nop \n nop \n nop");
	gpio_put(cs_pin_, 1);
	asm volatile("nop \n nop \n nop");
}

// Legacy thin wrapper kept for source compatibility — older internal callers
// or downstream code that referenced the symbol keep working.
void AudioProcessor::write_dac_channel_a(uint16_t dac_value) {
	write_dac_channel(kMcp4822ChannelA, dac_value);
}

}  // namespace brain::utils
