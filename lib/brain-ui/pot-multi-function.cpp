#include "brain-ui/pot-multi-function.h"

namespace brain::ui {

PotMultiFunction::PotMultiFunction()
	: max_functions_(kMaxFunctions) {
	for (uint8_t i = 0; i < kMaxPots; i++) {
		active_function_per_pot_[i] = 255;
		previous_active_function_per_pot_[i] = 255;
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
		previous_active_function_per_pot_[i] = 255;
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
		functions_[i].mode = config.mode;
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
		if (idx < 0) {
			previous_active_function_per_pot_[pot_index] = 255;
			continue;
		}

		FunctionState& state = functions_[idx];
		if (state.pot_index != pot_index) continue;

		uint16_t raw = read_raw_for_function(pots, state);
		if (previous_active_function_per_pot_[pot_index] != function_id) {
			on_function_activated(state, raw);
			previous_active_function_per_pot_[pot_index] = function_id;
		}

		switch (state.mode) {
			case PotMode::kPickup:
				update_pickup(state, raw);
				state.last_raw = raw;
				break;
			case PotMode::kValueScale:
				update_value_scale(state, raw);
				break;
			case PotMode::kDirect: {
				int32_t mapped = map_raw_to_range(state, raw);
				mapped = clamp_value(state, mapped);
				if (mapped != state.value) {
					state.value = mapped;
					state.changed = true;
				}
				state.last_raw = raw;
				break;
			}
		}
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

void PotMultiFunction::on_function_activated(FunctionState& state, uint16_t raw) {
	state.last_raw = raw;
	if (state.mode == PotMode::kPickup) {
		int32_t mapped = map_raw_to_range(state, raw);
		int32_t diff = mapped - state.value;
		if (diff < 0) diff = -diff;
		state.picked_up = (diff <= state.pickup_hysteresis);
	}
	if (state.mode == PotMode::kValueScale) {
		state.accumulator_q16 = state.value << 16;
		state.scale_direction = 0;
		state.scale_anchor_raw = raw;
		state.scale_anchor_value = state.value;
		state.scale_step_q16 = 0;
	}
}

void PotMultiFunction::update_pickup(FunctionState& state, uint16_t raw) {
	int32_t mapped_prev = map_raw_to_range(state, state.last_raw);
	int32_t mapped_curr = map_raw_to_range(state, raw);

	int32_t diff_curr = mapped_curr - state.value;
	if (diff_curr < 0) diff_curr = -diff_curr;

	if (!state.picked_up) {
		bool crossed = ((mapped_prev <= state.value) && (mapped_curr >= state.value))
			|| ((mapped_prev >= state.value) && (mapped_curr <= state.value));
		if (crossed || (diff_curr <= state.pickup_hysteresis)) {
			state.picked_up = true;
		}
	}

	if (state.picked_up) {
		int32_t mapped = clamp_value(state, mapped_curr);
		if (mapped != state.value) {
			state.value = mapped;
			state.changed = true;
		}
	}
}

void PotMultiFunction::update_value_scale(FunctionState& state, uint16_t raw) {
	static constexpr uint16_t kRawMax = 255;
	static constexpr uint16_t kNoiseDeadband = 2;
	static constexpr uint16_t kEdgeSnapThreshold = 2;
	static constexpr uint16_t kHighEdgeStart = kRawMax - kEdgeSnapThreshold;

	if (raw == state.last_raw) return;

	// Force deterministic endpoints only when crossing from interior to edge.
	// This avoids immediate snapping on function switch when the pot already sits at an edge.
	if (raw <= kEdgeSnapThreshold && state.last_raw > kEdgeSnapThreshold) {
		if (state.value != state.min_value) {
			state.value = state.min_value;
			state.changed = true;
		}
		state.accumulator_q16 = state.value << 16;
		state.last_raw = raw;
		return;
	}
	if (raw >= kHighEdgeStart && state.last_raw < kHighEdgeStart) {
		if (state.value != state.max_value) {
			state.value = state.max_value;
			state.changed = true;
		}
		state.accumulator_q16 = state.value << 16;
		state.last_raw = raw;
		return;
	}

	uint16_t raw_delta = (raw > state.last_raw) ? (raw - state.last_raw) : (state.last_raw - raw);
	if (raw_delta <= kNoiseDeadband) return;

	int8_t direction = (raw > state.last_raw) ? 1 : -1;
	if (direction != state.scale_direction) {
		state.scale_direction = direction;
		state.scale_anchor_raw = state.last_raw;
		state.scale_anchor_value = state.value;

		uint32_t raw_runway = 0;
		int32_t value_runway = 0;
		if (direction > 0) {
			raw_runway = kRawMax - state.scale_anchor_raw;
			value_runway = state.max_value - state.scale_anchor_value;
		} else {
			raw_runway = state.scale_anchor_raw;
			value_runway = state.scale_anchor_value - state.min_value;
		}

		if (raw_runway == 0 || value_runway <= 0) {
			state.scale_step_q16 = 0;
		} else {
			// Rounded fixed-point step to reduce directional quantization bias.
			state.scale_step_q16 =
				((static_cast<uint32_t>(value_runway) << 16) + (raw_runway / 2)) / raw_runway;
		}
	}

	if (state.scale_step_q16 == 0) {
		state.last_raw = raw;
		return;
	}

	uint64_t delta_q16 = static_cast<uint64_t>(raw_delta) * state.scale_step_q16;
	int64_t accumulator_q16 = state.accumulator_q16;
	if (direction > 0) {
		accumulator_q16 += static_cast<int64_t>(delta_q16);
	} else {
		accumulator_q16 -= static_cast<int64_t>(delta_q16);
	}

	const int64_t min_q16 = static_cast<int64_t>(state.min_value) << 16;
	const int64_t max_q16 = static_cast<int64_t>(state.max_value) << 16;
	if (accumulator_q16 < min_q16) accumulator_q16 = min_q16;
	if (accumulator_q16 > max_q16) accumulator_q16 = max_q16;
	state.accumulator_q16 = static_cast<int32_t>(accumulator_q16);

	int32_t rounded = (state.accumulator_q16 >= 0)
		? ((state.accumulator_q16 + (1 << 15)) >> 16)
		: ((state.accumulator_q16 - (1 << 15)) >> 16);
	int32_t clamped = clamp_value(state, rounded);
	if (clamped != state.value) {
		state.value = clamped;
		state.changed = true;
	}
	state.last_raw = raw;
}

}  // namespace brain::ui
