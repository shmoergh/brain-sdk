#ifndef BRAIN_UI_POT_MULTI_FUNCTION_H_
#define BRAIN_UI_POT_MULTI_FUNCTION_H_

#include <cstdint>

#include "brain-ui/pots.h"

namespace brain::ui {

/**
 * @brief Pot-to-value mapping mode for a registered function.
 */
enum class PotMode : uint8_t {
	/// Value directly follows current pot position.
	kDirect = 0,
	/// Value changes only after physical pot crosses stored value.
	kPickup = 1,
	/// Pot movement scales value incrementally based on movement direction.
	kValueScale = 2
};

/**
 * @brief Registration config for a logical function controlled by a pot.
 */
struct PotFunctionConfig {
	uint8_t function_id;  ///< Unique function identifier used by API calls.
	uint8_t pot_index;  ///< Pot index this function reads from.
	int32_t min_value;  ///< Inclusive minimum function value.
	int32_t max_value;  ///< Inclusive maximum function value.
	int32_t initial_value;  ///< Initial value (clamped to [min_value, max_value]).
	PotMode mode;  ///< Pot response mode.
	uint8_t pickup_hysteresis;  ///< Allowed error window for pickup engagement.
};

/**
 * @brief Multi-context pot mapper for mode-dependent logical function values.
 *
 * Lets one physical pot control multiple logical parameters, with one active
 * function per pot at runtime. Mode can be direct, pickup, or value-scale.
 */
class PotMultiFunction {
public:
	static constexpr uint8_t kMaxFunctions = 16;
	static constexpr uint8_t kMaxPots = 4;

	/**
	 * @brief Construct a PotMultiFunction with empty registrations.
	 */
	PotMultiFunction();

	/**
	 * @brief Reset internal state and configure active registration capacity.
	 *
	 * Clears all registered functions, active assignments, and changed flags.
	 *
	 * @param max_functions Maximum number of functions to allow (clamped to kMaxFunctions)
	 */
	void init(uint8_t max_functions = kMaxFunctions);

	/**
	 * @brief Register a logical function definition.
	 *
	 * Fails when the function_id already exists, pot index is invalid, min/max
	 * range is invalid, or no free registration slot is available.
	 *
	 * @param config Function registration parameters
	 * @return true if registered successfully, false otherwise
	 */
	bool register_function(const PotFunctionConfig& config);

	/**
	 * @brief Set active function for one pot.
	 *
	 * @param pot_index Pot index to assign
	 * @param function_id Function id to activate for that pot
	 */
	void set_active_function(uint8_t pot_index, uint8_t function_id);

	/**
	 * @brief Set active functions for multiple pots in one call.
	 *
	 * Copies up to kMaxPots entries from per_pot_function_ids.
	 *
	 * @param per_pot_function_ids Array of function ids indexed by pot
	 * @param count Number of entries in the array
	 */
	void set_active_functions(const uint8_t* per_pot_function_ids, uint8_t count);

	/**
	 * @brief Update active functions using current pot readings.
	 *
	 * Reads each active pot and applies that function's configured mode.
	 * When a value changes, the corresponding function changed flag is set.
	 *
	 * @param pots Pot reader used for current input values
	 */
	void update(Pots& pots);

	/**
	 * @brief Get current value of a registered function.
	 *
	 * @param function_id Function id
	 * @return Current value, or 0 when function is not found
	 */
	int32_t get_value(uint8_t function_id) const;

	/**
	 * @brief Get whether a function value changed during the last update().
	 *
	 * @param function_id Function id
	 * @return true if changed flag is set, false otherwise
	 */
	bool get_changed(uint8_t function_id) const;

	/**
	 * @brief Clear changed flags for all registered functions.
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

		// pickup state (phase 2)
		bool picked_up;
		uint16_t last_raw;

		// value-scale state (phase 3)
		int32_t accumulator_q16;
		int8_t scale_direction;
		uint16_t scale_anchor_raw;
		int32_t scale_anchor_value;
		uint32_t scale_step_q16;
	};

	uint8_t max_functions_;
	uint8_t active_function_per_pot_[kMaxPots];
	uint8_t previous_active_function_per_pot_[kMaxPots];
	FunctionState functions_[kMaxFunctions];

	/**
	 * @brief Find internal index of a function by its function id.
	 *
	 * @param function_id Function id
	 * @return Internal index, or -1 if not found
	 */
	int find_index_by_function_id(uint8_t function_id) const;

	/**
	 * @brief Clamp a candidate value to a function state's min/max bounds.
	 *
	 * @param state Function state providing bounds
	 * @param value Candidate value
	 * @return Clamped value
	 */
	int32_t clamp_value(const FunctionState& state, int32_t value) const;

	/**
	 * @brief Map raw pot input (0-255) to function value range.
	 *
	 * @param state Function state providing range
	 * @param raw Raw pot reading
	 * @return Mapped value within function range
	 */
	int32_t map_raw_to_range(const FunctionState& state, uint16_t raw) const;

	/**
	 * @brief Read raw pot value for a function's assigned pot index.
	 *
	 * @param pots Pot source
	 * @param state Function state containing pot index
	 * @return Raw scaled pot reading
	 */
	uint16_t read_raw_for_function(Pots& pots, const FunctionState& state);

	/**
	 * @brief Initialize per-mode runtime state when function becomes active.
	 *
	 * @param state Function state to initialize
	 * @param raw Current raw pot reading at activation time
	 */
	void on_function_activated(FunctionState& state, uint16_t raw);

	/**
	 * @brief Update function value in pickup mode.
	 *
	 * @param state Function state to update
	 * @param raw Current raw pot reading
	 */
	void update_pickup(FunctionState& state, uint16_t raw);

	/**
	 * @brief Update function value in value-scale mode.
	 *
	 * @param state Function state to update
	 * @param raw Current raw pot reading
	 */
	void update_value_scale(FunctionState& state, uint16_t raw);
};

}  // namespace brain::ui

#endif  // BRAIN_UI_POT_MULTI_FUNCTION_H_
