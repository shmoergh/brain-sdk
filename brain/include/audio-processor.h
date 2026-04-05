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
	AudioProcessor() = default;
	~AudioProcessor();
	AudioProcessor(const AudioProcessor&) = delete;
	AudioProcessor& operator=(const AudioProcessor&) = delete;
	AudioProcessor(AudioProcessor&&) = delete;
	AudioProcessor& operator=(AudioProcessor&&) = delete;

	BrainInitStatus init(
		const AudioProcessorConfig& config,
		ProcessSampleFn process_sample_fn,
		void* user_ctx = nullptr);
	void stop();
	bool is_initialized() const;
	AudioProcessorStats get_stats() const;
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
	uint16_t dma_ring_[kDmaRingSamples] = {0};
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
