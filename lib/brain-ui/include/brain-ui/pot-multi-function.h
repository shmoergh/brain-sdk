#ifndef BRAIN_UI_POT_MULTI_FUNCTION_H_
#define BRAIN_UI_POT_MULTI_FUNCTION_H_

#include <cstdint>

#include "brain-ui/pots.h"

namespace brain::ui {

enum class PotBehavior : uint8_t {
	kDirect = 0,
	kPickup = 1,
	kValueScale = 2
};

struct PotFunctionConfig {
	uint8_t function_id;
	uint8_t pot_index;
	int32_t min_value;
	int32_t max_value;
	int32_t initial_value;
	PotBehavior behavior;
	uint8_t pickup_hysteresis;
};

class PotMultiFunction {
public:
	static constexpr uint8_t kMaxFunctions = 16;
	static constexpr uint8_t kMaxPots = 4;

	PotMultiFunction();

	void init(uint8_t max_functions = kMaxFunctions);
	bool register_function(const PotFunctionConfig& config);

	void set_active_function(uint8_t pot_index, uint8_t function_id);
	void set_active_functions(const uint8_t* per_pot_function_ids, uint8_t count);

	void update(Pots& pots);

	int32_t get_value(uint8_t function_id) const;
	bool get_changed(uint8_t function_id) const;
	void clear_changed_flags();

private:
	struct FunctionState {
		bool registered;
		uint8_t function_id;
		uint8_t pot_index;
		int32_t min_value;
		int32_t max_value;
		int32_t value;
		PotBehavior behavior;
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

	int find_index_by_function_id(uint8_t function_id) const;
	int32_t clamp_value(const FunctionState& state, int32_t value) const;
	int32_t map_raw_to_range(const FunctionState& state, uint16_t raw) const;
	uint16_t read_raw_for_function(Pots& pots, const FunctionState& state);
	void on_function_activated(FunctionState& state, uint16_t raw);
	void update_pickup(FunctionState& state, uint16_t raw);
	void update_value_scale(FunctionState& state, uint16_t raw);
};

}  // namespace brain::ui

#endif  // BRAIN_UI_POT_MULTI_FUNCTION_H_
