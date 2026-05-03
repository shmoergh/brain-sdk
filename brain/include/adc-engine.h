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

private:
	AdcEngine() = default;
	AdcEngine(const AdcEngine&) = delete;
	AdcEngine& operator=(const AdcEngine&) = delete;

	void apply_pot_scan_config(const PotsConfig& pots_config);
	void reset_pot_state_machine();
	void switch_mux_to(uint8_t logical_pot_index);

	void on_dma_irq();
	static void dma_irq_handler_static();

	bool running_ = false;
	bool pot_scanning_enabled_ = false;
	int dma_data_chan_ = -1;
	int dma_ctrl_chan_ = -1;

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
