#include "tools/midi-to-cv.h"

namespace brain::tools {

MidiToCV* MidiToCV::instance_ = nullptr;

bool MidiToCV::init(brain::io::AudioCvOutChannel cv_channel, uint8_t midi_channel) {
	instance_ = this;

	cv_channel_ = cv_channel;
	midi_channel_ = midi_channel;

	// Let bits settle
	sleep_ms(200);

	// Init DAC
	if (!dac_.init()) {
		printf("[ERROR] Brain SDK / Midi to CV: DAC failed to initialize.\n");
		return false;
	}

	// DC couple the CV output and set it to 0.0f
	dac_.setCoupling(cv_channel_, brain::io::AudioCvOutCoupling::kDcCoupled);
	dac_.setVoltage(cv_channel_, 0.0f);

	// Init Gate and set to low
	gate_.begin();
	gate_.set(false);

	// Set MIDI parser channel
	midi_parser_.setChannel(midi_channel_);
	midi_parser_.setNoteOnCallback(noteOnCallback);

	return true;
}

void MidiToCV::noteOnCallback(uint8_t note, uint8_t velocity, uint8_t channel) {
	if (instance_) {
		instance_->noteOn(note, velocity, channel);
	}
}

void MidiToCV::noteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
	// handle note on
}

void MidiToCV::setMidiChannel(uint8_t midi_channel) {
	midi_channel_ = midi_channel;
	midi_parser_.setChannel(midi_channel_);
}

void MidiToCV::update() {

}

}