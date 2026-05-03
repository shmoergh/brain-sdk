#pragma once

#include <cstdint>

#include "pots-core.h"

namespace brain::internal {

/**
 * @brief Stable cache of the latest ADC inputs and pot values published by AdcEngine.
 *
 * AdcEngine continuously samples three ADC channels in fixed round-robin order
 * (POT, IN1, IN2). Pot samples flow through a settle/average state machine inside
 * the DMA IRQ; only the published averaged value lands in `pot_raw[]`. IN1/IN2
 * carry the most recent raw samples.
 */
struct AdcSnapshot {
	uint16_t in1_raw = 0;
	uint16_t in2_raw = 0;
	uint16_t pot_raw[kMaxPots] = {0, 0, 0, 0};
	uint64_t total_samples = 0;
	uint32_t pot_switch_count = 0;
	uint32_t pot_discard_count = 0;
};

/**
 * @brief Singleton owner of the RP2040/RP2350 ADC. Phase 1 of the Brain SDK 2.1 refactor.
 *
 * Runs continuous DMA-paced round-robin sampling over (POT, IN1, IN2) at full ADC
 * speed. Inputs and Pots become cache readers via `get_snapshot()`. The pot scanner
 * lives entirely inside the DMA IRQ — `mux switch -> discard N -> average M -> publish`.
 *
 * Safe concurrency model: the DMA IRQ is the sole writer of all internal state.
 * Readers call `get_snapshot()`, which briefly disables interrupts and copies a
 * small struct. No spinlocks, no atomics.
 */
class AdcEngine {
public:
	/**
	 * @brief Returns the singleton instance (Meyers singleton; thread-safe init in C++11+).
	 */
	static AdcEngine& instance();

	/**
	 * @brief Starts continuous round-robin sampling without enabling pot scanning.
	 *
	 * Idempotent. On first call: configures the ADC for 12-bit DMA-paced sampling
	 * at full speed, claims two DMA channels (data + ctrl), and starts a self-
	 * looping DMA chain over the fixed (POT, IN1, IN2) round-robin. The ctrl
	 * channel rewrites the data channel's write pointer on every cycle, so the
	 * loop runs entirely in hardware — the IRQ only processes samples. The pot
	 * scanner state machine is left disabled; POT samples are still taken (for
	 * deterministic frame layout) but ignored. IN1/IN2 caches update every frame.
	 *
	 * Used by `Inputs` when no pot scanning is required.
	 *
	 * @return true on success, false if DMA channels could not be claimed.
	 */
	bool start();

	/**
	 * @brief Enables pot scanning, starting the engine first if needed.
	 *
	 * Idempotent. Calls `start()` if not already running, then claims the mux
	 * GPIOs (S0/S1) and configures the pot scanner state machine from `pots_config`.
	 * Subsequent calls re-apply the pot scan parameters and reset the state machine.
	 *
	 * Used by `Pots`. Coexists freely with a prior `start()` from `Inputs`.
	 *
	 * @return true on success.
	 */
	bool enable_pots(const PotsConfig& pots_config);

	/**
	 * @brief Atomically updates pot scan parameters. Must be called after `enable_pots()`.
	 *
	 * Briefly disables interrupts to swap in the new config and reset the pot
	 * scanner state machine to pot 0 in the settling state. Last published pot
	 * values are preserved for indices that remain in range.
	 */
	void reconfigure_pots(const PotsConfig& pots_config);

	/**
	 * @brief Returns a coherent snapshot of the latest published values.
	 *
	 * Briefly disables interrupts (typically <1 µs) while copying the small
	 * snapshot struct.
	 */
	AdcSnapshot get_snapshot() const;

	/**
	 * @brief Reports whether the engine has been started.
	 */
	bool is_running() const { return running_; }

	/**
	 * @brief Per-sample callback fired from the DMA IRQ in audio mode.
	 *
	 * Receives the latest raw IN1/IN2 readings (12-bit), a coherent snapshot of
	 * cached state (pot raw values, counters), and the registered context. Runs
	 * in DMA IRQ context — must be brief (≤ ~20 µs at sample_period_us = 23 to
	 * leave headroom inside the 23 µs IRQ budget).
	 */
	using AdcAudioCallback = void (*)(uint16_t in1_raw, uint16_t in2_raw,
	                                   const AdcSnapshot& snap, void* ctx);

	/**
	 * @brief Switches the engine into audio mode: 1-frame DMA buffer at sample rate.
	 *
	 * Reconfigures ADC clkdiv so each round-robin frame [POT, IN1, IN2] takes
	 * `sample_period_us` µs, and shrinks the DMA transfer count to one frame so
	 * the IRQ fires at sample rate (~43 kHz at the default 23 µs). The pot
	 * scanner advances by one POT sample per IRQ.
	 *
	 * Calls `start()` first if the engine isn't running. Idempotent: calling
	 * again with a different period reapplies the new clkdiv atomically.
	 *
	 * @return true on success, false if the engine could not be started.
	 */
	bool enable_audio_mode(uint32_t sample_period_us);

	/**
	 * @brief Returns the engine to CV mode: 32-frame buffer at full ADC speed.
	 *
	 * Reverts ADC clkdiv to 0 (full speed) and restores the original 32-frame
	 * buffer. Clears the audio callback. No-op if audio mode wasn't enabled.
	 */
	void disable_audio_mode();

	/**
	 * @brief Registers the per-sample callback fired from the DMA IRQ in audio mode.
	 *
	 * Single subscriber; passing `nullptr` clears the callback. Must be called
	 * after `enable_audio_mode()` for the callback to fire.
	 */
	void set_audio_callback(AdcAudioCallback callback, void* ctx);

	/**
	 * @brief Reports whether the engine is currently in audio mode.
	 */
	bool is_audio_mode() const { return audio_mode_enabled_; }

	/**
	 * @brief Pauses ADC sampling so flash writes can run safely.
	 *
	 * Stops the ADC, waits for any in-flight conversion to drain, and clears
	 * the FIFO. The DMA chain naturally goes idle without DREQs and no DMA
	 * completion IRQ accumulates while interrupts are disabled by the bootrom
	 * flash routines. Internal state (mux position, scanner state, latest
	 * cached values) is preserved verbatim — `resume_after_flash()` continues
	 * from where this left off.
	 *
	 * No-op if the engine isn't running. Idempotent. Safe to call from any
	 * context that isn't the DMA IRQ itself.
	 *
	 * Used by `Storage` around its `flash_safe_execute` calls. Firmwares that
	 * call `flash_range_program` / `flash_range_erase` directly (bypassing
	 * the `Storage` API) must pair this with `resume_after_flash()` themselves.
	 */
	void pause_for_flash();

	/**
	 * @brief Resumes ADC sampling after a flash write.
	 *
	 * Restarts the ADC. The DMA chain picks up from where the pause left it,
	 * and pot/input snapshots resume updating within ~one buffer period.
	 *
	 * No-op if the engine isn't running, or if `pause_for_flash()` wasn't
	 * called first.
	 */
	void resume_after_flash();

private:
	AdcEngine() = default;
	AdcEngine(const AdcEngine&) = delete;
	AdcEngine& operator=(const AdcEngine&) = delete;

	void apply_pot_scan_config(const PotsConfig& pots_config);
	void reset_pot_state_machine();
	void switch_mux_to(uint8_t logical_pot_index);
	void run_pot_scanner_one_sample(uint16_t pot_sample);
	AdcSnapshot build_snapshot_inline() const;

	void on_dma_irq();
	static void dma_irq_handler_static();

	bool running_ = false;
	bool pot_scanning_enabled_ = false;
	bool audio_mode_enabled_ = false;
	bool paused_for_flash_ = false;
	int dma_data_chan_ = -1;
	int dma_ctrl_chan_ = -1;

	// Audio-mode subscriber. Cleared by disable_audio_mode().
	AdcAudioCallback audio_callback_ = nullptr;
	void* audio_ctx_ = nullptr;

	// Mux pin storage (set on first start, fixed thereafter)
	uint8_t mux_s0_gpio_ = 0;
	uint8_t mux_s1_gpio_ = 0;

	// Pot scan config (read by IRQ; updated under interrupts-disabled in reconfigure_pots)
	uint8_t pot_count_ = 0;
	uint8_t pot_channel_map_[kMaxPots] = {0, 0, 0, 0};
	uint16_t pot_settling_samples_ = 1;
	uint16_t pot_average_samples_ = 1;

	// Pot scanner state machine (mutated only in IRQ)
	enum PotState : uint8_t {
		kPotStateSettling = 0,
		kPotStateAveraging = 1,
	};
	uint8_t pot_current_index_ = 0;
	uint8_t pot_state_ = kPotStateSettling;
	uint16_t pot_settling_samples_remaining_ = 0;
	uint16_t pot_average_samples_remaining_ = 0;
	uint32_t pot_average_accumulator_ = 0;

	// Latest published values (mutated only in IRQ; read with interrupts disabled)
	uint16_t latest_in1_raw_ = 0;
	uint16_t latest_in2_raw_ = 0;
	uint16_t latest_pot_raw_[kMaxPots] = {0, 0, 0, 0};
	uint64_t total_samples_ = 0;
	uint32_t pot_switch_count_ = 0;
	uint32_t pot_discard_count_ = 0;
};

}  // namespace brain::internal
