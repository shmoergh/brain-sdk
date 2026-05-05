#pragma once

#include <cstdint>
#include <cstdio>

#include <pico/stdlib.h>

#include "include/audio-processor.h"
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
	!defined(BRAIN_USE_POT_MULTI_FUNCTION) && !defined(BRAIN_USE_STORAGE) && \
	!defined(BRAIN_USE_AUDIO_PROCESSOR)
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
#define BRAIN_CFG_AUDIO_PROCESSOR 1
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
#if !defined(BRAIN_USE_AUDIO_PROCESSOR)
#define BRAIN_USE_AUDIO_PROCESSOR 0
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
#define BRAIN_CFG_AUDIO_PROCESSOR BRAIN_USE_AUDIO_PROCESSOR
#endif

static_assert(!(BRAIN_CFG_MIDI_TO_CV && !BRAIN_CFG_OUTPUTS),
	"BRAIN_USE_MIDI_TO_CV requires BRAIN_USE_OUTPUTS=1 or BRAIN_USE_ALL=1");
static_assert(!(BRAIN_CFG_MIDI_TO_CV && !BRAIN_CFG_MIDI_PARSER),
	"BRAIN_USE_MIDI_TO_CV requires BRAIN_USE_MIDI_PARSER=1 or BRAIN_USE_ALL=1");
static_assert(!(BRAIN_CFG_POT_MULTI_FUNCTION && !BRAIN_CFG_POTS),
	"BRAIN_USE_POT_MULTI_FUNCTION requires BRAIN_USE_POTS=1 or BRAIN_USE_ALL=1");
static_assert(!(BRAIN_CFG_OUTPUTS && !BRAIN_CFG_STORAGE),
	"BRAIN_USE_OUTPUTS requires BRAIN_USE_STORAGE=1 or BRAIN_USE_ALL=1");

class Brain {
public:
	/**
	 * @brief Constructs a `Brain` instance and prepares default runtime state.
	 *
	 * Only one `Brain` instance may exist on a device. The shared singleton
	 * engines (`AdcEngine`, `OutputEngine`) own the underlying hardware, and
	 * a second `Brain`'s components would silently stomp the first's
	 * callback registrations and channel ownership. Constructing a second
	 * `Brain` halts the firmware via `panic()` so the bug is visible
	 * instead of producing subtle audio glitches.
	 *
	 * TODO(3.0): convert `Brain` to a Meyers singleton (`Brain::instance()`)
	 * and drop this runtime guard. That requires changing all callsites
	 * from `Brain brain;` to `auto& brain = Brain::instance();`, which is a
	 * source-level break and so deferred to the next major version.
	 */
	Brain() {
		int& count = live_instance_count();
		if (count != 0) {
			panic("Brain: only one instance allowed per device");
		}
		count = 1;
	}

	~Brain() {
		live_instance_count() = 0;
	}

	/**
	 * @brief Copy construction is disabled for this type.
	 */
	Brain(const Brain&) = delete;

	/**
	 * @brief Copy assignment is disabled for this type.
	 */
	Brain& operator=(const Brain&) = delete;

	/**
	 * @brief Move construction is disabled for this type.
	 */
	Brain(Brain&&) = delete;

	/**
	 * @brief Move assignment is disabled for this type.
	 */
	Brain& operator=(Brain&&) = delete;

	/**
	 * @brief Enables/disables Brain-level ADC optimization policy.
	 * @param enabled `true` lets `Brain` apply optimized defaults to `Inputs` and `Pots`
	 * (`set_audio_cv_dma_enabled` and `set_optimized_sampling_enabled`).
	 * `false` disables those optimizations and applies non-optimized behavior immediately.
	 */
	void enable_adc_optimization(bool enabled = true) {
		adc_optimization_enabled_ = enabled;
		apply_adc_policy_to_components();
	}

	/**
	 * @brief Controls whether `Inputs` should use DMA for CV/audio reads under Brain policy.
	 * @param enabled `true` allows DMA-backed sampling in `Inputs`.
	 * `false` forces direct ADC reads in `Inputs`.
	 */
	void set_audio_cv_dma_enabled(bool enabled) {
		audio_cv_dma_enabled_ = enabled;
		apply_adc_policy_to_components();
	}

	/**
	 * @brief Controls whether `Pots` should use optimized shared sampling under Brain policy.
	 * @param enabled `true` enables optimized pot sampling behavior in `Pots`.
	 * `false` disables optimized path and uses simpler reads.
	 */
	void set_shared_pot_sampling_enabled(bool enabled) {
		shared_pot_sampling_enabled_ = enabled;
		apply_adc_policy_to_components();
	}

	/**
	 * @brief Reports Brain ADC optimization master switch state.
	 * @return `true` when optimization policy is enabled.
	 */
	bool is_adc_optimization_enabled() const {
		return adc_optimization_enabled_;
	}

	/**
	 * @brief Reports Brain policy flag for `Inputs` DMA usage.
	 * @return `true` when Brain is configured to allow DMA path for `Inputs`.
	 */
	bool is_audio_cv_dma_enabled() const {
		return audio_cv_dma_enabled_;
	}

	/**
	 * @brief Reports Brain policy flag for `Pots` optimized sampling.
	 * @return `true` when Brain is configured to enable optimized pot sampling.
	 */
	bool is_shared_pot_sampling_enabled() const {
		return shared_pot_sampling_enabled_;
	}

	/**
	 * @brief Alias for `init_all()`.
	 * @return Same status as `init_all()`.
	 */
	BrainInitStatus init() {
		return init_all();
	}

	/**
	 * @brief Initializes all enabled Brain components in dependency-safe order.
	 * @return `BrainInitStatus::kOk` if at least one component initialized during this call,
	 * `BrainInitStatus::kAlreadyInitialized` if every enabled component was already initialized,
	 * or `BrainInitStatus::kFailed` if any enabled component init fails.
	 */
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

	/**
	 * @brief Alias for `update_all()`.
	 */
	void update() {
		update_all();
	}

	/**
	 * @brief Updates each enabled Brain runtime component once.
	 *
	 * Only components compiled in by `BRAIN_USE_*` are called.
	 */
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
	/**
	 * @brief Initializes `Leds` in the Brain wrapper.
	 * @param mode Panel LED mode (`kLedsModeSimple` or `kLedsModePwm`).
	 * @return `BrainInitStatus::kOk` on first successful initialization,
	 * or `BrainInitStatus::kAlreadyInitialized` if `Leds` is already initialized.
	 */
	BrainInitStatus init_leds(LedMode mode = kLedsModePwm) {
		if (leds_initialized_) return BrainInitStatus::kAlreadyInitialized;
		leds.init(mode);
		leds_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	/**
	 * @brief Calls `leds.update()`.
	 */
	void update_leds() {
		leds.update();
	}

	/**
	 * @brief Reports `Leds` wrapper initialization state.
	 * @return `true` when `init_leds()` succeeded.
	 */
	bool is_leds_initialized() const {
		return leds_initialized_;
	}

	Leds leds{};
#endif

#if BRAIN_CFG_BUTTONS
	/**
	 * @brief Initializes wrapper `Buttons` (`button_a` and `button_b`).
	 * @param pull_up `true` enables pull-up and active-low button logic.
	 * `false` disables pull-up and assumes external wiring provides stable level.
	 * @return `BrainInitStatus::kOk` on first success or `BrainInitStatus::kAlreadyInitialized` if already initialized.
	 */
	BrainInitStatus init_buttons(bool pull_up = true) {
		if (buttons_initialized_) return BrainInitStatus::kAlreadyInitialized;
		buttons.init(pull_up);
		buttons_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	/**
	 * @brief Calls `buttons.update()` to process debounce and callbacks.
	 */
	void update_buttons() {
		buttons.update();
	}

	/**
	 * @brief Reports `Buttons` wrapper initialization state.
	 * @return `true` when `init_buttons()` succeeded.
	 */
	bool is_buttons_initialized() const {
		return buttons_initialized_;
	}

	Buttons buttons{};
#endif

#if BRAIN_CFG_STORAGE
	/**
	 * @brief Initializes `Storage` with protected-layout requirement enabled.
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if already ready,
	 * or `BrainInitStatus::kFailed` when layout protection check fails.
	 */
	BrainInitStatus init_storage() {
		if (storage_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (!storage.init(true)) return BrainInitStatus::kFailed;
		storage_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	/**
	 * @brief Reports `Storage` initialization state.
	 * @return `true` when `init_storage()` succeeded.
	 */
	bool is_storage_initialized() const {
		return storage_initialized_;
	}

	Storage storage{};
#endif

#if BRAIN_CFG_OUTPUTS
	/**
	 * @brief Initializes `Outputs` and wires `Storage` dependency automatically.
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if already initialized,
	 * or `BrainInitStatus::kFailed` when required dependency init or `outputs.init()` fails.
	 */
	BrainInitStatus init_outputs() {
		if (outputs_initialized_) return BrainInitStatus::kAlreadyInitialized;
#if BRAIN_CFG_STORAGE
		BrainInitStatus storage_status = init_storage();
		if (storage_status == BrainInitStatus::kFailed) return BrainInitStatus::kFailed;
		outputs.set_dependencies(&storage);
#endif
		if (!outputs.init()) return BrainInitStatus::kFailed;
		outputs_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	/**
	 * @brief Reports `Outputs` initialization state.
	 * @return `true` when `init_outputs()` succeeded.
	 */
	bool is_outputs_initialized() const {
		return outputs_initialized_;
	}

	Outputs outputs{};
#endif

#if BRAIN_CFG_INPUTS
	/**
	 * @brief Initializes `Inputs` and applies current Brain ADC policy flags.
	 *
	 * Coexists freely with `AudioProcessor`, `Pots`, and `PotMultiFunction` —
	 * all components share the internal `AdcEngine` singleton.
	 *
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if already initialized,
	 * or `BrainInitStatus::kFailed` on `inputs.init()` failure.
	 */
	BrainInitStatus init_inputs() {
		if (inputs_initialized_) return BrainInitStatus::kAlreadyInitialized;
		inputs.set_audio_cv_dma_enabled(adc_optimization_enabled_ && audio_cv_dma_enabled_);
		if (!inputs.init()) return BrainInitStatus::kFailed;
		inputs_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	/**
	 * @brief Calls `inputs.update()` if `Inputs` is initialized.
	 */
	void update_inputs() {
		if (!inputs_initialized_) return;
		inputs.update();
	}

	/**
	 * @brief Reports `Inputs` initialization state.
	 * @return `true` when `init_inputs()` succeeded.
	 */
	bool is_inputs_initialized() const {
		return inputs_initialized_;
	}

	Inputs inputs{};
#endif

#if BRAIN_CFG_POTS
	/**
	 * @brief Initializes `Pots` with provided config and applies Brain sampling policy.
	 *
	 * Coexists freely with `AudioProcessor`, `Inputs`, and `PotMultiFunction` —
	 * all components share the internal `AdcEngine` singleton.
	 *
	 * @param config Pot configuration passed to `pots.init(...)`.
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if already initialized.
	 */
	BrainInitStatus init_pots(const PotsConfig& config = create_default_pots_config()) {
		if (pots_initialized_) return BrainInitStatus::kAlreadyInitialized;
		pots.init(config);
		pots.set_optimized_sampling_enabled(adc_optimization_enabled_ && shared_pot_sampling_enabled_);
		pots_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	/**
	 * @brief Applies a new pots configuration without recreating the Brain wrapper instance.
	 * @param config New pot configuration passed to `pots.reconfigure(...)`.
	 * @param reset_pot_multi_state `true` calls `pot_multi.reset_for_mode_change(...)` after reconfigure (if initialized).
	 * @param clear_pot_multi_active_mappings Forwarded to `reset_for_mode_change(...)` to optionally clear active IDs.
	 * @return `BrainInitStatus::kOk` on success.
	 */
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

	/**
	 * @brief Calls `pots.scan()` if `Pots` is initialized.
	 */
	void update_pots() {
		if (!pots_initialized_) return;
		pots.scan();
	}

	/**
	 * @brief Reports `Pots` initialization state.
	 * @return `true` when `init_pots()` succeeded.
	 */
	bool is_pots_initialized() const {
		return pots_initialized_;
	}

	Pots pots{};
#endif

#if BRAIN_CFG_MIDI_PARSER
	/**
	 * @brief Initializes `MidiParser` UART input.
	 * @param baud_rate Serial baud rate in bits per second.
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if already initialized,
	 * or `BrainInitStatus::kFailed` if UART init fails.
	 */
	BrainInitStatus init_midi_parser(uint32_t baud_rate = 31250) {
		if (midi_parser_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (!midi_parser.init_uart(baud_rate)) return BrainInitStatus::kFailed;
		midi_parser_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	/**
	 * @brief Reports `MidiParser` initialization state.
	 * @return `true` when `init_midi_parser()` succeeded.
	 */
	bool is_midi_parser_initialized() const {
		return midi_parser_initialized_;
	}

	MidiParser midi_parser{};
#endif

#if BRAIN_CFG_MIDI_TO_CV
	/**
	 * @brief Initializes `MidiToCV` plus its required dependencies (`Outputs` and `MidiParser`).
	 * @param cv_channel Pitch CV output channel (`kOutputsChannelA` or `kOutputsChannelB`).
	 * @param midi_channel MIDI channel filter in 1..16 format.
	 * @param baud_rate Serial baud rate in bits per second.
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if already initialized,
	 * or `BrainInitStatus::kFailed` when dependency init or `midi_to_cv.init(...)` fails.
	 */
	BrainInitStatus init_midi_to_cv(
		AudioCvOutChannel cv_channel = kOutputsChannelA,
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

	/**
	 * @brief Calls `midi_to_cv.update()` if initialized.
	 */
	void update_midi_to_cv() {
		if (!midi_to_cv_initialized_) return;
		midi_to_cv.update();
	}

	/**
	 * @brief Reports `MidiToCV` initialization state.
	 * @return `true` when `init_midi_to_cv()` succeeded.
	 */
	bool is_midi_to_cv_initialized() const {
		return midi_to_cv_initialized_;
	}

	MidiToCV midi_to_cv{};
#endif

#if BRAIN_CFG_POT_MULTI_FUNCTION
	/**
	 * @brief Initializes `PotMultiFunction`, ensuring `Pots` is available.
	 *
	 * Coexists freely with `AudioProcessor`, `Inputs`, and `Pots` — all components
	 * share the internal `AdcEngine` singleton.
	 *
	 * @param max_functions Maximum logical function slots (`PotMultiFunction` clamps to its hard limit).
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if already initialized,
	 * or `BrainInitStatus::kFailed` on dependency init failure.
	 */
	BrainInitStatus init_pot_multi(uint8_t max_functions = PotMultiFunction::kMaxFunctions) {
		if (pot_multi_initialized_) return BrainInitStatus::kAlreadyInitialized;
		if (init_pots() == BrainInitStatus::kFailed) return BrainInitStatus::kFailed;
		pot_multi.init(max_functions);
		pot_multi_initialized_ = true;
		return BrainInitStatus::kOk;
	}

	/**
	 * @brief Updates `PotMultiFunction` using buffered pot data.
	 * @param perform_scan `true` scans pots before updating mappings; `false` consumes already buffered values.
	 */
	void update_pot_multi(bool perform_scan = false) {
		if (!pot_multi_initialized_) return;
		pot_multi.update_buffered(pots, perform_scan);
	}

	/**
	 * @brief Updates `PotMultiFunction` using direct one-shot pot reads.
	 */
	void update_pot_multi_single() {
		if (!pot_multi_initialized_) return;
		pot_multi.update_single(pots);
	}

	/**
	 * @brief Resets pot-multi pickup/value-scale state after page/mode changes.
	 * @param clear_active_mappings `true` clears active function assignments, `false` preserves current assignments.
	 */
	void reset_pot_multi_for_mode_change(bool clear_active_mappings = false) {
		if (!pot_multi_initialized_) return;
		pot_multi.reset_for_mode_change(clear_active_mappings);
	}

	/**
	 * @brief Reports `PotMultiFunction` initialization state.
	 * @return `true` when `init_pot_multi()` succeeded.
	 */
	bool is_pot_multi_initialized() const {
		return pot_multi_initialized_;
	}

	PotMultiFunction pot_multi{};
#endif

#if BRAIN_CFG_AUDIO_PROCESSOR
	/**
	 * @brief Initializes `AudioProcessor` for audio-rate DSP processing on the shared engines.
	 *
	 * Coexists freely with `Inputs`, `Pots`, and `PotMultiFunction` — all components
	 * share the internal `AdcEngine` and `OutputEngine` singletons.
	 *
	 * @param config `AudioProcessorConfig` forwarded to `audio_processor.init(...)`.
	 * @param process_sample_fn Per-sample DSP callback that receives input sample and control frame.
	 * @param user_ctx User context pointer forwarded unchanged to each callback invocation.
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if audio processor is already initialized,
	 * or `BrainInitStatus::kFailed` on processor init failure.
	 */
	BrainInitStatus init_audio_processor(
		const AudioProcessorConfig& config,
		ProcessSampleFn process_sample_fn,
		void* user_ctx = nullptr) {
		if (audio_processor.is_initialized()) return BrainInitStatus::kAlreadyInitialized;
		return audio_processor.init(config, process_sample_fn, user_ctx);
	}

	/**
	 * @brief Initializes `AudioProcessor` with the v2 dual-channel API.
	 *
	 * Coexists freely with `Inputs`, `Pots`, and `PotMultiFunction` — all components
	 * share the internal `AdcEngine` and `OutputEngine` singletons.
	 *
	 * @param config `AudioProcessorConfigV2` with sample_rate_hz and per-channel claim flags.
	 * @param process_frame_fn Dual-input/dual-output DSP callback.
	 * @param user_ctx Opaque pointer passed through to each callback invocation.
	 * @return `BrainInitStatus::kOk` on success,
	 * `BrainInitStatus::kAlreadyInitialized` if audio processor is already initialized,
	 * or `BrainInitStatus::kFailed` on processor init failure.
	 */
	BrainInitStatus init_audio_processor_v2(
		const AudioProcessorConfigV2& config,
		ProcessFrameFnV2 process_frame_fn,
		void* user_ctx = nullptr) {
		if (audio_processor.is_initialized()) return BrainInitStatus::kAlreadyInitialized;
		return audio_processor.init_v2(config, process_frame_fn, user_ctx);
	}

	/**
	 * @brief Reports `AudioProcessor` initialization state.
	 * @return `true` when `audio_processor.is_initialized()` is true.
	 */
	bool is_audio_processor_initialized() const {
		return audio_processor.is_initialized();
	}

	AudioProcessor audio_processor{};
#endif

private:
	// Tracks live `Brain` instances. Function-local static gives one shared
	// counter across all TUs without requiring C++17 inline variables.
	static int& live_instance_count() {
		static int count = 0;
		return count;
	}

	void apply_adc_policy_to_components() {
#if BRAIN_CFG_INPUTS
		inputs.set_audio_cv_dma_enabled(adc_optimization_enabled_ && audio_cv_dma_enabled_);
#endif
#if BRAIN_CFG_POTS
		pots.set_optimized_sampling_enabled(adc_optimization_enabled_ && shared_pot_sampling_enabled_);
#endif
	}

	bool adc_optimization_enabled_ = true;
	bool audio_cv_dma_enabled_ = false;
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
#undef BRAIN_CFG_AUDIO_PROCESSOR
