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

// Stream selector for both ADC-in and DAC-out under the dual-stream API.
// Mirrors the A/B convention used by `AudioCvInChannel` / `AudioCvOutChannel`.
enum AudioStream : uint8_t {
	kAudioStreamA = 0,
	kAudioStreamB = 1,
};
constexpr uint8_t kMaxAudioStreams = 2;

// Per-tick I/O for the dual-stream callback. The user reads `in[]` and fills
// `out[]`. Independent processing is just `out[A] = f(in[A]); out[B] = g(in[B]);`
// — but cross-mix (e.g. mid/side, ping-pong delay) is also natural because
// the callback sees both inputs at once.
struct DualStreamSamples {
	int16_t in[kMaxAudioStreams];
	int16_t out[kMaxAudioStreams];
};

// Dual-stream callback signature. Called once per audio tick with both inputs.
using ProcessDualStreamFn = void (*)(
	DualStreamSamples* samples,
	const AudioProcessorFrame* frame,
	void* user_ctx);

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
	 * @brief Starts timer-driven single-stream audio processing.
	 *
	 * Subscribes to ADC channel A only, calls the callback once per tick with the
	 * single input sample, and writes the returned sample to DAC channel A. Channel
	 * B is left untouched and incurs no per-tick cost.
	 *
	 * @return `BrainInitStatus::kOk` on success, `kAlreadyInitialized` if already running,
	 * or `kFailed` for invalid config/callback or hardware/timer init failure.
	 */
	BrainInitStatus init(
		const AudioProcessorConfig& config,
		ProcessSampleFn process_sample_fn,
		void* user_ctx = nullptr);

	/**
	 * @brief Starts timer-driven dual-stream audio processing.
	 *
	 * Subscribes to ADC channels A and B, calls the callback once per tick with both
	 * inputs, and writes both DAC channels each tick. The two SPI writes per tick
	 * roughly double DAC bus time vs single-stream — the default `spi_baud_hz` of
	 * 1 MHz is too low for dual-stream at the default 23 µs sample period; raise it
	 * to at least 4 MHz (8 MHz recommended). See AUDIO_PROCESSOR.md for the
	 * per-tick performance budget.
	 *
	 * @return `BrainInitStatus::kOk` on success, `kAlreadyInitialized` if already running,
	 * or `kFailed` for invalid config/callback or hardware/timer init failure.
	 */
	BrainInitStatus init(
		const AudioProcessorConfig& config,
		ProcessDualStreamFn process_dual_fn,
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
	static constexpr uint8_t kMcp4822ChannelA = 0;
	static constexpr uint8_t kMcp4822ChannelB = 1;

	static bool timer_callback(repeating_timer_t* timer);
	void process_tick();
	void fill_pot_frame(AudioProcessorFrame& frame) const;
	bool init_spi_dac();
	void deinit_spi_dac();
	int16_t adc_raw_to_audio_sample(uint16_t raw) const;
	uint16_t audio_sample_to_dac_value(int16_t sample) const;
	void write_dac_channel(uint8_t channel, uint16_t dac_value);
	// Legacy thin wrapper kept for source compatibility.
	void write_dac_channel_a(uint16_t dac_value);

	AudioProcessorConfig config_{};
	ProcessSampleFn process_sample_fn_ = nullptr;
	ProcessDualStreamFn process_dual_fn_ = nullptr;
	void* user_ctx_ = nullptr;
	::Pots* pots_ = nullptr;

	bool initialized_ = false;
	bool timer_running_ = false;
	bool spi_initialized_ = false;
	bool dual_stream_mode_ = false;
	repeating_timer_t timer_{};

	spi_inst_t* spi_instance_ = spi0;
	uint cs_pin_ = kAudioCvOutCsPin;
	uint sck_pin_ = kAudioCvOutSckPin;
	uint tx_pin_ = kAudioCvOutTxPin;
	uint coupling_pin_a_ = kAudioCvOutCouplingAPin;
	uint coupling_pin_b_ = kAudioCvOutCouplingBPin;

	uint8_t audio_adc_channel_ = 0;
	uint8_t audio_adc_channel_b_ = 0;

	// AudioProcessor drives `Pots`' state machine inline from `process_tick`,
	// counting audio ticks and invoking `pots_->external_tick()` once per
	// `pots_tick_audio_interval_` audio ticks (~1 ms wall-clock at default
	// sample_period_us). Pots' own alarm-pool timer is canceled while audio
	// is running so the two never compete in the same IRQ batch — that
	// competition was the source of the audible 1 kHz "pop noise" on test 10.
	uint16_t pots_tick_counter_ = 0;
	uint16_t pots_tick_audio_interval_ = 0;

	volatile uint64_t tick_count_ = 0;
	volatile uint32_t overrun_count_ = 0;
};

}  // namespace brain::utils

using AudioProcessor = brain::utils::AudioProcessor;
using AudioProcessorConfig = brain::utils::AudioProcessorConfig;
using AudioProcessorFrame = brain::utils::AudioProcessorFrame;
using AudioProcessorStats = brain::utils::AudioProcessorStats;
using ProcessSampleFn = brain::utils::ProcessSampleFn;
using ProcessDualStreamFn = brain::utils::ProcessDualStreamFn;
using DualStreamSamples = brain::utils::DualStreamSamples;
using brain::utils::AudioStream;
using brain::utils::kAudioStreamA;
using brain::utils::kAudioStreamB;
using brain::utils::kMaxAudioStreams;

#endif
