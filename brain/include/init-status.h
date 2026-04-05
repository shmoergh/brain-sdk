#pragma once

enum class BrainInitStatus : unsigned char {
	kOk = 0,
	kAlreadyInitialized = 1,
	kFailed = 2
};

inline bool brain_init_succeeded(BrainInitStatus status) {
	return status != BrainInitStatus::kFailed;
}

