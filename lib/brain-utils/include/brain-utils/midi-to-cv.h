#ifndef BRAIN_MIDI_TO_CV_H_
#define BRAIN_MIDI_TO_CV_H_

#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "brain-io/audio-cv-out.h"
#include "brain-io/pulse.h"
#include "brain-io/midi-parser.h"

namespace brain::utils {

class MidiToCV {
	public:
		using NoteOnCallback = brain::io::MidiParser::NoteOnCallback;
		using NoteOffCallback = brain::io::MidiParser::NoteOffCallback;

		bool init(brain::io::AudioCvOutChannel cv_channel, uint8_t midi_channel);
		void set_midi_channel(uint8_t midi_channel);
		void set_pitch_channel(brain::io::AudioCvOutChannel cv_channel);

		// Callback functions
		void set_note_on_callback(brain::io::MidiParser::NoteOnCallback callback);
		void set_note_off_callback(brain::io::MidiParser::NoteOffCallback callback);

		// Call this in main loop
		void update();
		bool is_note_playing();

	protected:
		virtual void note_on(uint8_t note, uint8_t velocity, uint8_t channel);
		virtual void note_off(uint8_t note, uint8_t velocity, uint8_t channel);

	private:
		static constexpr uint8_t kNoteStackSize = 25;
		static constexpr uint8_t kZeroCVMidiNote = 24; // 0V CV is mapped to C1

		static MidiToCV* instance_;
		brain::io::AudioCvOutChannel cv_channel_;
		uint8_t midi_channel_;
		brain::io::AudioCvOut dac_;
		brain::io::Pulse gate_;
		bool gate_on_;
		brain::io::MidiParser midi_parser_;
		uint8_t note_stack_[kNoteStackSize];
		uint8_t current_stack_size_;
		uint8_t last_note_;

		static void note_on_callback(uint8_t note, uint8_t velocity, uint8_t channel);
		static void note_off_callback(uint8_t note, uint8_t velocity, uint8_t channel);

		NoteOnCallback note_on_callback_ = nullptr;
		NoteOffCallback note_off_callback_ = nullptr;

		void reset_note_stack();
		void push_note(uint8_t note);
		void pop_note(uint8_t note);
		int find_note(uint8_t note);
		void set_cv();
};

}

#endif