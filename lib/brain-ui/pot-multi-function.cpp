#include "brain-ui/pot-multi-function.h"

namespace brain::ui {

PotMultiFunction::PotMultiFunction()
	: max_functions_(kMaxFunctions) {
	for (uint8_t i = 0; i < kMaxPots; i++) {
		active_function_per_pot_[i] = 255;
	}

	for (uint8_t i = 0; i < kMaxFunctions; i++) {
		functions_[i].registered = false;
		functions_[i].changed = false;
	}
}

void PotMultiFunction::init(uint8_t max_functions) {
	max_functions_ = (max_functions > kMaxFunctions) ? kMaxFunctions : max_functions;
	for (uint8_t i = 0; i < kMaxPots; i++) {
		active_function_per_pot_[i] = 255;
	}
	for (uint8_t i = 0; i < kMaxFunctions; i++) {
		functions_[i].registered = false;
		functions_[i].changed = false;
	}
}

bool PotMultiFunction::register_function(const PotFunctionConfig& config) {
	if (config.pot_index >= kMaxPots) return false;
	if (config.min_value > config.max_value) return false;
	if (find_index_by_function_id(config.function_id) != -1) return false;

	for (uint8_t i = 0; i < max_functions_; i++) {
		if (functions_[i].registered) continue;

		functions_[i].registered = true;
		functions_[i].function_id = config.function_id;
		functions_[i].pot_index = config.pot_index;
		functions_[i].min_value = config.min_value;
		functions_[i].max_value = config.max_value;
		functions_[i].value = clamp_value(functions_[i], config.initial_value);
		functions_[i].behavior = config.behavior;
		functions_[i].pickup_hysteresis = config.pickup_hysteresis;
		functions_[i].changed = false;
		functions_[i].picked_up = false;
		functions_[i].last_raw = 0;
		functions_[i].accumulator_q16 = 0;
		functions_[i].scale_direction = 0;
		functions_[i].scale_anchor_raw = 0;
		functions_[i].scale_anchor_value = functions_[i].value;
		functions_[i].scale_step_q16 = 0;
		return true;
	}

	return false;
}

void PotMultiFunction::set_active_function(uint8_t pot_index, uint8_t function_id) {
	if (pot_index >= kMaxPots) return;
	active_function_per_pot_[pot_index] = function_id;
}

void PotMultiFunction::set_active_functions(const uint8_t* per_pot_function_ids, uint8_t count) {
	uint8_t n = (count > kMaxPots) ? kMaxPots : count;
	for (uint8_t i = 0; i < n; i++) {
		active_function_per_pot_[i] = per_pot_function_ids[i];
	}
}

void PotMultiFunction::update(Pots& pots) {
	for (uint8_t pot_index = 0; pot_index < kMaxPots; pot_index++) {
		uint8_t function_id = active_function_per_pot_[pot_index];
		int idx = find_index_by_function_id(function_id);
		if (idx < 0) continue;

		FunctionState& state = functions_[idx];
		if (state.pot_index != pot_index) continue;

		uint16_t raw = read_raw_for_function(pots, state);
		int32_t mapped = map_raw_to_range(state, raw);
		mapped = clamp_value(state, mapped);
		if (mapped != state.value) {
			state.value = mapped;
			state.changed = true;
		}
		state.last_raw = raw;
	}
}

int32_t PotMultiFunction::get_value(uint8_t function_id) const {
	int idx = find_index_by_function_id(function_id);
	if (idx < 0) return 0;
	return functions_[idx].value;
}

bool PotMultiFunction::get_changed(uint8_t function_id) const {
	int idx = find_index_by_function_id(function_id);
	if (idx < 0) return false;
	return functions_[idx].changed;
}

void PotMultiFunction::clear_changed_flags() {
	for (uint8_t i = 0; i < max_functions_; i++) {
		if (!functions_[i].registered) continue;
		functions_[i].changed = false;
	}
}

int PotMultiFunction::find_index_by_function_id(uint8_t function_id) const {
	for (uint8_t i = 0; i < max_functions_; i++) {
		if (!functions_[i].registered) continue;
		if (functions_[i].function_id == function_id) return i;
	}
	return -1;
}

int32_t PotMultiFunction::clamp_value(const FunctionState& state, int32_t value) const {
	if (value < state.min_value) return state.min_value;
	if (value > state.max_value) return state.max_value;
	return value;
}

int32_t PotMultiFunction::map_raw_to_range(const FunctionState& state, uint16_t raw) const {
	uint16_t raw_max = (1 << 8) - 1;
	if (raw > raw_max) raw = raw_max;
	int32_t span = state.max_value - state.min_value;
	if (span <= 0) return state.min_value;
	return state.min_value + (static_cast<int32_t>(raw) * span) / raw_max;
}

uint16_t PotMultiFunction::read_raw_for_function(Pots& pots, const FunctionState& state) {
	return pots.get(state.pot_index);
}

}  // namespace brain::ui
