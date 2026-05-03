#pragma once

#include <hardware/spi.h>

#include <cstdint>

#include "outputs.h"

namespace brain::internal {

/**
 * @brief Per-channel ownership of a streamed DAC output.
 *
 * `kManual` (default) routes the channel's stream from a cached hold value
 * updated by `Outputs::set_voltage_*`. `kAudio` routes from a per-sample slot
 * updated by `OutputEngine::write_audio_sample`.
 */
enum class ChannelOwner : uint8_t {
	kManual = 0,
	kAudio = 1,
};

/**
 * @brief Stable cache of the latest streamed values, ownership, and counters.
 *
 * Reader-side copy under disabled interrupts. Mirrors the AdcEngine snapshot
 * pattern. `hold_frame_*` and `audio_frame_*` are 16-bit MCP4822 frames already
 * carrying the channel/config bits — what the DAC sees on the wire.
 */
struct OutputEngineSnapshot {
	uint16_t hold_frame_a = 0;
	uint16_t hold_frame_b = 0;
	uint16_t audio_frame_a = 0;
	uint16_t audio_frame_b = 0;
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
 * @brief Singleton owner of the MCP4822 DAC. Phase 2 of the Brain SDK 2.1 refactor.
 *
 * Streams interleaved channel-A / channel-B 16-bit frames into the SPI peripheral
 * via three chained DMA channels:
 *
 *   render -> ctrl -> data -> render -> ...
 *
 *  - data    streams `streaming_block_` to `&spi_get_hw(spi)->dr`, paced by
 *            DREQ from a DMA pacing timer at frame rate (= 2 / sample_period_us).
 *  - ctrl    resets `data.read_addr` back to `&streaming_block_[0]` so streaming
 *            loops without CPU intervention.
 *  - render  copies `current_block_` -> `streaming_block_`, fires an IRQ on
 *            completion. The IRQ handler refills `current_block_` with the next
 *            batch of (A, B) frames based on per-channel ownership.
 *
 * The PL022 in 16-bit Motorola frame format toggles CS between frames; MCP4822
 * latches its input register on each rising CS edge. No CPU work per frame.
 *
 * Concurrency: render IRQ is the sole writer of all internal state. Public
 * accessors briefly disable interrupts and copy a small struct or single field.
 */
class OutputEngine {
public:
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
	 * @brief Pushes a new audio sample for an audio-owned channel.
	 *
	 * Wraps `dac12` in the MCP4822 frame envelope and atomically updates the
	 * audio slot. Sets the channel's "fresh" bit; the render IRQ clears it after
	 * each block and increments the audio underrun counter if the slot was stale.
	 *
	 * @return false (no-op) if the channel's current owner is `kManual`.
	 */
	bool write_audio_sample(AudioCvOutChannel channel, uint16_t dac12);

	/**
	 * @brief Atomically changes ownership for a channel. Glitch-free.
	 *
	 * On `kManual` -> `kAudio`: seeds the audio slot with the current hold frame
	 * so the first IRQ after the flip emits a continuous value.
	 * On `kAudio` -> `kManual`: hold frame retained as-is (last `set_voltage_*`).
	 */
	void set_channel_owner(AudioCvOutChannel channel, ChannelOwner owner);

	/**
	 * @brief Returns the current ownership for a channel.
	 */
	ChannelOwner get_channel_owner(AudioCvOutChannel channel) const;

	/**
	 * @brief Returns a coherent snapshot of latest slots, ownership, and counters.
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

	bool running_ = false;
	OutputEngineConfig cfg_{};
	spi_inst_t* spi_ = nullptr;
	int dma_data_chan_ = -1;
	int dma_ctrl_chan_ = -1;
	int dma_render_chan_ = -1;
	int dma_pacing_timer_ = -1;

	uint16_t mcp_base_a_ = 0;  // MCP4822 frame envelope for channel A (config bits, data=0).
	uint16_t mcp_base_b_ = 0;  // Same for channel B.

	// Slots written by public API, read by render IRQ. Mutated only under
	// disabled interrupts (single-cycle 16-bit / single-byte stores on Cortex-M;
	// the IRQ disable bracket is for grouped struct copies in get_snapshot()).
	uint16_t hold_frame_a_ = 0;
	uint16_t hold_frame_b_ = 0;
	uint16_t audio_frame_a_ = 0;
	uint16_t audio_frame_b_ = 0;
	ChannelOwner owner_a_ = ChannelOwner::kManual;
	ChannelOwner owner_b_ = ChannelOwner::kManual;
	bool audio_fresh_a_ = false;
	bool audio_fresh_b_ = false;

	// Counters mutated only in render IRQ.
	uint64_t total_blocks_ = 0;
	uint32_t audio_underrun_a_ = 0;
	uint32_t audio_underrun_b_ = 0;

	// Read-address pointer used by the ctrl channel to re-seed data channel each cycle.
	const volatile uint16_t* streaming_block_read_addr_ = nullptr;
};

}  // namespace brain::internal
