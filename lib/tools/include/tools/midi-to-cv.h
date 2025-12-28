#ifndef BRAIN_MIDI_TO_CV_H_
#define BRAIN_MIDI_TO_CV_H_

#include <pico/stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "brain-io/audio-cv-out.h"
#include "brain-io/pulse.h"
#include "brain-io/midi-parser.h"

namespace brain::tools {

class MidiToCV {
	public:
		bool init(brain::io::AudioCvOutChannel cv_channel, uint8_t midi_channel);
		void setMidiChannel(uint8_t midi_channel);
		void update();

	private:
		static MidiToCV* instance_;
		brain::io::AudioCvOutChannel cv_channel_;
		uint8_t midi_channel_;
		brain::io::AudioCvOut dac_;
		brain::io::Pulse gate_;
		brain::io::MidiParser midi_parser_;
		static void noteOnCallback(uint8_t note, uint8_t velocity, uint8_t channel);
		void noteOn(uint8_t note, uint8_t velocity, uint8_t channel);
};

}

#endif