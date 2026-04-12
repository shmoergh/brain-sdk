#pragma once

#include <cstdint>

#include "pots-core.h"

enum class PotMode : uint8_t {
	kDirect = 0,
	kPickup = 1,
	kValueScale = 2
};

// Convenience aliases for less verbose mode selection.
constexpr PotMode kPotMultiFunctionModeDirect = PotMode::kDirect;
constexpr PotMode kPotMultiFunctionModePickup = PotMode::kPickup;
constexpr PotMode kPotMultiFunctionModeValueScale = PotMode::kValueScale;

struct PotFunctionConfig {
	uint8_t function_id;
	uint8_t pot_index;
	int32_t min_value;
	int32_t max_value;
	int32_t initial_value;
	PotMode mode;
	uint8_t pickup_hysteresis;
};

class PotMultiFunction {
public:
	static constexpr uint8_t kMaxFunctions = 16;
	static constexpr uint8_t kMaxPots = 4;

	/**
	 * @brief Creates a `PotMultiFunction` mapper for assigning multiple logical parameters to shared pots.
	 */
	PotMultiFunction();

	/**
	 * @brief Resets internal mapping state and capacity limits.
	 * @param max_functions Maximum number of logical functions that can be registered.
	 * Values above `kMaxFunctions` are clamped.
	 */
	void init(uint8_t max_functions = kMaxFunctions);

	/**
	 * @brief Registers one logical function mapping.
	 * @param config Mapping configuration:
	 * - `function_id`: app-defined ID used for lookup and activation.
	 * - `pot_index`: physical pot index (0..`kMaxPots`-1).
	 * - `min_value` / `max_value`: logical output range.
	 * - `initial_value`: startup value for this logical parameter.
	 * - `mode`: tracking mode:
	 *   - `kPotMultiFunctionModeDirect`: raw pot position always maps directly to logical value.
	 *   - `kPotMultiFunctionModePickup`: value changes only after physical pot crosses stored value.
	 *   - `kPotMultiFunctionModeValueScale`: incremental relative scaling to avoid jumps when switching targets.
	 * - `pickup_hysteresis`: tolerance window used by pickup mode.
	 * @return `true` if function was registered, `false` on invalid config, duplicate ID, or no free slot.
	 */
	bool register_function(const PotFunctionConfig& config);

	/**
	 * @brief Selects which logical function a pot controls right now.
	 * @param pot_index Physical pot index (0..`kMaxPots`-1).
	 * @param function_id Registered function ID to bind to that pot for subsequent updates.
	 */
	void set_active_function(uint8_t pot_index, uint8_t function_id);

	/**
	 * @brief Sets active function mapping for multiple pots in one call.
	 * @param per_pot_function_ids Array where each entry is the active function ID for matching pot index.
	 * @param count Number of entries to consume from `per_pot_function_ids` (clamped to `kMaxPots`).
	 */
	void set_active_functions(const uint8_t* per_pot_function_ids, uint8_t count);

	/**
	 * @brief Resets per-function tracking state after UI mode/page changes.
	 * @param clear_active_mappings `true` clears active function IDs for all pots.
	 * `false` keeps current active assignments but resets pickup/value-scale internals.
	 */
	void reset_for_mode_change(bool clear_active_mappings = true);

	/**
	 * @brief Updates all active mappings using buffered pot reads and performs a fresh `pots.scan()`.
	 * @param pots `Pots` instance used as source of physical pot values.
	 */
	void update(Pots& pots);

	/**
	 * @brief Updates active mappings using direct one-shot pot reads (`get_single`) without buffered scan path.
	 * @param pots `Pots` instance used as source of physical pot values.
	 */
	void update_single(Pots& pots);

	/**
	 * @brief Updates active mappings using buffered pot values.
	 * @param pots `Pots` instance used as source of buffered values.
	 * @param perform_scan `true` calls `pots.scan()` first; `false` uses current buffer as-is.
	 */
	void update_buffered(Pots& pots, bool perform_scan = true);

	/**
	 * @brief Returns the current logical value for a registered function.
	 * @param function_id Registered function ID.
	 * @return Current mapped logical value for that function, or `0` if ID is unknown.
	 */
	int32_t get_value(uint8_t function_id) const;

	/**
	 * @brief Returns whether a registered function changed on the last update.
	 * @param function_id Registered function ID.
	 * @return `true` if that function's logical value changed during recent update processing.
	 */
	bool get_changed(uint8_t function_id) const;

	/**
	 * @brief Clears `changed` flags for all registered functions.
	 */
	void clear_changed_flags();

private:
	struct FunctionState {
		bool registered;
		uint8_t function_id;
		uint8_t pot_index;
		int32_t min_value;
		int32_t max_value;
		int32_t value;
		PotMode mode;
		uint8_t pickup_hysteresis;
		bool changed;
		bool picked_up;
		uint16_t last_raw;
		int32_t accumulator_q16;
		int8_t scale_direction;
		uint16_t scale_anchor_raw;
		int32_t scale_anchor_value;
		uint32_t scale_step_q16;
	};

	uint8_t max_functions_;
	uint8_t active_function_per_pot_[kMaxPots];
	uint8_t previous_active_function_per_pot_[kMaxPots];
	uint16_t raw_max_ =
		static_cast<uint16_t>((1u << kDefaultPotsOutputResolution) - 1u);
	FunctionState functions_[kMaxFunctions];

	int find_index_by_function_id(uint8_t function_id) const;
	int32_t clamp_value(const FunctionState& state, int32_t value) const;
	int32_t map_raw_to_range(const FunctionState& state, uint16_t raw) const;
	uint16_t read_raw_for_function(Pots& pots, const FunctionState& state);
	void update_internal(Pots& pots, bool use_buffered_values, bool perform_scan);
	void on_function_activated(FunctionState& state, uint16_t raw);
	void update_pickup(FunctionState& state, uint16_t raw);
	void update_value_scale(FunctionState& state, uint16_t raw);
};
