#ifndef BRAIN_AUDIO_PROCESSOR_H_
#define BRAIN_AUDIO_PROCESSOR_H_

#include <hardware/spi.h>
#include <pico/time.h>

#include <cstdint>

#include "constants.h"
#include "init-status.h"

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
	uint32_t pot_mux_switch_count = 0;
	uint32_t pot_settle_discard_count = 0;
};

struct AudioProcessorConfig {
	uint32_t sample_period_us = 23;
	bool enable_pot_mux = true;
	uint8_t pot_count = 3;
	uint8_t pot_settle_discard_samples = 2;
	uint8_t pot_average_samples = 4;
	uint16_t max_dma_drain_samples_per_tick = 64;
	uint32_t spi_baud_hz = 1000000;
};

using ProcessSampleFn = int16_t (*)(int16_t input_sample, const AudioProcessorFrame* frame, void* user_ctx);

class AudioProcessor {
public:
	/**
	 * @brief Constructs a `AudioProcessor` instance and prepares default runtime state.
	 */
	AudioProcessor() = default;

	/**
	 * @brief Releases resources owned by `AudioProcessor`.
	 */
	~AudioProcessor();

	/**
	 * @brief Copy construction is disabled for this type.
	 */
	AudioProcessor(const AudioProcessor&) = delete;

	/**
	 * @brief Copy assignment is disabled for this type.
	 */
	AudioProcessor& operator=(const AudioProcessor&) = delete;

	/**
	 * @brief Move construction is disabled for this type.
	 */
	AudioProcessor(AudioProcessor&&) = delete;

	/**
	 * @brief Move assignment is disabled for this type.
	 */
	AudioProcessor& operator=(AudioProcessor&&) = delete;

	/**
	 * @brief Starts timer-driven audio processing with ADC DMA input and DAC output on channel A.
	 * @param config Runtime configuration:
	 * - `sample_period_us`: tick period for processing callback.
	 * - `enable_pot_mux`: include pot-mux ADC channel in the DMA stream.
	 * - `pot_count`: number of pot channels to cycle (clamped to `AudioProcessorFrame::kMaxPots`).
	 * - `pot_settle_discard_samples`: samples discarded after each mux switch.
	 * - `pot_average_samples`: samples averaged per pot update.
	 * - `max_dma_drain_samples_per_tick`: backlog limit per processing tick.
	 * - `spi_baud_hz`: DAC SPI clock rate.
	 * @param process_sample_fn Audio callback called each tick with input sample and frame metadata.
	 * Returning value becomes output sample written to DAC.
	 * @param user_ctx Opaque pointer passed through to each `process_sample_fn` call.
	 * @return `BrainInitStatus::kOk` on success, `BrainInitStatus::kAlreadyInitialized` if already running,
	 * or `BrainInitStatus::kFailed` for invalid config/callback or hardware/timer init failure.
	 */
	BrainInitStatus init(
		const AudioProcessorConfig& config,
		ProcessSampleFn process_sample_fn,
		void* user_ctx = nullptr);

	/**
	 * @brief Stops timer, DMA, and DAC activity and releases acquired runtime resources.
	 */
	void stop();

	/**
	 * @brief Reports whether the audio processor is currently initialized and running.
	 * @return `true` after successful `init(...)` until `stop()` is called.
	 */
	bool is_initialized() const;

	/**
	 * @brief Returns a thread-safe snapshot of runtime counters.
	 * @return `AudioProcessorStats` containing tick count, overruns, pot mux switches, and settle discards.
	 */
	AudioProcessorStats get_stats() const;

	/**
	 * @brief Reads the latest 8-bit pot value sampled by the audio processor path.
	 * @param index Pot index in range `0..AudioProcessorFrame::kMaxPots-1`.
	 * @return Latest pot value in 0..255 for valid index, or `0` when index is out of range.
	 */
	uint16_t get_pot_raw_u8(uint8_t index) const;

private:
	static constexpr uint16_t kDacMaxValue = 4095;
	static constexpr uint16_t kDmaRingSamples = 256;
	static constexpr uint16_t kDmaRingMask = kDmaRingSamples - 1;
	static constexpr uint16_t kDmaRingBytes = kDmaRingSamples * sizeof(uint16_t);
	static constexpr uint8_t kDmaRingBits = 9;
	static_assert((kDmaRingSamples & (kDmaRingSamples - 1)) == 0, "kDmaRingSamples must be power-of-two");
	static_assert((1u << kDmaRingBits) == kDmaRingBytes, "kDmaRingBits must match ring byte size");

	static bool timer_callback(repeating_timer_t* timer);
	void process_tick();
	bool init_spi_dac();
	bool init_adc_dma();
	void deinit_spi_dac();
	void deinit_adc_dma();
	void set_pot_mux_channel(uint8_t channel);
	void process_pot_sample(uint16_t raw);
	void drain_dma_ring(bool* tick_overrun);
	void reset_runtime_state();
	uint16_t read_dma_write_index() const;
	int16_t adc_raw_to_audio_sample(uint16_t raw) const;
	uint16_t audio_sample_to_dac_value(int16_t sample) const;
	void write_dac_channel_a(uint16_t dac_value);

	AudioProcessorConfig config_{};
	ProcessSampleFn process_sample_fn_ = nullptr;
	void* user_ctx_ = nullptr;

	bool initialized_ = false;
	bool timer_running_ = false;
	bool adc_includes_pot_channel_ = false;
	bool spi_initialized_ = false;
	bool adc_dma_initialized_ = false;
	repeating_timer_t timer_{};

	spi_inst_t* spi_instance_ = spi0;
	uint cs_pin_ = kAudioCvOutCsPin;
	uint sck_pin_ = kAudioCvOutSckPin;
	uint tx_pin_ = kAudioCvOutTxPin;
	uint coupling_pin_a_ = kAudioCvOutCouplingAPin;

	int dma_channel_ = -1;
	alignas(512) uint16_t dma_ring_[kDmaRingSamples] = {0};
	uint16_t dma_read_index_ = 0;
	bool next_dma_sample_is_audio_ = true;
	volatile uint16_t latest_audio_raw_ = 2048;

	uint8_t active_pot_index_ = 0;
	uint8_t pot_discard_remaining_ = 0;
	uint8_t pot_samples_collected_ = 0;
	uint32_t pot_accumulator_ = 0;
	volatile uint8_t pot_raw_u8_[AudioProcessorFrame::kMaxPots] = {0};

	volatile uint64_t tick_count_ = 0;
	volatile uint32_t overrun_count_ = 0;
	volatile uint32_t pot_mux_switch_count_ = 0;
	volatile uint32_t pot_settle_discard_count_ = 0;
};

}  // namespace brain::utils

using AudioProcessor = brain::utils::AudioProcessor;
using AudioProcessorConfig = brain::utils::AudioProcessorConfig;
using AudioProcessorFrame = brain::utils::AudioProcessorFrame;
using AudioProcessorStats = brain::utils::AudioProcessorStats;
using ProcessSampleFn = brain::utils::ProcessSampleFn;

#endif
