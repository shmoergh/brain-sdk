#pragma once

#include <cstdint>
#include <type_traits>

#include "include/adc-arbiter.h"
#include "include/buttons.h"
#include "include/constants.h"
#include "include/inputs.h"
#include "include/leds.h"
#include "include/outputs.h"
#include "include/pots.h"
#include "include/storage.h"

struct BrainDisabledComponent {};

#if defined(__has_cpp_attribute)
#if __has_cpp_attribute(no_unique_address)
#define BRAIN_NO_UNIQUE_ADDRESS [[no_unique_address]]
#else
#define BRAIN_NO_UNIQUE_ADDRESS
#endif
#else
#define BRAIN_NO_UNIQUE_ADDRESS
#endif

enum BrainFeature : uint32_t {
	kBrainFeatureLeds = 1u << 0,
	kBrainFeatureButtons = 1u << 1,
	kBrainFeatureOutputs = 1u << 2,
	kBrainFeatureInputs = 1u << 3,
	kBrainFeaturePots = 1u << 4,
};

constexpr uint32_t kBrainFeaturesAll =
	kBrainFeatureLeds |
	kBrainFeatureButtons |
	kBrainFeatureOutputs |
	kBrainFeatureInputs |
	kBrainFeaturePots;

template <uint32_t Features = kBrainFeaturesAll>
class BrainT {
public:
	static constexpr bool kHasLeds = (Features & kBrainFeatureLeds) != 0;
	static constexpr bool kHasButtons = (Features & kBrainFeatureButtons) != 0;
	static constexpr bool kHasOutputs = (Features & kBrainFeatureOutputs) != 0;
	static constexpr bool kHasInputs = (Features & kBrainFeatureInputs) != 0;
	static constexpr bool kHasPots = (Features & kBrainFeaturePots) != 0;

	using LedsType = std::conditional_t<kHasLeds, Leds, BrainDisabledComponent>;
	using ButtonsType = std::conditional_t<kHasButtons, Buttons, BrainDisabledComponent>;
	using OutputsType = std::conditional_t<kHasOutputs, Outputs, BrainDisabledComponent>;
	using InputsType = std::conditional_t<kHasInputs, Inputs, BrainDisabledComponent>;
	using PotsType = std::conditional_t<kHasPots, Pots, BrainDisabledComponent>;

	BrainT() = default;
	BrainT(const BrainT&) = delete;
	BrainT& operator=(const BrainT&) = delete;
	BrainT(BrainT&&) = delete;
	BrainT& operator=(BrainT&&) = delete;

	void enable_adc_optimization(bool enabled = true) {
		adc_optimization_enabled_ = enabled;
		apply_adc_policy_to_components();
	}

	void set_audio_cv_dma_enabled(bool enabled) {
		audio_cv_dma_enabled_ = enabled;
		apply_adc_policy_to_components();
	}

	void set_shared_pot_sampling_enabled(bool enabled) {
		shared_pot_sampling_enabled_ = enabled;
		apply_adc_policy_to_components();
	}

	bool is_adc_optimization_enabled() const {
		return adc_optimization_enabled_;
	}

	bool is_audio_cv_dma_enabled() const {
		return audio_cv_dma_enabled_;
	}

	bool is_shared_pot_sampling_enabled() const {
		return shared_pot_sampling_enabled_;
	}

	bool init() {
		return init_all();
	}

	bool init_all() {
		bool ok = true;
		ok = ok && init_leds();
		ok = ok && init_buttons();
		ok = ok && init_outputs();
		ok = ok && init_pots();
		ok = ok && init_inputs();
		return ok;
	}

	template <bool Enabled = kHasLeds, typename std::enable_if<Enabled, int>::type = 0>
	bool init_leds(LedMode mode = LedMode::kPwm) {
		leds.init(mode);
		return true;
	}

	template <bool Enabled = kHasLeds, typename std::enable_if<!Enabled, int>::type = 0>
	bool init_leds(LedMode mode = LedMode::kPwm) {
		(void)mode;
		return true;
	}

	template <bool Enabled = kHasButtons, typename std::enable_if<Enabled, int>::type = 0>
	bool init_buttons(bool pull_up = true) {
		buttons.init(pull_up);
		return true;
	}

	template <bool Enabled = kHasButtons, typename std::enable_if<!Enabled, int>::type = 0>
	bool init_buttons(bool pull_up = true) {
		(void)pull_up;
		return true;
	}

	template <bool Enabled = kHasOutputs, typename std::enable_if<Enabled, int>::type = 0>
	bool init_outputs() {
		return outputs.init();
	}

	template <bool Enabled = kHasOutputs, typename std::enable_if<!Enabled, int>::type = 0>
	bool init_outputs() {
		return true;
	}

	template <bool Enabled = kHasInputs, typename std::enable_if<Enabled, int>::type = 0>
	bool init_inputs() {
		inputs.set_audio_cv_dma_enabled(adc_optimization_enabled_ && audio_cv_dma_enabled_);
		return inputs.init();
	}

	template <bool Enabled = kHasInputs, typename std::enable_if<!Enabled, int>::type = 0>
	bool init_inputs() {
		return true;
	}

	template <bool Enabled = kHasPots, typename std::enable_if<Enabled, int>::type = 0>
	bool init_pots(const PotsConfig& config = create_default_pots_config()) {
		pots.init(config);
		pots.set_optimized_sampling_enabled(adc_optimization_enabled_ && shared_pot_sampling_enabled_);
		return true;
	}

	template <bool Enabled = kHasPots, typename std::enable_if<!Enabled, int>::type = 0>
	bool init_pots(const PotsConfig& config = create_default_pots_config()) {
		(void)config;
		return true;
	}

	void update() {
		update_all();
	}

	void update_all() {
		update_leds();
		update_buttons();
		update_inputs();
		update_pots();
	}

	template <bool Enabled = kHasLeds, typename std::enable_if<Enabled, int>::type = 0>
	void update_leds() {
		leds.update();
	}

	template <bool Enabled = kHasLeds, typename std::enable_if<!Enabled, int>::type = 0>
	void update_leds() {}

	template <bool Enabled = kHasButtons, typename std::enable_if<Enabled, int>::type = 0>
	void update_buttons() {
		buttons.update();
	}

	template <bool Enabled = kHasButtons, typename std::enable_if<!Enabled, int>::type = 0>
	void update_buttons() {}

	template <bool Enabled = kHasInputs, typename std::enable_if<Enabled, int>::type = 0>
	void update_inputs() {
		inputs.update();
	}

	template <bool Enabled = kHasInputs, typename std::enable_if<!Enabled, int>::type = 0>
	void update_inputs() {}

	template <bool Enabled = kHasPots, typename std::enable_if<Enabled, int>::type = 0>
	void update_pots() {
		pots.scan();
	}

	template <bool Enabled = kHasPots, typename std::enable_if<!Enabled, int>::type = 0>
	void update_pots() {}

	BRAIN_NO_UNIQUE_ADDRESS LedsType leds{};
	BRAIN_NO_UNIQUE_ADDRESS ButtonsType buttons{};
	BRAIN_NO_UNIQUE_ADDRESS OutputsType outputs{};
	BRAIN_NO_UNIQUE_ADDRESS InputsType inputs{};
	BRAIN_NO_UNIQUE_ADDRESS PotsType pots{};

private:
	void apply_adc_policy_to_components() {
		apply_inputs_adc_policy();
		apply_pots_adc_policy();
	}

	template <bool Enabled = kHasInputs, typename std::enable_if<Enabled, int>::type = 0>
	void apply_inputs_adc_policy() {
		inputs.set_audio_cv_dma_enabled(adc_optimization_enabled_ && audio_cv_dma_enabled_);
	}

	template <bool Enabled = kHasInputs, typename std::enable_if<!Enabled, int>::type = 0>
	void apply_inputs_adc_policy() {}

	template <bool Enabled = kHasPots, typename std::enable_if<Enabled, int>::type = 0>
	void apply_pots_adc_policy() {
		pots.set_optimized_sampling_enabled(adc_optimization_enabled_ && shared_pot_sampling_enabled_);
	}

	template <bool Enabled = kHasPots, typename std::enable_if<!Enabled, int>::type = 0>
	void apply_pots_adc_policy() {}

	bool adc_optimization_enabled_ = true;
	bool audio_cv_dma_enabled_ = true;
	bool shared_pot_sampling_enabled_ = true;
};

using Brain = BrainT<kBrainFeaturesAll>;
using BrainAll = BrainT<kBrainFeaturesAll>;
using BrainIO = BrainT<kBrainFeatureInputs | kBrainFeatureOutputs>;
using BrainUI = BrainT<kBrainFeatureLeds | kBrainFeatureButtons | kBrainFeaturePots>;
using BrainMinimal = BrainT<0>;

#undef BRAIN_NO_UNIQUE_ADDRESS
