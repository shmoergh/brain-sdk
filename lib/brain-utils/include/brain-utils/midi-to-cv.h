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
		bool init(brain::io::AudioCvOutChannel cv_channel, uint8_t midi_channel);
		void setMidiChannel(uint8_t midi_channel);

		/**
		 * Call in main loop
		 */
		void update();

	private:
		static constexpr uint8_t kNoteStackSize = 25;
		static constexpr uint8_t kZeroCVMidiNote = 24; // 0V CV is mapped to C1

		static MidiToCV* instance_;
		brain::io::AudioCvOutChannel cv_channel_;
		uint8_t midi_channel_;
		brain::io::AudioCvOut dac_;
		brain::io::Pulse gate_;
		brain::io::MidiParser midi_parser_;
		uint8_t note_stack_[kNoteStackSize];
		uint8_t current_stack_size_;
		uint8_t last_note_;

		static void noteOnCallback(uint8_t note, uint8_t velocity, uint8_t channel);
		static void noteOffCallback(uint8_t note, uint8_t velocity, uint8_t channel);
		void noteOn(uint8_t note, uint8_t velocity, uint8_t channel);
		void noteOff(uint8_t note, uint8_t velocity, uint8_t channel);

		void resetNoteStack();
		void pushNote(uint8_t note);
		void popNote(uint8_t note);
		int findNote(uint8_t note);
		void setCV();
};

}

#endif