#include "audio-processor.h"

#include <hardware/gpio.h>
#include <hardware/sync.h>
#include <hardware/timer.h>
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

AudioProcessor* g_alarm_owners[NUM_ALARMS] = {nullptr};

}  // namespace

namespace brain::utils {

AudioProcessor::~AudioProcessor() {
	stop();
}

void AudioProcessor::set_pots(::Pots* pots) {
	pots_ = pots;
	// If audio is already running, take Pots out of the alarm pool now —
	// audio's process_tick will drive the Pots state machine inline. If
	// audio isn't running yet, init() will do this when it starts up.
	if (pots_ != nullptr && initialized_) {
		pots_->cancel_internal_timer();
	}
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
	in_a_hist_seeded_ = false;

	if (!init_spi_dac()) {
		stop();
		return BrainInitStatus::kFailed;
	}

	audio_adc_channel_ = static_cast<uint8_t>(GPIO_BRAIN_AUDIO_CV_IN_A - 26);
	audio_adc_channel_b_ = static_cast<uint8_t>(GPIO_BRAIN_AUDIO_CV_IN_B - 26);
	AdcEngine::instance().enable_channel(audio_adc_channel_);
	// Keep a second ADC channel active in single-stream mode as a stability
	// workaround for discontinuity bursts seen under high-rate single-channel
	// free-run capture on some RP-series setups. Channel B is sampled but not
	// used for single-stream output.
	AdcEngine::instance().enable_channel(audio_adc_channel_b_);
	AdcEngine::instance().set_background_drain_enabled(false);

	const uint32_t target_per_channel_hz = kMicrosPerSecond / config_.sample_period_us;
	AdcEngine::instance().set_min_per_channel_rate_hz(target_per_channel_hz);

	// Drive Pots' state machine inline at ~1 ms intervals (so cross-bleed
	// settle and averaging timing stay the same as Pots' standalone tick).
	pots_tick_audio_interval_ = static_cast<uint16_t>(1000u / config_.sample_period_us);
	if (pots_tick_audio_interval_ == 0) pots_tick_audio_interval_ = 1;
	pots_tick_counter_ = 0;

	initialized_ = true;
	if (!start_tick_scheduler()) {
		fprintf(stderr, "AudioProcessor: failed to start sample timer\n");
		stop();
		return BrainInitStatus::kFailed;
	}

	// Audio's tick is now firing — take Pots out of the alarm pool.
	if (pots_ != nullptr) pots_->cancel_internal_timer();

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
	in_a_hist_seeded_ = false;

	if (!init_spi_dac()) {
		stop();
		return BrainInitStatus::kFailed;
	}

	audio_adc_channel_ = static_cast<uint8_t>(GPIO_BRAIN_AUDIO_CV_IN_A - 26);
	audio_adc_channel_b_ = static_cast<uint8_t>(GPIO_BRAIN_AUDIO_CV_IN_B - 26);
	AdcEngine::instance().enable_channel(audio_adc_channel_);
	AdcEngine::instance().enable_channel(audio_adc_channel_b_);
	AdcEngine::instance().set_background_drain_enabled(false);

	const uint32_t target_per_channel_hz = kMicrosPerSecond / config_.sample_period_us;
	AdcEngine::instance().set_min_per_channel_rate_hz(target_per_channel_hz);

	pots_tick_audio_interval_ = static_cast<uint16_t>(1000u / config_.sample_period_us);
	if (pots_tick_audio_interval_ == 0) pots_tick_audio_interval_ = 1;
	pots_tick_counter_ = 0;

	initialized_ = true;
	if (!start_tick_scheduler()) {
		fprintf(stderr, "AudioProcessor: failed to start dual-stream sample timer\n");
		stop();
		return BrainInitStatus::kFailed;
	}

	if (pots_ != nullptr) pots_->cancel_internal_timer();

	return BrainInitStatus::kOk;
}

void AudioProcessor::stop() {
	if (!initialized_ && !timer_running_ && !spi_initialized_) return;

	stop_tick_scheduler();

	// AdcEngine channels stay enabled across stop() — disabling them would
	// thrash other consumers (Pots, Inputs). The audio cache simply goes
	// stale, which is fine because nothing is reading it anymore.
	AdcEngine::instance().set_background_drain_enabled(true);

	deinit_spi_dac();
	initialized_ = false;
	dual_stream_mode_ = false;
	process_sample_fn_ = nullptr;
	process_dual_fn_ = nullptr;
	user_ctx_ = nullptr;
	in_a_hist_seeded_ = false;
}

bool AudioProcessor::is_initialized() const {
	return initialized_;
}

AudioProcessorStats AudioProcessor::get_stats() const {
	AudioProcessorStats stats{};
	const uint32_t irq_state = save_and_disable_interrupts();
	stats.tick_count = tick_count_;
	stats.overrun_count = overrun_count_;
	stats.late_tick_count = late_tick_count_;
	stats.max_tick_interval_us = max_tick_interval_us_;
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

void AudioProcessor::hardware_alarm_callback(uint alarm_num) {
	if (alarm_num >= NUM_ALARMS) return;
	AudioProcessor* self = g_alarm_owners[alarm_num];
	if (self == nullptr || !self->timer_running_) return;

	self->process_tick();
	if (self->timer_running_) {
		self->schedule_next_hardware_alarm();
	}
}

bool AudioProcessor::start_tick_scheduler() {
	timer_running_ = true;
	using_hardware_alarm_ = false;
	hardware_alarm_num_ = -1;

	// Use the alarm-pool repeating timer for now. The one-shot hardware-alarm
	// scheduler path can silently halt on some RP2350 runs under sustained
	// audio load (observed as frozen `tick_count` / pot updates after a few
	// seconds), while repeating timer remains stable.

	if (!add_repeating_timer_us(
			-static_cast<int64_t>(config_.sample_period_us),
			&AudioProcessor::timer_callback,
			this,
			&timer_)) {
		timer_running_ = false;
		return false;
	}

	return true;
}

void AudioProcessor::stop_tick_scheduler() {
	timer_running_ = false;

	if (using_hardware_alarm_ && hardware_alarm_num_ >= 0 && hardware_alarm_num_ < NUM_ALARMS) {
		hardware_alarm_cancel(static_cast<uint>(hardware_alarm_num_));
		hardware_alarm_set_callback(static_cast<uint>(hardware_alarm_num_), nullptr);
		g_alarm_owners[hardware_alarm_num_] = nullptr;
		hardware_alarm_unclaim(static_cast<uint>(hardware_alarm_num_));
		hardware_alarm_num_ = -1;
		using_hardware_alarm_ = false;
		return;
	}

	cancel_repeating_timer(&timer_);
}

void AudioProcessor::schedule_next_hardware_alarm() {
	if (!using_hardware_alarm_ || hardware_alarm_num_ < 0 || hardware_alarm_num_ >= NUM_ALARMS) return;

	do {
		next_alarm_deadline_ = delayed_by_us(next_alarm_deadline_, config_.sample_period_us);
	} while (absolute_time_diff_us(get_absolute_time(), next_alarm_deadline_) <= 0);

	hardware_alarm_set_target(static_cast<uint>(hardware_alarm_num_), next_alarm_deadline_);
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

	// Tick-interval diagnostics: how long since the previous tick fired?
	// Nominal value is `sample_period_us` (~23 µs). Anything noticeably
	// larger means the audio IRQ was delayed by something else (another
	// IRQ handler, a critical section, etc.). Both counters are updated
	// inside the audio IRQ so no extra synchronization needed.
	if (to_us_since_boot(last_tick_start_us_) != 0) {
		const int64_t since_last = absolute_time_diff_us(last_tick_start_us_, tick_start);
		if (since_last > 50) ++late_tick_count_;
		if (since_last > 0 && static_cast<uint32_t>(since_last) > max_tick_interval_us_) {
			max_tick_interval_us_ = static_cast<uint32_t>(since_last);
		}
	}
	last_tick_start_us_ = tick_start;

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
		int16_t filtered_input = input_sample;
		if (config_.input_deglitch_enabled) {
			// Single-sample deglitcher for isolated ADC impulses. Median-of-3
			// adds one-sample latency and suppresses click/pop artifacts with
			// minimal impact on normal waveform transients.
			if (!in_a_hist_seeded_) {
				in_a_hist0_ = input_sample;
				in_a_hist1_ = input_sample;
				in_a_hist_seeded_ = true;
			}
			filtered_input = median3_i16(in_a_hist0_, in_a_hist1_, input_sample);
			in_a_hist0_ = in_a_hist1_;
			in_a_hist1_ = input_sample;
		}

		int16_t output_sample = filtered_input;
		if (process_sample_fn_ != nullptr) {
			output_sample = process_sample_fn_(filtered_input, &frame, user_ctx_);
		}
		write_dac_channel(kMcp4822ChannelA, audio_sample_to_dac_value(output_sample));
	}

	++tick_count_;

	// Drive Pots' state machine inline, after the audio DAC update is done.
	// This avoids any alarm-pool collision between Pots' tick and audio's
	// tick that would otherwise delay the audio DAC write at ~1 kHz.
	if (pots_ != nullptr && pots_tick_audio_interval_ > 0) {
		if (++pots_tick_counter_ >= pots_tick_audio_interval_) {
			pots_tick_counter_ = 0;
			pots_->external_tick();
		}
	}

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

int16_t AudioProcessor::median3_i16(int16_t a, int16_t b, int16_t c) const {
	if (a > b) {
		const int16_t t = a;
		a = b;
		b = t;
	}
	if (b > c) {
		const int16_t t = b;
		b = c;
		c = t;
	}
	if (a > b) {
		const int16_t t = a;
		a = b;
		b = t;
	}
	return b;
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
