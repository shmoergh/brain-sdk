#ifndef BRAIN_AUDIO_PROCESSOR_H_
#define BRAIN_AUDIO_PROCESSOR_H_

#include <cstdint>

#include "constants.h"
#include "init-status.h"

namespace brain::internal {
struct AdcSnapshot;
}

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

/**
 * @brief V2 stereo (dual-input, dual-output) frame metadata.
 *
 * Same shape as `AudioProcessorFrame` but explicitly named for the v2 API to
 * keep callsite types unambiguous.
 */
struct AudioProcessorFrameV2 {
	static constexpr uint8_t kMaxPots = AudioProcessorFrame::kMaxPots;
	static constexpr uint16_t kFlagOverrun = AudioProcessorFrame::kFlagOverrun;

	uint64_t tick = 0;
	uint16_t flags = 0;
	uint8_t pot_count = 0;
	uint8_t reserved = 0;
	uint8_t pot_raw_u8[kMaxPots] = {0};
};

/**
 * @brief V2 dual-channel runtime configuration.
 *
 * Selected by calling `AudioProcessor::init_v2(...)` instead of `init(...)`.
 * `sample_rate_hz` replaces `sample_period_us` — internally we derive the
 * period as `1'000'000 / sample_rate_hz`. `claim_channel_a` / `claim_channel_b`
 * select which output channels the DSP drives; unclaimed channels remain in
 * Manual mode and can still be written via `Outputs::set_voltage_*`.
 */
struct AudioProcessorConfigV2 {
	uint32_t sample_rate_hz = 43'500;  // ≈ 23 µs sample period
	bool enable_pot_mux = true;
	uint8_t pot_count = 3;
	uint8_t pot_settle_discard_samples = 2;
	uint8_t pot_average_samples = 4;
	bool claim_channel_a = true;
	bool claim_channel_b = true;
};

/**
 * @brief V2 dual-input / dual-output DSP callback.
 *
 * Receives the freshly sampled IN1 and IN2 values, writes its computed
 * output samples into `*out_a` (channel A) and `*out_b` (channel B). Either
 * output is ignored if the corresponding `claim_channel_*` flag was false
 * at init time. Runs in ADC IRQ context at audio rate.
 */
using ProcessFrameFnV2 = void (*)(int16_t in1, int16_t in2,
                                   const AudioProcessorFrameV2* frame,
                                   int16_t* out_a, int16_t* out_b,
                                   void* user_ctx);

/**
 * @brief Audio-rate DSP harness running atop the shared `AdcEngine` and `OutputEngine`.
 *
 * AudioProcessor does not own hardware itself. On `init()` it starts the
 * shared engines (idempotent), switches `AdcEngine` into audio mode at the
 * requested `sample_period_us`, claims `OutputEngine` channel A as `kAudio`,
 * and registers an audio callback. Each ADC IRQ at sample rate (~43 kHz at
 * the default 23 µs) calls the user's `ProcessSampleFn` with the freshly
 * sampled IN1 value and the latest pot/frame metadata, then writes the
 * returned sample into the OutputEngine's streaming buffer for DAC output.
 *
 * Coexistence: this class is friendly with `Inputs`, `Pots`, `PotMultiFunction`,
 * and `Outputs` instances — they all share the same engines. Any combination
 * of components can be initialized in any order.
 *
 * Output range: claiming a channel for audio forces it into the bipolar
 * (-5..+5 V) range by driving its coupling pin high — `AudioProcessor` writes
 * signed samples around 0. **`stop()` does not restore the prior range.** If
 * your firmware previously set the channel to unipolar via
 * `Outputs::set_output_range(...)`, you must call `set_output_range(...)`
 * again after `stop()` to put the channel back into unipolar mode.
 */
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

	AudioProcessor(const AudioProcessor&) = delete;
	AudioProcessor& operator=(const AudioProcessor&) = delete;
	AudioProcessor(AudioProcessor&&) = delete;
	AudioProcessor& operator=(AudioProcessor&&) = delete;

	/**
	 * @brief Starts audio-rate DSP processing on the shared engines.
	 *
	 * @param config Runtime configuration:
	 * - `sample_period_us`: audio sample period (also drives DAC frame rate and ADC IRQ rate).
	 * - `enable_pot_mux`: include pot scanning in the shared `AdcEngine`.
	 * - `pot_count`: number of pot channels to cycle (clamped to `AudioProcessorFrame::kMaxPots`).
	 * - `pot_settle_discard_samples`: pot samples discarded after each mux switch.
	 * - `pot_average_samples`: pot samples averaged per logical pot update.
	 * - `max_dma_drain_samples_per_tick`: ignored (no internal ring); preserved for source compatibility.
	 * - `spi_baud_hz`: ignored (`OutputEngine` owns SPI at 20 MHz); preserved for source compatibility.
	 * @param process_sample_fn Per-sample DSP callback called from the ADC IRQ at audio rate.
	 * Returning value is converted to a 12-bit DAC sample and pushed into channel A's audio ring.
	 * @param user_ctx Opaque pointer passed through to each `process_sample_fn` call.
	 * @return `BrainInitStatus::kOk` on success, `BrainInitStatus::kAlreadyInitialized` if already running,
	 * or `BrainInitStatus::kFailed` for invalid config/callback or engine start failure.
	 */
	BrainInitStatus init(
		const AudioProcessorConfig& config,
		ProcessSampleFn process_sample_fn,
		void* user_ctx = nullptr);

	/**
	 * @brief Starts audio-rate DSP with the v2 dual-input / dual-output callback.
	 *
	 * Same engine setup as `init(...)` but uses `AudioProcessorConfigV2` and
	 * `ProcessFrameFnV2`. Channels A and B are claimed independently per the
	 * config's `claim_channel_*` flags; the user callback receives both IN1 and
	 * IN2 and produces both OUT A and OUT B per audio sample.
	 *
	 * @param config V2 runtime configuration.
	 * @param process_frame_fn V2 dual-channel DSP callback.
	 * @param user_ctx Opaque pointer passed through to each callback invocation.
	 * @return `BrainInitStatus::kOk` on success, `BrainInitStatus::kAlreadyInitialized` if already running,
	 * or `BrainInitStatus::kFailed` for invalid config/callback or engine start failure.
	 */
	BrainInitStatus init_v2(
		const AudioProcessorConfigV2& config,
		ProcessFrameFnV2 process_frame_fn,
		void* user_ctx = nullptr);

	/**
	 * @brief Stops audio-rate processing.
	 *
	 * Clears the audio callback, releases claimed channels back to manual
	 * ownership, and disables audio mode on the `AdcEngine` (engine reverts
	 * to CV mode for any other consumers). Does not stop the engines
	 * themselves, and does not restore output ranges — claimed channels stay
	 * in bipolar mode until you call `Outputs::set_output_range(...)`.
	 */
	void stop();

	/**
	 * @brief Reports whether the audio processor is currently initialized and running.
	 */
	bool is_initialized() const;

	/**
	 * @brief Returns a snapshot of runtime counters.
	 */
	AudioProcessorStats get_stats() const;

	/**
	 * @brief Reads the latest 8-bit pot value derived from the shared `AdcEngine` snapshot.
	 * @param index Pot index in range `0..AudioProcessorFrame::kMaxPots-1`.
	 * @return Latest pot value mapped to 0..255 for valid index, or `0` when index is out of range.
	 */
	uint16_t get_pot_raw_u8(uint8_t index) const;

private:
	struct EngineSetup {
		uint32_t sample_period_us;
		bool enable_pot_mux;
		uint8_t pot_count;
		uint8_t pot_settle_discard_samples;
		uint8_t pot_average_samples;
		bool claim_channel_a;
		bool claim_channel_b;
	};

	BrainInitStatus start_engines(const EngineSetup& s);

	static void on_adc_sample_static(uint16_t in1_raw, uint16_t in2_raw,
	                                  const brain::internal::AdcSnapshot& snap,
	                                  void* ctx);
	void on_adc_sample(uint16_t in1_raw, uint16_t in2_raw,
	                   const brain::internal::AdcSnapshot& snap);

	static int16_t adc_raw_to_audio_sample(uint16_t raw);
	static uint16_t audio_sample_to_dac_value(int16_t sample);

	AudioProcessorConfig config_{};
	ProcessSampleFn process_sample_fn_ = nullptr;
	ProcessFrameFnV2 process_frame_fn_v2_ = nullptr;
	void* user_ctx_ = nullptr;

	bool initialized_ = false;
	bool audio_mode_started_ = false;
	bool channel_a_claimed_ = false;
	bool channel_b_claimed_ = false;
	bool mode_v2_ = false;
	uint8_t active_pot_count_ = 0;

	uint64_t tick_count_ = 0;
	uint32_t initial_underrun_a_ = 0;  // baseline subtracted from OutputEngine underrun for stats.
};

}  // namespace brain::utils

using AudioProcessor = brain::utils::AudioProcessor;
using AudioProcessorConfig = brain::utils::AudioProcessorConfig;
using AudioProcessorConfigV2 = brain::utils::AudioProcessorConfigV2;
using AudioProcessorFrame = brain::utils::AudioProcessorFrame;
using AudioProcessorFrameV2 = brain::utils::AudioProcessorFrameV2;
using AudioProcessorStats = brain::utils::AudioProcessorStats;
using ProcessSampleFn = brain::utils::ProcessSampleFn;
using ProcessFrameFnV2 = brain::utils::ProcessFrameFnV2;

#endif
