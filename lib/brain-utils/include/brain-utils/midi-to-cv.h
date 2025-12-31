#ifndef BRAIN_MIDI_TO_CV_H_
#define BRAIN_MIDI_TO_CV_H_

#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "brain-io/audio-cv-out.h"
#include "brain-io/pulse.h"
#include "brain-io/midi-parser.h"

namespace brain::utils {

enum Midi2CVMode {
	kDefault = 0, 	// Pitch on selected channel, velocity on the other
	kModWheel = 1, 	// Pitch on selected channel, modwheel on the other
	kUnison = 2,	// Pitch on both channel
	kDuo = 3		// Duophonic mode with first note on selected channel
};

class MidiToCV {
	public:
		// Call this in main loop
		void update();

		void set_mode(Midi2CVMode mode);
		Midi2CVMode get_mode() const;

		using NoteOnCallback = brain::io::MidiParser::NoteOnCallback;
		using NoteOffCallback = brain::io::MidiParser::NoteOffCallback;

		bool init(brain::io::AudioCvOutChannel cv_channel, uint8_t midi_channel);
		void set_midi_channel(uint8_t midi_channel);
		void set_pitch_channel(brain::io::AudioCvOutChannel cv_channel);

		// Callback functions
		void set_note_on_callback(brain::io::MidiParser::NoteOnCallback callback);
		void set_note_off_callback(brain::io::MidiParser::NoteOffCallback callback);

		void reset_note_stack();

		void set_gate(bool state);
		bool is_note_playing();

		void enable_cv();
		void disable_cv();

	protected:
		virtual void note_on(uint8_t note, uint8_t velocity, uint8_t channel);
		virtual void note_off(uint8_t note, uint8_t velocity, uint8_t channel);

	private:
		static constexpr uint8_t kNoteStackSize = 25;
		static constexpr uint8_t kZeroCVMidiNote = 24; // 0V CV is mapped to C1

		struct NoteVelocity {
			uint8_t note;
			uint8_t velocity;
		};

		Midi2CVMode mode_;

		static MidiToCV* instance_;
		brain::io::AudioCvOutChannel cv_channel_;
		brain::io::AudioCvOutChannel cv_other_channel_;
		uint8_t midi_channel_;
		brain::io::AudioCvOut dac_;
		brain::io::Pulse gate_;
		bool gate_on_;
		brain::io::MidiParser midi_parser_;
		NoteVelocity note_stack_[kNoteStackSize];
		uint8_t current_stack_size_;
		NoteVelocity last_note_;
		bool cv_enabled_;

		static void note_on_callback(uint8_t note, uint8_t velocity, uint8_t channel);
		static void note_off_callback(uint8_t note, uint8_t velocity, uint8_t channel);

		NoteOnCallback note_on_callback_ = nullptr;
		NoteOffCallback note_off_callback_ = nullptr;

		void push_note(uint8_t note, uint8_t velocity);
		void pop_note(uint8_t note);
		int find_note(uint8_t note);

		void set_cv();
	};

}

#endif