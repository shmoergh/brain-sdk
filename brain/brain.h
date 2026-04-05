#pragma once

#include <cstdint>

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

#if !defined(BRAIN_USE_ALL) && !defined(BRAIN_USE_LEDS) && !defined(BRAIN_USE_BUTTONS) && \
	!defined(BRAIN_USE_OUTPUTS) && !defined(BRAIN_USE_INPUTS) && !defined(BRAIN_USE_POTS) && \
	!defined(BRAIN_USE_MIDI_PARSER) && !defined(BRAIN_USE_MIDI_TO_CV) && \
	!defined(BRAIN_USE_POT_MULTI_FUNCTION) && !defined(BRAIN_USE_STORAGE)
#error "Brain config missing: define BRAIN_USE_ALL=1 or one/more BRAIN_USE_* macros before including brain/brain.h"
#endif

#if !defined(BRAIN_USE_ALL)
#define BRAIN_USE_ALL 0
#endif

#if BRAIN_USE_ALL
#define BRAIN_CFG_LEDS 1
#define BRAIN_CFG_BUTTONS 1
#define BRAIN_CFG_OUTPUTS 1
#define BRAIN_CFG_INPUTS 1
#define BRAIN_CFG_POTS 1
#define BRAIN_CFG_MIDI_PARSER 1
#define BRAIN_CFG_MIDI_TO_CV 1
#define BRAIN_CFG_POT_MULTI_FUNCTION 1
#define BRAIN_CFG_STORAGE 1
#else
#if !defined(BRAIN_USE_LEDS)
#define BRAIN_USE_LEDS 0
#endif
#if !defined(BRAIN_USE_BUTTONS)
#define BRAIN_USE_BUTTONS 0
#endif
#if !defined(BRAIN_USE_OUTPUTS)
#define BRAIN_USE_OUTPUTS 0
#endif
#if !defined(BRAIN_USE_INPUTS)
#define BRAIN_USE_INPUTS 0
#endif
#if !defined(BRAIN_USE_POTS)
#define BRAIN_USE_POTS 0
#endif
#if !defined(BRAIN_USE_MIDI_PARSER)
#define BRAIN_USE_MIDI_PARSER 0
#endif
#if !defined(BRAIN_USE_MIDI_TO_CV)
#define BRAIN_USE_MIDI_TO_CV 0
#endif
#if !defined(BRAIN_USE_POT_MULTI_FUNCTION)
#define BRAIN_USE_POT_MULTI_FUNCTION 0
#endif
#if !defined(BRAIN_USE_STORAGE)
#define BRAIN_USE_STORAGE 0
#endif

#define BRAIN_CFG_LEDS BRAIN_USE_LEDS
#define BRAIN_CFG_BUTTONS BRAIN_USE_BUTTONS
#define BRAIN_CFG_OUTPUTS BRAIN_USE_OUTPUTS
#define BRAIN_CFG_INPUTS BRAIN_USE_INPUTS
#define BRAIN_CFG_POTS BRAIN_USE_POTS
#define BRAIN_CFG_MIDI_PARSER BRAIN_USE_MIDI_PARSER
#define BRAIN_CFG_MIDI_TO_CV BRAIN_USE_MIDI_TO_CV
#define BRAIN_CFG_POT_MULTI_FUNCTION BRAIN_USE_POT_MULTI_FUNCTION
#define BRAIN_CFG_STORAGE BRAIN_USE_STORAGE
#endif

static_assert(!(BRAIN_CFG_MIDI_TO_CV && !BRAIN_CFG_OUTPUTS),
	"BRAIN_USE_MIDI_TO_CV requires BRAIN_USE_OUTPUTS=1 or BRAIN_USE_ALL=1");
static_assert(!(BRAIN_CFG_MIDI_TO_CV && !BRAIN_CFG_MIDI_PARSER),
	"BRAIN_USE_MIDI_TO_CV requires BRAIN_USE_MIDI_PARSER=1 or BRAIN_USE_ALL=1");
static_assert(!(BRAIN_CFG_POT_MULTI_FUNCTION && !BRAIN_CFG_POTS),
	"BRAIN_USE_POT_MULTI_FUNCTION requires BRAIN_USE_POTS=1 or BRAIN_USE_ALL=1");

class Brain {
public:
	Brain() = default;
	Brain(const Brain&) = delete;
	Brain& operator=(const Brain&) = delete;
	Brain(Brain&&) = delete;
	Brain& operator=(Brain&&) = delete;

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

	BrainInitStatus init() {
		return init_all();
	}

	BrainInitStatus init_all() {
		bool any_ok = false;
		BrainInitStatus status = BrainInitStatus::kAlreadyInitialized;

#if BRAIN_CFG_LEDS
		status = init_leds();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;
#endif

#if BRAIN_CFG_BUTTONS
		status = init_buttons();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;
#endif

#if BRAIN_CFG_STORAGE
		status = init_storage();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;
#endif

#if BRAIN_CFG_OUTPUTS
		status = init_outputs();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;
#endif

#if BRAIN_CFG_POTS
		status = init_pots();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;
#endif

#if BRAIN_CFG_INPUTS
		status = init_inputs();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;
#endif

#if BRAIN_CFG_MIDI_PARSER
		status = init_midi_parser();
		if (status == BrainInitStatus::kFailed) return status;
		if (status == BrainInitStatus::kOk) any_ok = true;
#endif

		return any_ok ? BrainInitStatus::kOk : BrainInitStatus::kAlreadyInitialized;
	}

	void update() {
		update_all();
	}

	void update_all() {
#if BRAIN_CFG_LEDS
		update_leds();
#endif
#if BRAIN_CFG_BUTTONS
		update_buttons();
#endif
#if BRAIN_CFG_INPUTS
		update_inputs();
#endif
#if BRAIN_CFG_POTS
		update_pots();
#endif
#if BRAIN_CFG_MIDI_TO_CV
		update_midi_to_cv();
#endif
#if BRAIN_CFG_POT_MULTI_FUNCTION
		update_pot_multi();
#endif
	}

#if BRAIN_CFG_LEDS
	BrainInitStatus init_leds(LedMode mode = LedMode::kPwm) {
		if (leds_initialized_) return BrainInitStatus::kAlreadyInitialized;
		leds.init(mode);
		leds_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	void update_leds() {
		leds.update();
	}

	bool is_leds_initialized() const {
		return leds_initialized_;
	}

	Leds leds{};
#endif

#if BRAIN_CFG_BUTTONS
	BrainInitStatus init_buttons(bool pull_up = true) {
		if (buttons_initialized_) return BrainInitStatus::kAlreadyInitialized;
		buttons.init(pull_up);
		buttons_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	void update_buttons() {
		buttons.update();
	}

	bool is_buttons_initialized() const {
		return buttons_initialized_;
	}

	Buttons buttons{};
#endif

#if BRAIN_CFG_STORAGE
	BrainInitStatus init_storage() {
		if (storage_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (!storage.init(true)) return BrainInitStatus::kFailed;
		storage_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	bool is_storage_initialized() const {
		return storage_initialized_;
	}

	Storage storage{};
#endif

#if BRAIN_CFG_OUTPUTS
	BrainInitStatus init_outputs() {
		if (outputs_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (!outputs.init()) return BrainInitStatus::kFailed;
		outputs_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	bool is_outputs_initialized() const {
		return outputs_initialized_;
	}

	Outputs outputs{};
#endif

#if BRAIN_CFG_INPUTS
	BrainInitStatus init_inputs() {
		if (inputs_initialized_) return BrainInitStatus::kAlreadyInitialized;
		inputs.set_audio_cv_dma_enabled(adc_optimization_enabled_ && audio_cv_dma_enabled_);
		if (!inputs.init()) return BrainInitStatus::kFailed;
		inputs_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	void update_inputs() {
		inputs.update();
	}

	bool is_inputs_initialized() const {
		return inputs_initialized_;
	}

	Inputs inputs{};
#endif

#if BRAIN_CFG_POTS
	BrainInitStatus init_pots(const PotsConfig& config = create_default_pots_config()) {
		if (pots_initialized_) return BrainInitStatus::kAlreadyInitialized;
		pots.init(config);
		pots.set_optimized_sampling_enabled(adc_optimization_enabled_ && shared_pot_sampling_enabled_);
		pots_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	BrainInitStatus reconfigure_pots(
		const PotsConfig& config,
		bool reset_pot_multi_state = true,
		bool clear_pot_multi_active_mappings = false) {
		if (!pots_initialized_) {
			return init_pots(config);
		}

		pots.reconfigure(config);
		pots.set_optimized_sampling_enabled(adc_optimization_enabled_ && shared_pot_sampling_enabled_);

#if BRAIN_CFG_POT_MULTI_FUNCTION
		if (reset_pot_multi_state && pot_multi_initialized_) {
			pot_multi.reset_for_mode_change(clear_pot_multi_active_mappings);
		}
#endif

		return BrainInitStatus::kOk;
	}

	void update_pots() {
		pots.scan();
	}

	bool is_pots_initialized() const {
		return pots_initialized_;
	}

	Pots pots{};
#endif

#if BRAIN_CFG_MIDI_PARSER
	BrainInitStatus init_midi_parser(uint32_t baud_rate = 31250) {
		if (midi_parser_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (!midi_parser.init_uart(baud_rate)) return BrainInitStatus::kFailed;
		midi_parser_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	bool is_midi_parser_initialized() const {
		return midi_parser_initialized_;
	}

	MidiParser midi_parser{};
#endif

#if BRAIN_CFG_MIDI_TO_CV
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

	void update_midi_to_cv() {
		if (!midi_to_cv_initialized_) return;
		midi_to_cv.update();
	}

	bool is_midi_to_cv_initialized() const {
		return midi_to_cv_initialized_;
	}

	MidiToCV midi_to_cv{};
#endif

#if BRAIN_CFG_POT_MULTI_FUNCTION
	BrainInitStatus init_pot_multi(uint8_t max_functions = PotMultiFunction::kMaxFunctions) {
		if (pot_multi_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (init_pots() == BrainInitStatus::kFailed) return BrainInitStatus::kFailed;
		pot_multi.init(max_functions);
		pot_multi_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	void update_pot_multi(bool perform_scan = false) {
		if (!pot_multi_initialized_) return;
		pot_multi.update_buffered(pots, perform_scan);
	}

	void update_pot_multi_single() {
		if (!pot_multi_initialized_) return;
		pot_multi.update_single(pots);
	}

	void reset_pot_multi_for_mode_change(bool clear_active_mappings = false) {
		if (!pot_multi_initialized_) return;
		pot_multi.reset_for_mode_change(clear_active_mappings);
	}

	bool is_pot_multi_initialized() const {
		return pot_multi_initialized_;
	}

	PotMultiFunction pot_multi{};
#endif

private:
	void apply_adc_policy_to_components() {
#if BRAIN_CFG_INPUTS
		inputs.set_audio_cv_dma_enabled(adc_optimization_enabled_ && audio_cv_dma_enabled_);
#endif
#if BRAIN_CFG_POTS
		pots.set_optimized_sampling_enabled(adc_optimization_enabled_ && shared_pot_sampling_enabled_);
#endif
	}

	bool adc_optimization_enabled_ = true;
	bool audio_cv_dma_enabled_ = true;
	bool shared_pot_sampling_enabled_ = true;

#if BRAIN_CFG_LEDS
	bool leds_initialized_ = false;
#endif
#if BRAIN_CFG_BUTTONS
	bool buttons_initialized_ = false;
#endif
#if BRAIN_CFG_STORAGE
	bool storage_initialized_ = false;
#endif
#if BRAIN_CFG_OUTPUTS
	bool outputs_initialized_ = false;
#endif
#if BRAIN_CFG_INPUTS
	bool inputs_initialized_ = false;
#endif
#if BRAIN_CFG_POTS
	bool pots_initialized_ = false;
#endif
#if BRAIN_CFG_MIDI_PARSER
	bool midi_parser_initialized_ = false;
#endif
#if BRAIN_CFG_MIDI_TO_CV
	bool midi_to_cv_initialized_ = false;
#endif
#if BRAIN_CFG_POT_MULTI_FUNCTION
	bool pot_multi_initialized_ = false;
#endif
};

#undef BRAIN_CFG_LEDS
#undef BRAIN_CFG_BUTTONS
#undef BRAIN_CFG_OUTPUTS
#undef BRAIN_CFG_INPUTS
#undef BRAIN_CFG_POTS
#undef BRAIN_CFG_MIDI_PARSER
#undef BRAIN_CFG_MIDI_TO_CV
#undef BRAIN_CFG_POT_MULTI_FUNCTION
#undef BRAIN_CFG_STORAGE
