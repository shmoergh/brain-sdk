#pragma once

enum class BrainInitStatus : unsigned char {
	kOk = 0,
	kAlreadyInitialized = 1,
	kFailed = 2
};

// Convenience aliases for concise status checks.
constexpr BrainInitStatus kBrainInitStatusOk = BrainInitStatus::kOk;
constexpr BrainInitStatus kBrainInitStatusAlreadyInitialized = BrainInitStatus::kAlreadyInitialized;
constexpr BrainInitStatus kBrainInitStatusFailed = BrainInitStatus::kFailed;

inline bool brain_init_succeeded(BrainInitStatus status) {
	return status != BrainInitStatus::kFailed;
}
