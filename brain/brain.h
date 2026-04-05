#pragma once

#include <cstdint>
#include <type_traits>

#include "include/adc-arbiter.h"
#include "include/buttons.h"
#include "include/constants.h"
#include "include/init-status.h"
#include "include/inputs.h"
#include "include/leds.h"
#include "include/midi-parser.h"
#include "include/midi-to-cv.h"
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
	kBrainFeatureMidiToCv = 1u << 5,
	kBrainFeatureMidiParser = 1u << 6,
};

constexpr uint32_t kBrainFeaturesAll =
	kBrainFeatureLeds |
	kBrainFeatureButtons |
	kBrainFeatureOutputs |
	kBrainFeatureInputs |
	kBrainFeaturePots |
	kBrainFeatureMidiParser |
	kBrainFeatureMidiToCv;

template <uint32_t Features = kBrainFeaturesAll>
class BrainT {
public:
	static constexpr bool kHasLeds = (Features & kBrainFeatureLeds) != 0;
	static constexpr bool kHasButtons = (Features & kBrainFeatureButtons) != 0;
	static constexpr bool kHasOutputs = (Features & kBrainFeatureOutputs) != 0;
	static constexpr bool kHasInputs = (Features & kBrainFeatureInputs) != 0;
	static constexpr bool kHasPots = (Features & kBrainFeaturePots) != 0;
	static constexpr bool kHasMidiParser = (Features & kBrainFeatureMidiParser) != 0;
	static constexpr bool kHasMidiToCv = (Features & kBrainFeatureMidiToCv) != 0;

	static_assert(!kHasMidiToCv || kHasOutputs,
		"kBrainFeatureMidiToCv requires kBrainFeatureOutputs");
	static_assert(!kHasMidiToCv || kHasMidiParser,
		"kBrainFeatureMidiToCv requires kBrainFeatureMidiParser");

	using LedsType = std::conditional_t<kHasLeds, Leds, BrainDisabledComponent>;
	using ButtonsType = std::conditional_t<kHasButtons, Buttons, BrainDisabledComponent>;
	using OutputsType = std::conditional_t<kHasOutputs, Outputs, BrainDisabledComponent>;
	using InputsType = std::conditional_t<kHasInputs, Inputs, BrainDisabledComponent>;
	using PotsType = std::conditional_t<kHasPots, Pots, BrainDisabledComponent>;
	using MidiParserType = std::conditional_t<kHasMidiParser, MidiParser, BrainDisabledComponent>;
	using MidiToCvType = std::conditional_t<kHasMidiToCv, MidiToCV, BrainDisabledComponent>;

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

	bool is_leds_initialized() const { return leds_initialized_; }
	bool is_buttons_initialized() const { return buttons_initialized_; }
	bool is_outputs_initialized() const { return outputs_initialized_; }
	bool is_inputs_initialized() const { return inputs_initialized_; }
	bool is_pots_initialized() const { return pots_initialized_; }
	bool is_midi_parser_initialized() const { return midi_parser_initialized_; }
	bool is_midi_to_cv_initialized() const { return midi_to_cv_initialized_; }

	BrainInitStatus init() {
		return init_all();
	}

	BrainInitStatus init_all() {
		bool any_ok = false;

		BrainInitStatus status = init_leds();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;

		status = init_buttons();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;

		status = init_outputs();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;

		status = init_pots();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;

		status = init_inputs();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;

		status = init_midi_parser();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;

		return any_ok ? BrainInitStatus::kOk : BrainInitStatus::kAlreadyInitialized;
	}

	template <bool Enabled = kHasLeds, typename std::enable_if<Enabled, int>::type = 0>
	BrainInitStatus init_leds(LedMode mode = LedMode::kPwm) {
		if (leds_initialized_) return BrainInitStatus::kAlreadyInitialized;
		leds.init(mode);
		leds_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	template <bool Enabled = kHasLeds, typename std::enable_if<!Enabled, int>::type = 0>
	BrainInitStatus init_leds(LedMode mode = LedMode::kPwm) {
		(void)mode;
		return BrainInitStatus::kAlreadyInitialized;
	}

	template <bool Enabled = kHasButtons, typename std::enable_if<Enabled, int>::type = 0>
	BrainInitStatus init_buttons(bool pull_up = true) {
		if (buttons_initialized_) return BrainInitStatus::kAlreadyInitialized;
		buttons.init(pull_up);
		buttons_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	template <bool Enabled = kHasButtons, typename std::enable_if<!Enabled, int>::type = 0>
	BrainInitStatus init_buttons(bool pull_up = true) {
		(void)pull_up;
		return BrainInitStatus::kAlreadyInitialized;
	}

	template <bool Enabled = kHasOutputs, typename std::enable_if<Enabled, int>::type = 0>
	BrainInitStatus init_outputs() {
		if (outputs_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (!outputs.init()) return BrainInitStatus::kFailed;
		outputs_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	template <bool Enabled = kHasOutputs, typename std::enable_if<!Enabled, int>::type = 0>
	BrainInitStatus init_outputs() {
		return BrainInitStatus::kAlreadyInitialized;
	}

	template <bool Enabled = kHasInputs, typename std::enable_if<Enabled, int>::type = 0>
	BrainInitStatus init_inputs() {
		if (inputs_initialized_) return BrainInitStatus::kAlreadyInitialized;
		inputs.set_audio_cv_dma_enabled(adc_optimization_enabled_ && audio_cv_dma_enabled_);
		if (!inputs.init()) return BrainInitStatus::kFailed;
		inputs_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	template <bool Enabled = kHasInputs, typename std::enable_if<!Enabled, int>::type = 0>
	BrainInitStatus init_inputs() {
		return BrainInitStatus::kAlreadyInitialized;
	}

	template <bool Enabled = kHasPots, typename std::enable_if<Enabled, int>::type = 0>
	BrainInitStatus init_pots(const PotsConfig& config = create_default_pots_config()) {
		if (pots_initialized_) return BrainInitStatus::kAlreadyInitialized;
		pots.init(config);
		pots.set_optimized_sampling_enabled(adc_optimization_enabled_ && shared_pot_sampling_enabled_);
		pots_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	template <bool Enabled = kHasPots, typename std::enable_if<!Enabled, int>::type = 0>
	BrainInitStatus init_pots(const PotsConfig& config = create_default_pots_config()) {
		(void)config;
		return BrainInitStatus::kAlreadyInitialized;
	}

	template <bool Enabled = kHasMidiParser, typename std::enable_if<Enabled, int>::type = 0>
	BrainInitStatus init_midi_parser(uint32_t baud_rate = 31250) {
		if (midi_parser_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (!midi_parser.init_uart(baud_rate)) return BrainInitStatus::kFailed;
		midi_parser_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	template <bool Enabled = kHasMidiParser, typename std::enable_if<!Enabled, int>::type = 0>
	BrainInitStatus init_midi_parser(uint32_t baud_rate = 31250) {
		(void)baud_rate;
		return BrainInitStatus::kAlreadyInitialized;
	}

	template <bool Enabled = kHasMidiToCv, typename std::enable_if<Enabled, int>::type = 0>
	BrainInitStatus init_midi_to_cv(
		AudioCvOutChannel cv_channel = AudioCvOutChannel::kChannelA,
		uint8_t midi_channel = 1,
		uint32_t baud_rate = 31250) {
		if (midi_to_cv_initialized_) return BrainInitStatus::kAlreadyInitialized;

		if (init_outputs() == BrainInitStatus::kFailed) return BrainInitStatus::kFailed;
		if (init_midi_parser(baud_rate) == BrainInitStatus::kFailed) return BrainInitStatus::kFailed;

		midi_to_cv.set_dependencies(&outputs, &midi_parser);
		BrainInitStatus status = midi_to_cv.init(cv_channel, midi_channel);
		if (status == BrainInitStatus::kFailed) {
			return BrainInitStatus::kFailed;
		}

		midi_to_cv_initialized_ = true;
		return status;
	}

	template <bool Enabled = kHasMidiToCv, typename std::enable_if<!Enabled, int>::type = 0>
	BrainInitStatus init_midi_to_cv(
		AudioCvOutChannel cv_channel = AudioCvOutChannel::kChannelA,
		uint8_t midi_channel = 1,
		uint32_t baud_rate = 31250) {
		(void)cv_channel;
		(void)midi_channel;
		(void)baud_rate;
		return BrainInitStatus::kAlreadyInitialized;
	}

	void update() {
		update_all();
	}

	void update_all() {
		update_leds();
		update_buttons();
		update_inputs();
		update_pots();
		update_midi_to_cv();
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

	template <bool Enabled = kHasMidiToCv, typename std::enable_if<Enabled, int>::type = 0>
	void update_midi_to_cv() {
		if (!midi_to_cv_initialized_) return;
		midi_to_cv.update();
	}

	template <bool Enabled = kHasMidiToCv, typename std::enable_if<!Enabled, int>::type = 0>
	void update_midi_to_cv() {}

	BRAIN_NO_UNIQUE_ADDRESS LedsType leds{};
	BRAIN_NO_UNIQUE_ADDRESS ButtonsType buttons{};
	BRAIN_NO_UNIQUE_ADDRESS OutputsType outputs{};
	BRAIN_NO_UNIQUE_ADDRESS InputsType inputs{};
	BRAIN_NO_UNIQUE_ADDRESS PotsType pots{};
	BRAIN_NO_UNIQUE_ADDRESS MidiParserType midi_parser{};
	BRAIN_NO_UNIQUE_ADDRESS MidiToCvType midi_to_cv{};

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
	bool leds_initialized_ = false;
	bool buttons_initialized_ = false;
	bool outputs_initialized_ = false;
	bool inputs_initialized_ = false;
	bool pots_initialized_ = false;
	bool midi_parser_initialized_ = false;
	bool midi_to_cv_initialized_ = false;
};

using Brain = BrainT<kBrainFeaturesAll>;
using BrainAll = BrainT<kBrainFeaturesAll>;
using BrainIO = BrainT<kBrainFeatureInputs | kBrainFeatureOutputs>;
using BrainUI = BrainT<kBrainFeatureLeds | kBrainFeatureButtons | kBrainFeaturePots>;
using BrainWithMidiParser = BrainT<kBrainFeatureMidiParser>;
using BrainWithMidiToCv = BrainT<kBrainFeatureOutputs | kBrainFeatureMidiParser | kBrainFeatureMidiToCv>;
using BrainMinimal = BrainT<0>;

#undef BRAIN_NO_UNIQUE_ADDRESS
