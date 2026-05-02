#pragma once

#include <cstdint>

#include <pico/time.h>

/**
 * @brief Singleton owner of the RP2040/RP2350 ADC + DMA + round-robin.
 *
 * Reads ADC channels in round-robin via DMA into a small ring buffer, then
 * drains the ring into a per-channel "latest sample" cache. Consumers
 * (`Pots`, `Inputs`, `AudioProcessor`) call `enable_channel(ch)` once at
 * init and then poll `get_latest(ch)` whenever they want the most recent
 * sample. There are no callbacks, no subscriber lists, no tokens.
 *
 * `drain_now()` is exposed for consumers that need the cache to reflect
 * the absolute latest ADC state (audio at 23 us). A background timer
 * also drains the ring at ~500 us as a backstop for pure-Pots/Inputs use
 * where no audio tick is running.
 *
 * Initialization is implicit on first `enable_channel(...)` call.
 */
class AdcEngine {
public:
	static constexpr uint8_t kMaxAdcChannels = 4;

	struct Stats {
		uint64_t drain_count = 0;
		uint32_t overrun_count = 0;
		uint32_t reconfigure_count = 0;
		uint32_t conversion_error_count = 0;
	};

	/**
	 * @brief Returns the global engine instance, constructing it on first call.
	 */
	static AdcEngine& instance();

	/**
	 * @brief Adds an ADC input channel (0..3) to the round-robin sample set.
	 * Idempotent — calling for the same channel twice is harmless. Reconfigures
	 * the ADC + DMA on first enable for that channel.
	 */
	void enable_channel(uint8_t adc_channel);

	/**
	 * @brief Raises the per-channel ADC sample-rate floor.
	 * AdcEngine multiplies by the number of active channels and recomputes
	 * the ADC clkdiv. Only ever raises — never lowers automatically.
	 */
	void set_min_per_channel_rate_hz(uint32_t hz);

	/**
	 * @brief Drains the DMA ring synchronously and updates the per-channel
	 * `latest` cache before returning. Cheap (a handful of samples) and safe
	 * to call from any context, including IRQ. AudioProcessor calls this on
	 * every audio tick.
	 */
	void drain_now();

	/**
	 * @brief Enables/disables the 500 us background drain timer.
	 *
	 * AudioProcessor drains inline every audio tick; in that mode the
	 * background timer only adds lock contention and jitter, so callers should
	 * disable it while real-time audio is active and re-enable it when audio
	 * stops.
	 */
	void set_background_drain_enabled(bool enabled);

	/**
	 * @brief Returns the latest cached raw ADC sample for the given channel.
	 * @return 0..4095 12-bit sample, or 0 if the channel has never been
	 * enabled or no sample has arrived yet.
	 */
	uint16_t get_latest(uint8_t adc_channel) const;

	/**
	 * @brief Snapshot of internal counters.
	 */
	Stats get_stats() const;

private:
	AdcEngine();
	AdcEngine(const AdcEngine&) = delete;
	AdcEngine& operator=(const AdcEngine&) = delete;

	static constexpr uint16_t kRingSamples = 256;
	static constexpr uint16_t kRingMask = kRingSamples - 1;
	static constexpr uint16_t kRingBytes = kRingSamples * sizeof(uint16_t);
	static constexpr uint8_t kRingBits = 9;
	static constexpr uint32_t kDefaultDrainPeriodUs = 500;
	static constexpr uint32_t kDefaultMinPerChannelRateHz = 4000;
	// Background drain skips its work if a consumer (e.g. AudioProcessor's
	// inline drain) drained within this window — eliminates redundant lock
	// contention with the audio tick.
	static constexpr uint64_t kRecentDrainSkipUs = 1000;

	void ensure_started_locked();
	void reconfigure_locked();
	void drain_ring_locked();
	void compute_clkdiv_locked();
	uint16_t read_dma_write_index() const;

	static bool drain_timer_callback(repeating_timer_t* timer);

	bool channel_active_[kMaxAdcChannels] = {false, false, false, false};
	uint8_t active_channels_[kMaxAdcChannels] = {};	 // ordered, indexed 0..N-1
	uint8_t num_active_channels_ = 0;
	uint8_t next_channel_cursor_ = 0;
	volatile uint16_t latest_[kMaxAdcChannels] = {0, 0, 0, 0};

	uint32_t min_per_channel_rate_hz_ = kDefaultMinPerChannelRateHz;

	bool initialized_ = false;
	int dma_channel_ = -1;
	alignas(512) uint16_t ring_[kRingSamples] = {};
	uint16_t ring_read_index_ = 0;

	repeating_timer_t timer_{};
	bool timer_running_ = false;
	bool background_drain_enabled_ = true;

	uint64_t stats_drain_count_ = 0;
	uint32_t stats_overrun_count_ = 0;
	uint32_t stats_reconfigure_count_ = 0;
	uint32_t stats_conversion_error_count_ = 0;
	volatile uint32_t last_drain_us_ = 0;
};
