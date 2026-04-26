#ifndef BRAIN_AUDIO_PROCESSOR_H_
#define BRAIN_AUDIO_PROCESSOR_H_

#include <hardware/spi.h>
#include <pico/time.h>

#include <cstdint>

#include "constants.h"
#include "init-status.h"

class Pots;

namespace brain::utils {

struct AudioProcessorFrame {
	static constexpr uint8_t kMaxPots = 4;
	static constexpr uint16_t kFlagOverrun = (1u << 0);

	uint64_t tick = 0;
	uint16_t flags = 0;
	uint8_t pot_count = 0;
	uint8_t reserved = 0;
	uint8_t pot_raw_u8[kMaxPots] = {0};
};

struct AudioProcessorStats {
	uint64_t tick_count = 0;
	uint32_t overrun_count = 0;
	uint32_t pot_mux_switch_count = 0;	// legacy: always 0 under AdcEngine
	uint32_t pot_settle_discard_count = 0;	// legacy: always 0 under AdcEngine
};

struct AudioProcessorConfig {
	uint32_t sample_period_us = 23;
	bool enable_pot_mux = true;	 // legacy: pot reading is owned by Pots, not AudioProcessor
	uint8_t pot_count = 3;	// legacy: pot count is owned by Pots
	uint8_t pot_settle_discard_samples = 2;	 // legacy: ignored
	uint8_t pot_average_samples = 4;  // legacy: ignored
	uint16_t max_dma_drain_samples_per_tick = 64;  // legacy: ignored
	uint32_t spi_baud_hz = 1000000;
};

using ProcessSampleFn = int16_t (*)(int16_t input_sample, const AudioProcessorFrame* frame, void* user_ctx);

class AudioProcessor {
public:
	AudioProcessor() = default;
	~AudioProcessor();
	AudioProcessor(const AudioProcessor&) = delete;
	AudioProcessor& operator=(const AudioProcessor&) = delete;
	AudioProcessor(AudioProcessor&&) = delete;
	AudioProcessor& operator=(AudioProcessor&&) = delete;

	/**
	 * @brief Wires up an optional `Pots` instance so `get_pot_raw_u8(...)` keeps working.
	 *
	 * Callers that don't use `get_pot_raw_u8(...)` may skip this. Must be called before `init(...)`
	 * (or any time before the first call to `get_pot_raw_u8(...)`).
	 */
	void set_pots(::Pots* pots);

	/**
	 * @brief Starts timer-driven audio processing.
	 *
	 * The audio input ADC channel is registered with the global `AdcEngine` and sampled
	 * concurrently with any pot/CV consumers — there is no longer any need to coordinate
	 * with `Pots` or `Inputs`.
	 *
	 * @return `BrainInitStatus::kOk` on success, `kAlreadyInitialized` if already running,
	 * or `kFailed` for invalid config/callback or hardware/timer init failure.
	 */
	BrainInitStatus init(
		const AudioProcessorConfig& config,
		ProcessSampleFn process_sample_fn,
		void* user_ctx = nullptr);

	void stop();
	bool is_initialized() const;
	AudioProcessorStats get_stats() const;

	/**
	 * @brief Legacy API: returns the latest pot value from the wired `Pots` instance, mapped to 8 bits.
	 *
	 * Returns `0` if no `Pots` instance was wired via `set_pots(...)`. Apps written against the
	 * unified API should call `pots.get(i)` directly.
	 */
	uint16_t get_pot_raw_u8(uint8_t index) const;

private:
	static constexpr uint16_t kDacMaxValue = 4095;

	static bool timer_callback(repeating_timer_t* timer);
	void process_tick();
	bool init_spi_dac();
	void deinit_spi_dac();
	int16_t adc_raw_to_audio_sample(uint16_t raw) const;
	uint16_t audio_sample_to_dac_value(int16_t sample) const;
	void write_dac_channel_a(uint16_t dac_value);

	AudioProcessorConfig config_{};
	ProcessSampleFn process_sample_fn_ = nullptr;
	void* user_ctx_ = nullptr;
	::Pots* pots_ = nullptr;

	bool initialized_ = false;
	bool timer_running_ = false;
	bool spi_initialized_ = false;
	repeating_timer_t timer_{};

	spi_inst_t* spi_instance_ = spi0;
	uint cs_pin_ = kAudioCvOutCsPin;
	uint sck_pin_ = kAudioCvOutSckPin;
	uint tx_pin_ = kAudioCvOutTxPin;
	uint coupling_pin_a_ = kAudioCvOutCouplingAPin;

	uint8_t audio_adc_channel_ = 0;
	uint32_t audio_adc_token_ = 0;

	volatile uint64_t tick_count_ = 0;
	volatile uint32_t overrun_count_ = 0;
};

}  // namespace brain::utils

using AudioProcessor = brain::utils::AudioProcessor;
using AudioProcessorConfig = brain::utils::AudioProcessorConfig;
using AudioProcessorFrame = brain::utils::AudioProcessorFrame;
using AudioProcessorStats = brain::utils::AudioProcessorStats;
using ProcessSampleFn = brain::utils::ProcessSampleFn;

#endif
