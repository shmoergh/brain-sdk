#ifndef BRAIN_MIDI_TO_CV_H_
#define BRAIN_MIDI_TO_CV_H_

#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "brain-io/audio-cv-out.h"
#include "brain-io/pulse.h"
#include "brain-io/midi-parser.h"
#include "brain-utils/helpers.h"

namespace brain::utils {

class MidiToCV {
	public:
		enum Mode {
			kDefault = 0, 	// Pitch on selected channel, velocity on the other
			kModWheel = 1, 	// Pitch on selected channel, modwheel on the other
			kUnison = 2,	// Pitch on both channel
			kDuo = 3		// Duophonic mode with first note on selected channel
		};

		// Call this in main loop
		void update();

		void set_mode(Mode mode);
		Mode get_mode() const;

		bool init(brain::io::AudioCvOutChannel cv_channel, uint8_t midi_channel);
		void set_midi_channel(uint8_t midi_channel);
		void set_pitch_channel(brain::io::AudioCvOutChannel cv_channel);

		// Callback functions
		using NoteOnCallback = brain::io::MidiParser::NoteOnCallback;
		using NoteOffCallback = brain::io::MidiParser::NoteOffCallback;
		using ControlChangeCallback = brain::io::MidiParser::ControlChangeCallback;

		void set_note_on_callback(brain::io::MidiParser::NoteOnCallback callback);
		void set_note_off_callback(brain::io::MidiParser::NoteOffCallback callback);
		void set_control_change_callback(brain::io::MidiParser::ControlChangeCallback callback);

		void reset_note_stack();

		void set_gate(bool state);
		bool is_note_playing();

		void set_max_cc_voltage(uint8_t max_voltage);

		void enable_cv();
		void disable_cv();

	protected:
		virtual void note_on(uint8_t note, uint8_t velocity, uint8_t channel);
		virtual void note_off(uint8_t note, uint8_t velocity, uint8_t channel);
		virtual void control_change(uint8_t cc, uint8_t value, uint8_t channel);

	private:
		static constexpr uint8_t kNoteStackSize = 25;
		static constexpr uint8_t kZeroCVMidiNote = 24; // 0V CV is mapped to C1

		struct NoteVelocity {
			uint8_t note;
			uint8_t velocity;
		};

		static MidiToCV* instance_;
		brain::io::MidiParser midi_parser_;

		Mode mode_;

		bool cv_enabled_;
		brain::io::AudioCvOutChannel cv_channel_;
		brain::io::AudioCvOutChannel cv_other_channel_;
		uint8_t midi_channel_;
		brain::io::AudioCvOut dac_;

		brain::io::Pulse gate_;
		bool gate_on_;

		NoteVelocity note_stack_[kNoteStackSize];
		uint8_t current_stack_size_;
		NoteVelocity last_note_;

		uint8_t modwheel_value_;

		static void note_on_callback(uint8_t note, uint8_t velocity, uint8_t channel);
		static void note_off_callback(uint8_t note, uint8_t velocity, uint8_t channel);
		static void control_change_callback(uint8_t cc, uint8_t value, uint8_t channel);

		NoteOnCallback note_on_callback_ = nullptr;
		NoteOffCallback note_off_callback_ = nullptr;
		ControlChangeCallback control_change_callback_ = nullptr;

		void push_note(uint8_t note, uint8_t velocity);
		void pop_note(uint8_t note);
		int find_note(uint8_t note);

		uint8_t max_cc_voltage_;
		void set_cc_cv(float cc_voltage);

		void set_cv();

		float midi_value_to_voltage(uint8_t value);
	};

}

#endif