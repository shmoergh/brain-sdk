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
		MidiToCV() = default;
		~MidiToCV();
		MidiToCV(const MidiToCV&) = delete;
		MidiToCV& operator=(const MidiToCV&) = delete;
		MidiToCV(MidiToCV&&) = delete;
		MidiToCV& operator=(MidiToCV&&) = delete;

		enum Mode {
			kDefault = 0, 	// Pitch on selected channel, velocity on the other
			kModWheel = 1, 	// Pitch on selected channel, modwheel on the other
			kUnison = 2,	// Pitch on both channel
			kDuo = 3		// Duophonic mode with last-two-note priority and latched duo fallback
		};

		// Call this in main loop
		void update();

		void set_mode(Mode mode);
		Mode get_mode() const;

		void set_dependencies(Outputs* outputs, MidiParser* midi_parser);
		BrainInitStatus init(AudioCvOutChannel cv_channel, uint8_t midi_channel);
		bool is_initialized() const;
		void set_midi_channel(uint8_t midi_channel);
		void set_pitch_channel(AudioCvOutChannel cv_channel);

		// Callback functions
		using NoteOnCallback = MidiParser::NoteOnCallback;
		using NoteOffCallback = MidiParser::NoteOffCallback;
		using ControlChangeCallback = MidiParser::ControlChangeCallback;

		void set_note_on_callback(MidiParser::NoteOnCallback callback);
		void set_note_off_callback(MidiParser::NoteOffCallback callback);
		void set_control_change_callback(MidiParser::ControlChangeCallback callback);

		void reset_note_stack();

		void set_gate(bool state);
		bool is_note_playing();

		void set_max_cc_voltage(uint8_t max_voltage);

		void enable_cv();
		void disable_cv();
		bool enable_calibrated_output(bool load_from_flash = true);
		void disable_calibrated_output();
		bool set_cv_calibration(const CvCalibrationV1& calibration);
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
		AudioCvOutChannel cv_channel_ = AudioCvOutChannel::kChannelA;
		AudioCvOutChannel cv_other_channel_ = AudioCvOutChannel::kChannelB;
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
