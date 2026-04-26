#include "adc-engine.h"

#include <hardware/adc.h>
#include <hardware/dma.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>

#include "adc-arbiter.h"
#include "common.h"

namespace {

constexpr float kAdcClockHz = 48000000.0f;

// Map ADC input index (0..3) to its GPIO pin (26..29 on RP2040).
inline uint8_t adc_channel_to_gpio(uint8_t channel) {
	return static_cast<uint8_t>(26 + channel);
}

}  // namespace

AdcEngine& AdcEngine::instance() {
	static AdcEngine engine;
	return engine;
}

AdcEngine::AdcEngine() = default;

uint32_t AdcEngine::register_channel(uint8_t adc_channel, SampleCallback on_sample) {
	if (adc_channel >= kMaxAdcChannels || !on_sample) return 0;

	BrainAdcLockGuard guard;

	uint32_t token = 0;
	for (uint8_t i = 0; i < kMaxSubscribersPerChannel; ++i) {
		if (subscribers_[adc_channel][i].token == 0) {
			token = next_token_++;
			if (next_token_ == 0) next_token_ = 1;	// wrap, never hand out 0
			subscribers_[adc_channel][i].token = token;
			subscribers_[adc_channel][i].callback = std::move(on_sample);
			break;
		}
	}
	if (token == 0) return 0;  // subscriber list full

	ensure_started_locked();
	reconfigure_locked();
	return token;
}

void AdcEngine::unregister(uint32_t token) {
	if (token == 0) return;

	BrainAdcLockGuard guard;
	bool found = false;
	for (uint8_t ch = 0; ch < kMaxAdcChannels && !found; ++ch) {
		for (uint8_t i = 0; i < kMaxSubscribersPerChannel; ++i) {
			if (subscribers_[ch][i].token == token) {
				subscribers_[ch][i].token = 0;
				subscribers_[ch][i].callback = nullptr;
				found = true;
				break;
			}
		}
	}
	if (!found) return;

	reconfigure_locked();
}

void AdcEngine::set_min_sample_rate_hz(uint32_t hz) {
	BrainAdcLockGuard guard;
	if (hz <= min_sample_rate_hz_) return;
	min_sample_rate_hz_ = hz;
	if (initialized_) {
		reconfigure_locked();
	}
}

uint16_t AdcEngine::get_latest(uint8_t adc_channel) const {
	if (adc_channel >= kMaxAdcChannels) return 0;
	const uint32_t irq_state = save_and_disable_interrupts();
	const uint16_t value = latest_[adc_channel];
	restore_interrupts(irq_state);
	return value;
}

AdcEngine::Stats AdcEngine::get_stats() const {
	Stats stats{};
	const uint32_t irq_state = save_and_disable_interrupts();
	stats.drain_count = stats_drain_count_;
	stats.overrun_count = stats_overrun_count_;
	stats.reconfigure_count = stats_reconfigure_count_;
	restore_interrupts(irq_state);
	return stats;
}

void AdcEngine::ensure_started_locked() {
	if (initialized_) return;

	adc_init();
	dma_channel_ = dma_claim_unused_channel(false);
	if (dma_channel_ < 0) return;  // no DMA channel available; engine stays inert

	adc_fifo_setup(
		true,	// enable FIFO
		true,	// DMA request
		1,		// DREQ when >=1 sample present
		false,	// no ERR bit
		false); // 12-bit samples

	dma_channel_config cfg = dma_channel_get_default_config(dma_channel_);
	channel_config_set_transfer_data_size(&cfg, DMA_SIZE_16);
	channel_config_set_read_increment(&cfg, false);
	channel_config_set_write_increment(&cfg, true);
	channel_config_set_dreq(&cfg, DREQ_ADC);
	channel_config_set_ring(&cfg, true, kRingBits);

	dma_channel_configure(
		dma_channel_,
		&cfg,
		ring_,
		&adc_hw->fifo,
		0xffffffffu,
		false);  // don't start until first reconfigure_locked() runs adc_run

	ring_read_index_ = 0;
	for (uint8_t i = 0; i < kMaxAdcChannels; ++i) {
		latest_[i] = 0;
	}

	if (!timer_running_) {
		timer_running_ = add_repeating_timer_us(
			-static_cast<int64_t>(kDefaultDrainPeriodUs),
			&AdcEngine::drain_timer_callback,
			this,
			&timer_);
	}

	initialized_ = true;
}

void AdcEngine::reconfigure_locked() {
	if (!initialized_) return;

	++stats_reconfigure_count_;

	// Stop sampling, drain anything in flight under the OLD layout so we
	// don't misattribute samples after the round-robin mask changes.
	adc_run(false);
	adc_fifo_drain();
	drain_ring_locked();

	rebuild_active_channels_locked();

	if (num_active_channels_ == 0) {
		// No subscribers — leave ADC stopped. DMA channel stays claimed.
		next_channel_cursor_ = 0;
		return;
	}

	uint32_t mask = 0;
	for (uint8_t i = 0; i < num_active_channels_; ++i) {
		const uint8_t ch = active_channels_[i];
		mask |= (1u << ch);
		adc_gpio_init(adc_channel_to_gpio(ch));
	}

	adc_set_round_robin(mask);
	adc_select_input(active_channels_[0]);
	next_channel_cursor_ = 0;

	compute_clkdiv_locked();

	// Reset ring read index to the current DMA write position so we don't
	// replay samples written before the reconfig under the wrong cursor.
	ring_read_index_ = read_dma_write_index();

	adc_run(true);
}

void AdcEngine::compute_clkdiv_locked() {
	// `adc_set_clkdiv(d)` configures one sample every (1 + d) ADC clock cycles
	// when d >= 1, where the ADC clock is 48 MHz on RP2040.
	const float target_total_hz = static_cast<float>(min_sample_rate_hz_);
	float clkdiv = 0.0f;
	if (target_total_hz > 0.0f) {
		clkdiv = (kAdcClockHz / target_total_hz) - 1.0f;
		if (clkdiv < 0.0f) clkdiv = 0.0f;
	}
	adc_set_clkdiv(clkdiv);
}

void AdcEngine::rebuild_active_channels_locked() {
	num_active_channels_ = 0;
	for (uint8_t ch = 0; ch < kMaxAdcChannels; ++ch) {
		bool any = false;
		for (uint8_t i = 0; i < kMaxSubscribersPerChannel; ++i) {
			if (subscribers_[ch][i].token != 0) {
				any = true;
				break;
			}
		}
		if (any) {
			active_channels_[num_active_channels_++] = ch;
		}
	}
}

uint16_t AdcEngine::read_dma_write_index() const {
	if (dma_channel_ < 0) return ring_read_index_;
	const uintptr_t base = reinterpret_cast<uintptr_t>(ring_);
	const uintptr_t write_addr = dma_hw->ch[dma_channel_].write_addr;
	const uintptr_t byte_delta = (write_addr - base) & (kRingBytes - 1u);
	return static_cast<uint16_t>(byte_delta / sizeof(uint16_t));
}

void AdcEngine::drain_ring_locked() {
	if (dma_channel_ < 0 || num_active_channels_ == 0) return;

	const uint16_t write_idx = read_dma_write_index();
	uint16_t available = static_cast<uint16_t>((write_idx - ring_read_index_) & kRingMask);
	if (available == 0) return;

	// If the ring is more than half-full, we're falling behind the ADC.
	// Drop oldest samples to catch up; advance the demux cursor by the same
	// count modulo num_active_channels so attribution stays aligned.
	if (available > (kRingSamples / 2)) {
		const uint16_t drop = static_cast<uint16_t>(available - (kRingSamples / 2));
		ring_read_index_ = static_cast<uint16_t>((ring_read_index_ + drop) & kRingMask);
		next_channel_cursor_ = static_cast<uint8_t>((next_channel_cursor_ + drop) % num_active_channels_);
		available = kRingSamples / 2;
		++stats_overrun_count_;
	}

	for (uint16_t i = 0; i < available; ++i) {
		const uint16_t raw = static_cast<uint16_t>(ring_[ring_read_index_] & kAdcMaxValue);
		ring_read_index_ = static_cast<uint16_t>((ring_read_index_ + 1) & kRingMask);

		const uint8_t channel = active_channels_[next_channel_cursor_];
		latest_[channel] = raw;

		for (uint8_t s = 0; s < kMaxSubscribersPerChannel; ++s) {
			if (subscribers_[channel][s].token != 0 && subscribers_[channel][s].callback) {
				subscribers_[channel][s].callback(raw);
			}
		}

		next_channel_cursor_ = static_cast<uint8_t>((next_channel_cursor_ + 1) % num_active_channels_);
	}
}

bool AdcEngine::drain_timer_callback(repeating_timer_t* timer) {
	auto* self = static_cast<AdcEngine*>(timer->user_data);
	if (self == nullptr) return false;

	BrainAdcLockGuard guard;
	self->drain_ring_locked();
	++self->stats_drain_count_;
	return true;
}
