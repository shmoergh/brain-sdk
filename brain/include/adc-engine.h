#pragma once

#include <cstdint>
#include <functional>

#include <pico/time.h>

/**
 * @brief Singleton owner of the RP2040 ADC + DMA + round-robin.
 *
 * Components that need ADC samples (`Pots`, `AudioProcessor`, `Inputs`)
 * subscribe to one or more ADC input channels via `register_channel(...)` and
 * receive raw 12-bit samples via callback. They never touch ADC/DMA registers
 * themselves. This eliminates the cross-component "must not mix" rules that
 * existed when each component owned its own ADC configuration.
 *
 * The engine runs an internal repeating timer that drains the DMA ring,
 * demuxes samples by round-robin order, dispatches each sample to its
 * subscribers, and updates a per-channel `latest` cache for polling-style
 * consumers (`get_latest(channel)`).
 *
 * Initialization is implicit on first `register_channel(...)` call. Apps
 * never need to call any setup method.
 */
class AdcEngine {
public:
	using SampleCallback = std::function<void(uint16_t raw)>;

	static constexpr uint8_t kMaxAdcChannels = 4;
	static constexpr uint8_t kMaxSubscribersPerChannel = 4;

	struct Stats {
		uint64_t drain_count = 0;
		uint32_t overrun_count = 0;
		uint32_t reconfigure_count = 0;
	};

	/**
	 * @brief Returns the global engine instance, constructing it on first call.
	 */
	static AdcEngine& instance();

	/**
	 * @brief Subscribes to samples for an ADC input channel.
	 * @param adc_channel ADC input index (0..3 on RP2040).
	 * @param on_sample Callback invoked from drain timer IRQ context for each sample.
	 * @return Non-zero token used with `unregister(token)`. Returns 0 on failure
	 * (invalid channel or subscriber list full).
	 */
	uint32_t register_channel(uint8_t adc_channel, SampleCallback on_sample);

	/**
	 * @brief Unregisters a previously registered subscriber.
	 * @param token Token returned by `register_channel(...)`. No-op for `0` or unknown tokens.
	 */
	void unregister(uint32_t token);

	/**
	 * @brief Raises the floor for the per-channel ADC sample rate.
	 * @param hz Per-channel samples-per-second floor. AdcEngine multiplies by the
	 * number of active channels in round-robin and recomputes ADC clkdiv to meet
	 * the resulting aggregate rate. Only ever raises; never lowers automatically.
	 *
	 * Called by AudioProcessor at init so each channel still hits audio rate even
	 * when other components register additional channels.
	 */
	void set_min_sample_rate_hz(uint32_t hz);

	/**
	 * @brief Returns the latest cached raw ADC sample for the given channel.
	 * @param adc_channel ADC input index (0..3).
	 * @return Latest 12-bit sample (0..4095), or `0` if invalid channel or no sample yet.
	 *
	 * The cache is updated by the internal drain timer and by `drain_now()`. Consumers
	 * that need sample-accurate freshness (e.g. AudioProcessor) should call
	 * `drain_now()` immediately before `get_latest(...)`.
	 */
	uint16_t get_latest(uint8_t adc_channel) const;

	/**
	 * @brief Drains the DMA ring synchronously and updates the per-channel `latest`
	 * cache before returning.
	 *
	 * Cheap (typically a handful of samples) and safe to call from any context,
	 * including IRQ context. AudioProcessor calls this on every audio tick so
	 * `get_latest(audio_channel)` reflects the most recent ADC sample.
	 */
	void drain_now();

	/**
	 * @brief Returns the number of in-flight ADC samples for the given channel.
	 *
	 * "In-flight" means: samples already captured by the ADC but not yet
	 * dispatched to subscribers — i.e. samples sitting in the DMA ring buffer
	 * (between read and write pointers) plus a conservative estimate for the
	 * RP2040/RP2350 ADC FIFO depth and any in-progress sample-and-hold.
	 *
	 * Used by `Pots` after a mux GPIO flip to know how many subsequent samples
	 * still reflect the *previous* mux state, so they can be discarded before
	 * accumulating averages for the new pot.
	 *
	 * IMPORTANT: only safe to call from inside a `SampleCallback` — the engine's
	 * internal lock is already held by the dispatcher in that context. Calling
	 * from anywhere else will return a value but is not guaranteed correct under
	 * concurrent reconfiguration.
	 */
	uint16_t pending_sample_count_for_channel_unlocked(uint8_t adc_channel) const;

	/**
	 * @brief Snapshot of internal counters (drain count, overruns, reconfigures).
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
	static constexpr uint32_t kDefaultMinSampleRateHz = 4000;
	// If a consumer (e.g. AudioProcessor) drained within this window, the
	// background timer skips its drain pass to avoid redundant lock activity
	// that competes with the audio tick at ~23 us cadence.
	static constexpr uint64_t kRecentDrainSkipUs = 1000;

	struct Subscriber {
		uint32_t token = 0;	 // 0 = empty slot
		SampleCallback callback;
	};

	void ensure_started_locked();
	void reconfigure_locked();
	void drain_ring_locked();
	void compute_clkdiv_locked();
	void rebuild_active_channels_locked();
	uint16_t read_dma_write_index() const;

	static bool drain_timer_callback(repeating_timer_t* timer);

	Subscriber subscribers_[kMaxAdcChannels][kMaxSubscribersPerChannel] = {};
	uint8_t active_channels_[kMaxAdcChannels] = {};
	uint8_t num_active_channels_ = 0;
	uint8_t next_channel_cursor_ = 0;

	volatile uint16_t latest_[kMaxAdcChannels] = {0, 0, 0, 0};

	uint32_t min_sample_rate_hz_ = kDefaultMinSampleRateHz;

	bool initialized_ = false;
	int dma_channel_ = -1;
	alignas(512) uint16_t ring_[kRingSamples] = {};
	uint16_t ring_read_index_ = 0;

	repeating_timer_t timer_{};
	bool timer_running_ = false;

	uint32_t next_token_ = 1;

	uint64_t stats_drain_count_ = 0;
	uint32_t stats_overrun_count_ = 0;
	uint32_t stats_reconfigure_count_ = 0;
	volatile uint64_t last_drain_us_ = 0;
};
