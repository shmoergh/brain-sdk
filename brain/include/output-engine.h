#pragma once

#include <hardware/spi.h>

#include <cstdint>

#include "outputs.h"

namespace brain::internal {

/**
 * @brief Per-channel ownership of a streamed DAC output.
 *
 * `kManual` (default) routes the channel's stream from a cached hold value
 * updated by `Outputs::set_voltage_*`. `kAudio` routes from a per-channel
 * sample ring updated by `OutputEngine::write_audio_sample`.
 */
enum class ChannelOwner : uint8_t {
	kManual = 0,
	kAudio = 1,
};

/**
 * @brief Stable cache of latest hold values, ownership, and engine counters.
 *
 * Reader-side copy under disabled interrupts. `hold_frame_*` is the 16-bit
 * MCP4822 frame (channel/config nibble + 12-bit data) that the DAC sees on
 * the wire when the channel is `kManual`.
 */
struct OutputEngineSnapshot {
	uint16_t hold_frame_a = 0;
	uint16_t hold_frame_b = 0;
	ChannelOwner owner_a = ChannelOwner::kManual;
	ChannelOwner owner_b = ChannelOwner::kManual;
	uint64_t total_blocks = 0;
	uint64_t total_frames = 0;
	uint32_t audio_underrun_a = 0;
	uint32_t audio_underrun_b = 0;
};

/**
 * @brief Engine config passed once to `start()`.
 *
 * Defaults match the Brain hardware: SPI0 with 16-bit Motorola frames at 20 MHz,
 * CS routed via `GPIO_FUNC_SPI` so the PL022 toggles CS between frames. Sample
 * period of 23 µs matches the legacy `AudioProcessorConfig::sample_period_us`.
 */
struct OutputEngineConfig {
	spi_inst_t* spi_instance = spi0;
	uint cs_gpio = 0;
	uint sck_gpio = 0;
	uint tx_gpio = 0;
	uint32_t spi_baud_hz = 20'000'000;
	uint32_t sample_period_us = 23;
	bool gain_2x = false;  // MCP4822 GAIN bit; matches today's behavior (gain disabled).
};

/**
 * @brief Singleton owner of the MCP4822 DAC. Phase 2 + 3 of the Brain SDK 2.1 refactor.
 *
 * Streams interleaved channel-A / channel-B 16-bit frames into the SPI peripheral
 * via three chained DMA channels paced by a hardware DMA timer:
 *
 *   render -> ctrl -> data -> render -> ...
 *
 *  - data    streams `streaming_block_` to `&spi_get_hw(spi)->dr`, paced by
 *            DREQ from the DMA pacing timer at frame rate (= 2 / sample_period_us).
 *  - ctrl    rewrites `data.read_addr` back to `&streaming_block_[0]` so streaming
 *            loops without CPU intervention.
 *  - render  copies `current_block_` -> `streaming_block_`, fires an IRQ on
 *            completion. The IRQ refills `current_block_` with the next block of
 *            (A, B) frames and resets render's read/write addresses for the
 *            next cycle.
 *
 * Phase 3 audio path: each `kAudio` channel has its own lock-free sample ring
 * filled by `write_audio_sample()`. The render IRQ pulls 16 samples per block
 * from the ring; on underrun the last consumed sample is repeated and the
 * channel's underrun counter advances by one for that block. This gives real
 * ~43 kHz audio updates when fed at sample rate, and graceful sample-and-hold
 * when fed slower.
 *
 * Manual channels are sourced from `hold_frame_*`; `set_hold_value` updates
 * the hold frame and the next render block propagates it within ~368 µs.
 *
 * Concurrency: render IRQ is the sole writer of internal counters and ring
 * tail pointers. Public accessors briefly disable interrupts and copy a small
 * struct. `write_audio_sample` advances the ring head only and is safe from
 * any IRQ priority (single-core sequential dispatch on Cortex-M).
 */
class OutputEngine {
public:
	/**
	 * @brief Public block-size constants for callers that fill audio sample arrays.
	 *
	 * `kAudioBlockSamples` is the number of stereo (A,B) pairs per render block.
	 * `kAudioRingSamples` is the per-channel ring depth (≥ 2× block size for headroom).
	 */
	static constexpr uint32_t kAudioBlockSamples = 16;
	static constexpr uint32_t kAudioRingSamples = 32;

	/**
	 * @brief Returns the singleton instance (Meyers singleton; thread-safe init).
	 */
	static OutputEngine& instance();

	/**
	 * @brief Configures SPI/DMA and starts streaming. Idempotent.
	 *
	 * On first call: configures the SPI peripheral for 16-bit Motorola frames
	 * at `cfg.spi_baud_hz`, routes SCK / TX / CS through `GPIO_FUNC_SPI`, claims
	 * a DMA pacing timer + three DMA channels, installs the render IRQ handler,
	 * and starts streaming. Both channels begin as `kManual` with hold value 0.
	 *
	 * @return true on success; false if a DMA channel or DMA timer could not be claimed.
	 */
	bool start(const OutputEngineConfig& cfg);

	/**
	 * @brief Stores a new hold value for a manual-owned channel.
	 *
	 * Wraps the 12-bit `dac12` in the MCP4822 frame envelope (channel bit, BUF=0,
	 * GAIN per config, SHDN=1) and atomically updates the hold slot.
	 *
	 * @return false (no-op) if the channel's current owner is `kAudio`.
	 */
	bool set_hold_value(AudioCvOutChannel channel, uint16_t dac12);

	/**
	 * @brief Pushes a new audio sample into the per-channel ring.
	 *
	 * Wraps `dac12` in the MCP4822 frame envelope and pushes one frame into the
	 * channel's ring head. The render IRQ pulls 16 samples per block from the
	 * ring tail; if the ring is empty mid-block the last consumed sample is
	 * repeated and the channel's underrun counter increments once for that block.
	 *
	 * Safe to call from IRQ context (intended use: ADC IRQ at audio rate).
	 *
	 * @return false (no-op) if the channel's current owner is `kManual`.
	 */
	bool write_audio_sample(AudioCvOutChannel channel, uint16_t dac12);

	/**
	 * @brief Atomically changes ownership for a channel. Glitch-free.
	 *
	 * On `kManual` -> `kAudio`: pre-fills the audio ring with `kAudioBlockSamples`
	 * copies of the current `hold_frame_*` so the first render block emits a
	 * continuous value.
	 * On `kAudio` -> `kManual`: hold frame retained as-is (last `set_voltage_*`).
	 */
	void set_channel_owner(AudioCvOutChannel channel, ChannelOwner owner);

	/**
	 * @brief Returns the current ownership for a channel.
	 */
	ChannelOwner get_channel_owner(AudioCvOutChannel channel) const;

	/**
	 * @brief Returns a coherent snapshot of latest hold values, ownership, and counters.
	 */
	OutputEngineSnapshot get_snapshot() const;

	/**
	 * @brief Reports whether the engine has been started.
	 */
	bool is_running() const { return running_; }

private:
	OutputEngine() = default;
	OutputEngine(const OutputEngine&) = delete;
	OutputEngine& operator=(const OutputEngine&) = delete;

	void on_dma_irq();
	static void dma_irq_handler_static();
	void refill_current_block();
	void seed_ring_with_hold(AudioCvOutChannel channel);

	bool running_ = false;
	OutputEngineConfig cfg_{};
	spi_inst_t* spi_ = nullptr;
	int dma_data_chan_ = -1;
	int dma_ctrl_chan_ = -1;
	int dma_render_chan_ = -1;
	int dma_pacing_timer_ = -1;

	uint16_t mcp_base_a_ = 0;  // MCP4822 frame envelope for channel A (config bits, data=0).
	uint16_t mcp_base_b_ = 0;  // Same for channel B.

	// Hold slots written by public API. Mutated only under disabled interrupts.
	uint16_t hold_frame_a_ = 0;
	uint16_t hold_frame_b_ = 0;
	ChannelOwner owner_a_ = ChannelOwner::kManual;
	ChannelOwner owner_b_ = ChannelOwner::kManual;

	// Per-channel sample rings. Writer = `write_audio_sample` (any IRQ priority),
	// reader = render IRQ. 8-bit head/tail counters are atomic on Cortex-M; on
	// a single core, IRQs of the same priority dispatch sequentially, so no
	// preemption between writer and reader.
	uint16_t audio_ring_a_[kAudioRingSamples] = {0};
	uint16_t audio_ring_b_[kAudioRingSamples] = {0};
	volatile uint8_t audio_ring_a_head_ = 0;  // writer index (free-running)
	volatile uint8_t audio_ring_a_tail_ = 0;  // reader index (free-running)
	volatile uint8_t audio_ring_b_head_ = 0;
	volatile uint8_t audio_ring_b_tail_ = 0;

	// Last value successfully consumed from each ring. Repeated on underrun so
	// the DAC sees a smooth sample-and-hold rather than a glitch when the
	// feeder rate is below the consumer rate.
	uint16_t last_consumed_a_ = 0;
	uint16_t last_consumed_b_ = 0;

	// Counters mutated only in render IRQ.
	uint64_t total_blocks_ = 0;
	uint32_t audio_underrun_a_ = 0;
	uint32_t audio_underrun_b_ = 0;

	// Read-address pointer used by the ctrl channel to re-seed data channel each cycle.
	const volatile uint16_t* streaming_block_read_addr_ = nullptr;
};

}  // namespace brain::internal
