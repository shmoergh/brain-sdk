#pragma once

#include <hardware/spi.h>

#include <cstdint>

#include "outputs.h"

namespace brain::internal {

/**
 * @brief Per-channel ownership of a streamed DAC output.
 *
 * `kManual` (default) routes the channel's stream from a cached hold value
 * updated by `Outputs::set_voltage_*`. `kAudio` routes from per-sample writes
 * via `OutputEngine::write_audio_sample` directly into the streaming buffer.
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
 *
 * `total_frames` / `total_blocks` are computed from elapsed time × configured
 * frame rate (since the engine itself runs entirely in hardware with no IRQs).
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
	// Diagnostic: increments any time the writer detects it has caught up to
	// or lapped the DMA's current read position. Should stay at 0 with locked rates.
	uint32_t audio_overflow_a = 0;
	uint32_t audio_overflow_b = 0;
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
 * @brief Singleton owner of the MCP4822 DAC. Phase 3 refactor.
 *
 * **Architecture: zero-IRQ, zero-chain ring DMA.** A single DMA channel reads
 * 16-bit frames from a power-of-two-sized ring buffer (`kStreamFrames` = 64
 * frames = 128 bytes) directly into the SPI data register, paced by a hardware
 * DMA timer at frame rate (= 2 / sample_period_us). The channel's read-side
 * ring DMA mode wraps the read pointer in hardware every 128 bytes — no chain
 * channel, no render channel, no CPU interrupt. The transfer count is set to
 * 0xFFFFFFFF so the channel runs autonomously for ~14 hours.
 *
 * **Buffer layout:** the 64-frame buffer holds 32 alternating (A, B) stereo
 * pairs. `buffer[2k]` = A frame at pair k; `buffer[2k+1]` = B frame at pair k.
 *
 * **Manual mode:** every channel-A slot in the buffer holds `hold_frame_a_`;
 * every channel-B slot holds `hold_frame_b_`. The DMA cycles through the buffer
 * indefinitely, emitting the same A and B values at each frame. `set_hold_value`
 * updates `hold_frame_*` and writes the new value into all that channel's slots
 * under disabled-IRQ — change reaches the DAC within one buffer rotation
 * (~736 µs at sample_period_us = 23).
 *
 * **Audio mode:** the writer (`write_audio_sample`, called from the ADC IRQ at
 * audio rate) maintains a per-channel "next pair to write" counter that leads
 * the DMA's current read position by `kSafetyLead` pairs. With matched rates
 * (DMA frame rate = 2 × audio rate, hardware-locked since both derive from the
 * same crystal-driven PLLs and there's no chain overhead), the lead stays
 * constant forever. The writer simply writes its frame into the channel's slot
 * at the current writer-pair, then advances by one pair per audio sample.
 *
 * Concurrency: writer counters are written only from the audio IRQ; other
 * fields are written under disabled interrupts. Public accessors briefly
 * disable interrupts for coherent snapshots.
 */
class OutputEngine {
public:
	/**
	 * @brief Public block-size constant (informational; the underlying DMA is
	 * frame-paced, not block-paced).
	 */
	static constexpr uint32_t kAudioBlockSamples = 16;

	/**
	 * @brief Returns the singleton instance (Meyers singleton; thread-safe init).
	 */
	static OutputEngine& instance();

	/**
	 * @brief Configures SPI/DMA and starts streaming. Idempotent.
	 *
	 * On first call: configures the SPI peripheral for 16-bit Motorola frames
	 * at `cfg.spi_baud_hz`, routes SCK / TX / CS through `GPIO_FUNC_SPI`, claims
	 * a DMA pacing timer + one DMA channel, and starts streaming. Both channels
	 * begin as `kManual` with hold value 0.
	 *
	 * @return true on success; false if a DMA channel or DMA timer could not be claimed.
	 */
	bool start(const OutputEngineConfig& cfg);

	/**
	 * @brief Stores a new hold value for a manual-owned channel and propagates
	 * it into every slot of that channel inside the streaming buffer.
	 *
	 * @return false (no-op) if the channel's current owner is `kAudio`.
	 */
	bool set_hold_value(AudioCvOutChannel channel, uint16_t dac12);

	/**
	 * @brief Writes a new audio sample into the streaming buffer at the writer's
	 * current pair index for this channel, then advances the writer.
	 *
	 * Safe to call from IRQ context (intended use: ADC IRQ at audio rate).
	 *
	 * @return false (no-op) if the channel's current owner is `kManual`.
	 */
	bool write_audio_sample(AudioCvOutChannel channel, uint16_t dac12);

	/**
	 * @brief Atomically changes ownership for a channel. Glitch-free.
	 *
	 * On `kManual` -> `kAudio`: seeds the writer's pair index to lead the DMA's
	 * current read position by `kSafetyLead` pairs. The buffer slots ahead of
	 * the writer still hold the channel's hold value, so the first samples out
	 * are continuous with the prior manual output.
	 * On `kAudio` -> `kManual`: re-fills all of that channel's slots with
	 * `hold_frame_*`. The audio writer ceases to push.
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

	// Buffer holds kStreamPairs (= 32) alternating (A, B) stereo pairs = 64 frames
	// = 128 bytes. Power of 2 for the DMA read-side ring wrap.
	static constexpr uint32_t kStreamPairs = 32;
	static constexpr uint32_t kStreamFrames = kStreamPairs * 2;
	static constexpr uint32_t kStreamBytes = kStreamFrames * sizeof(uint16_t);
	static constexpr uint32_t kRingSizeBits = 7;  // log2(kStreamBytes)
	static_assert(kStreamBytes == (1u << kRingSizeBits), "ring DMA needs power-of-two byte size");

	void fill_channel_a_with(uint16_t frame);
	void fill_channel_b_with(uint16_t frame);

	bool running_ = false;
	OutputEngineConfig cfg_{};
	spi_inst_t* spi_ = nullptr;
	int dma_data_chan_ = -1;
	int dma_pacing_timer_ = -1;

	uint16_t mcp_base_a_ = 0;  // MCP4822 frame envelope for channel A (config bits, data=0).
	uint16_t mcp_base_b_ = 0;  // Same for channel B.

	uint16_t hold_frame_a_ = 0;
	uint16_t hold_frame_b_ = 0;
	ChannelOwner owner_a_ = ChannelOwner::kManual;
	ChannelOwner owner_b_ = ChannelOwner::kManual;

	// Diagnostic counters. Reserved for now; `write_audio_sample` is rate-
	// agnostic so neither under- nor over-runs really apply, but the snapshot
	// fields are retained so AudioProcessor stats and existing tests compile.
	uint32_t audio_underrun_a_ = 0;
	uint32_t audio_underrun_b_ = 0;
	uint32_t audio_overflow_a_ = 0;
	uint32_t audio_overflow_b_ = 0;

	// For computing total_frames in get_snapshot() (no IRQ to count, so we use
	// elapsed time × configured frame rate).
	uint64_t start_time_us_ = 0;
};

}  // namespace brain::internal
