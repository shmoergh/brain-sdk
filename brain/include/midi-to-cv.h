#ifndef BRAIN_MIDI_TO_CV_H_
#define BRAIN_MIDI_TO_CV_H_

#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "outputs.h"
#include "midi-parser.h"
#include "helpers.h"
#include "init-status.h"

namespace brain::utils {

class MidiToCV {
public:
	/**
	 * @brief Creates a `MidiToCV` utility that maps incoming MIDI events to Brain CV outputs and gate pulse.
	 */
	MidiToCV() = default;

	/**
	 * @brief Releases resources owned by `MidiToCV`.
	 */
	~MidiToCV();

	/**
	 * @brief Copy construction is disabled for this type.
	 */
	MidiToCV(const MidiToCV&) = delete;

	/**
	 * @brief Copy assignment is disabled for this type.
	 */
	MidiToCV& operator=(const MidiToCV&) = delete;

	/**
	 * @brief Move construction is disabled for this type.
	 */
	MidiToCV(MidiToCV&&) = delete;

	/**
	 * @brief Move assignment is disabled for this type.
	 */
	MidiToCV& operator=(MidiToCV&&) = delete;

	enum Mode {
		kDefault = 0, 	// Pitch on selected channel, velocity on the other
		kModWheel = 1, 	// Pitch on selected channel, modwheel on the other
		kUnison = 2,	// Pitch on both channel
		kDuo = 3		// Duophonic mode with last-two-note priority and latched duo fallback
	};

	/**
	 * @brief Processes pending MIDI UART data through the linked `MidiParser`.
	 *
	 * Call this regularly in the main loop to keep CV and gate output responsive.
	 */
	void update();

	/**
	 * @brief Sets how the second CV channel is derived from MIDI data.
	 * @param mode Conversion mode:
	 * - `kMidiToCVModeDefault`: pitch on selected pitch channel, velocity on the other channel.
	 * - `kMidiToCVModeModWheel`: pitch on selected channel, CC1 (mod wheel) on the other.
	 * - `kMidiToCVModeUnison`: same pitch CV on both channels.
	 * - `kMidiToCVModeDuo`: two-note duophonic behavior with latched fallback when notes are released.
	 */
	void set_mode(Mode mode);

	/**
	 * @brief Returns current MIDI-to-CV conversion mode.
	 * @return One of the `kMidiToCVMode*` constants.
	 */
	Mode get_mode() const;

	/**
	 * @brief Injects external `Outputs` and `MidiParser` dependencies.
	 * @param outputs Pointer to initialized `Outputs` used for CV and gate writes.
	 * @param midi_parser Pointer to initialized `MidiParser` used to receive MIDI events.
	 */
	void set_dependencies(Outputs* outputs, MidiParser* midi_parser);

	/**
	 * @brief Initializes MIDI-to-CV routing and attaches parser callbacks.
	 * @param cv_channel CV channel used for pitch output (`kOutputsChannelA` or `kOutputsChannelB`).
	 * The other channel is used for velocity/modwheel/duo-secondary output.
	 * @param midi_channel MIDI input channel filter in 1..16 format.
	 * @return `BrainInitStatus::kOk` when setup succeeded, `BrainInitStatus::kAlreadyInitialized` if called
	 * again after success, or `BrainInitStatus::kFailed` when dependencies are missing/not initialized or
	 * another active `MidiToCV` instance already exists.
	 */
	BrainInitStatus init(AudioCvOutChannel cv_channel, uint8_t midi_channel);

	/**
	 * @brief Reports whether `MidiToCV` completed initialization.
	 * @return `true` after successful `init(...)`.
	 */
	bool is_initialized() const;

	/**
	 * @brief Changes MIDI channel filter used by this converter.
	 * @param midi_channel MIDI channel in 1..16 format, forwarded to `MidiParser::set_channel()`.
	 */
	void set_midi_channel(uint8_t midi_channel);

	/**
	 * @brief Selects which CV output channel carries pitch.
	 * @param cv_channel Pitch channel (`kOutputsChannelA` or `kOutputsChannelB`).
	 * The non-selected channel becomes the secondary CV output.
	 */
	void set_pitch_channel(AudioCvOutChannel cv_channel);

	// Callback functions
	using NoteOnCallback = MidiParser::NoteOnCallback;
	using NoteOffCallback = MidiParser::NoteOffCallback;
	using ControlChangeCallback = MidiParser::ControlChangeCallback;

	/**
	 * @brief Registers callback called after `MidiToCV` handles a Note On.
	 * @param callback Handler with `(note, velocity, channel)` values from parsed MIDI input.
	 */
	void set_note_on_callback(MidiParser::NoteOnCallback callback);

	/**
	 * @brief Registers callback called after `MidiToCV` handles a Note Off.
	 * @param callback Handler with `(note, velocity, channel)` values from parsed MIDI input.
	 */
	void set_note_off_callback(MidiParser::NoteOffCallback callback);

	/**
	 * @brief Registers callback called after `MidiToCV` handles a Control Change message.
	 * @param callback Handler with `(cc, value, channel)` from parsed MIDI input.
	 */
	void set_control_change_callback(MidiParser::ControlChangeCallback callback);

	/**
	 * @brief Clears the active note stack and resets duo latch state.
	 */
	void reset_note_stack();

	/**
	 * @brief Sets MIDI gate state and forwards it to `Outputs::pulse_set()`.
	 * @param state `true` sets gate high, `false` sets gate low.
	 */
	void set_gate(bool state);

	/**
	 * @brief Reports whether gate is currently high.
	 * @return `true` when at least one note is currently active (or gate has been forced high).
	 */
	bool is_note_playing();

	/**
	 * @brief Sets the maximum voltage used for velocity/modwheel secondary CV mapping.
	 * @param max_voltage Maximum output span in volts for 0..127 MIDI values.
	 * Value is clamped internally to 0..10 V.
	 */
	void set_max_cc_voltage(uint8_t max_voltage);

	/**
	 * @brief Enables CV output writes on incoming MIDI events.
	 */
	void enable_cv();

	/**
	 * @brief Disables CV output writes while still parsing MIDI/gate events.
	 */
	void disable_cv();

	/**
	 * @brief Enables calibrated CV output mode.
	 * @param load_from_flash `true` reloads calibration from `Storage` before enabling.
	 * `false` keeps currently loaded calibration offsets.
	 * @return `true` when dependencies are ready and calibration is available (or reload disabled),
	 * `false` when dependencies are missing or calibration load fails.
	 */
	bool enable_calibrated_output(bool load_from_flash = true);

	/**
	 * @brief Disables calibration compensation and writes raw millivolt mapping.
	 */
	void disable_calibrated_output();

	/**
	 * @brief Injects CV calibration table directly and enables calibrated output mode on success.
	 * @param calibration Calibration structure with channel A/B 1V step DAC offsets.
	 * @return `true` when dependencies are ready and `Outputs` accepted calibration data.
	 */
	bool set_cv_calibration(const CvCalibrationV1& calibration);

	/**
	 * @brief Reports whether calibrated output writes are enabled.
	 * @return `true` when `MidiToCV` currently uses `Outputs::set_voltage_calibrated_millivolts(...)`.
	 */
	bool is_calibrated_output_enabled() const;

	protected:
		virtual void note_on(uint8_t note, uint8_t velocity, uint8_t channel);
		virtual void note_off(uint8_t note, uint8_t velocity, uint8_t channel);
		virtual void control_change(uint8_t cc, uint8_t value, uint8_t channel);
		virtual void pitch_bend(int16_t value, uint8_t channel);

	private:
		static constexpr uint8_t kNoteStackSize = 25;
		static constexpr uint8_t kZeroCVMidiNote = 24; // 0V CV is mapped to C1
		static constexpr int16_t kPitchBendMin = -8192;
		static constexpr int16_t kPitchBendMax = 8191;
		static constexpr uint8_t kDefaultPitchBendRangeSemitones = 2;

		struct NoteVelocity {
			uint8_t note;
			uint8_t velocity;
		};

		static MidiToCV* instance_;
		MidiParser* midi_parser_ = nullptr;

		Mode mode_ = Mode::kDefault;

		bool cv_enabled_ = false;
			AudioCvOutChannel cv_channel_ = kOutputsChannelA;
			AudioCvOutChannel cv_other_channel_ = kOutputsChannelB;
		uint8_t midi_channel_ = 1;
		Outputs* outputs_ = nullptr;
		bool gate_on_ = false;
		bool initialized_ = false;

		NoteVelocity note_stack_[kNoteStackSize];
		uint8_t current_stack_size_ = 0;
		NoteVelocity last_note_ = {kZeroCVMidiNote, 0};
		NoteVelocity duo_latched_primary_note_ = {kZeroCVMidiNote, 0};
		NoteVelocity duo_latched_secondary_note_ = {kZeroCVMidiNote, 0};
		uint8_t duo_prev_stack_size_ = 0;

		uint8_t modwheel_value_ = 0;
		int16_t pitch_bend_value_ = 0;
		uint8_t pitch_bend_range_semitones_ = kDefaultPitchBendRangeSemitones;
		bool calibrated_output_enabled_ = false;

		static void note_on_callback(uint8_t note, uint8_t velocity, uint8_t channel);
		static void note_off_callback(uint8_t note, uint8_t velocity, uint8_t channel);
		static void control_change_callback(uint8_t cc, uint8_t value, uint8_t channel);
		static void pitch_bend_callback(int16_t value, uint8_t channel);

		NoteOnCallback note_on_callback_ = nullptr;
		NoteOffCallback note_off_callback_ = nullptr;
		ControlChangeCallback control_change_callback_ = nullptr;

		void push_note(uint8_t note, uint8_t velocity);
		void pop_note(uint8_t note);
		int find_note(uint8_t note);

		uint8_t max_cc_voltage_ = 10;
		void set_cc_cv(int32_t cc_millivolts);
		bool write_cv_millivolts(AudioCvOutChannel channel, int32_t millivolts);
		bool dependencies_ready() const;
		void attach_parser_callbacks();
		void detach_parser_callbacks();
		Outputs& outputs();
		const Outputs& outputs() const;
		MidiParser& midi_parser();
		const MidiParser& midi_parser() const;

		void set_cv();
		int32_t pitch_bend_to_millivolts(int16_t value) const;
		int32_t midi_value_to_millivolts(uint8_t value);
	};

}

using MidiToCV = brain::utils::MidiToCV;

// Convenience aliases for less verbose mode selection.
constexpr MidiToCV::Mode kMidiToCVModeDefault = MidiToCV::Mode::kDefault;
constexpr MidiToCV::Mode kMidiToCVModeModWheel = MidiToCV::Mode::kModWheel;
constexpr MidiToCV::Mode kMidiToCVModeUnison = MidiToCV::Mode::kUnison;
constexpr MidiToCV::Mode kMidiToCVModeDuo = MidiToCV::Mode::kDuo;

#endif
