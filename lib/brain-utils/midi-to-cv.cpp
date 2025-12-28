#include "brain-utils/midi-to-cv.h"

namespace brain::utils {

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
	midi_parser_.setNoteOffCallback(noteOffCallback);
	if (!midi_parser_.initUart()) {
		printf("[ERROR] Brain SDK / Midi to CV: MIDI parser failed to initialize.\n");
		return false;
	}

	// Reset note stack & last played note
	resetNoteStack();
	last_note_ = kZeroCVMidiNote;

	return true;
}

void MidiToCV::noteOnCallback(uint8_t note, uint8_t velocity, uint8_t channel) {
	if (instance_) {
		instance_->noteOn(note, velocity, channel);
	}
}

void MidiToCV::noteOffCallback(uint8_t note, uint8_t velocity, uint8_t channel) {
	if (instance_) {
		instance_->noteOff(note, velocity, channel);
	}
}

void MidiToCV::noteOn(uint8_t note, uint8_t velocity, uint8_t channel) {
	// Handle velocity 0 as note off
	if (velocity == 0) {
		noteOff(note, velocity, channel);
		return;
	}

	// Push note to the note stack
	pushNote(note);

	// Convert MIDI note to voltage
	setCV();

	// Set gate high
	gate_.set(true);
}

void MidiToCV::noteOff(uint8_t note, uint8_t velocity, uint8_t channel) {
	popNote(note);
	setCV();
	if (current_stack_size_ == 0) gate_.set(false);
}

void MidiToCV::setMidiChannel(uint8_t midi_channel) {
	midi_channel_ = midi_channel;
	midi_parser_.setChannel(midi_channel_);
}

void MidiToCV::update() {
	midi_parser_.processUartInput();
}

void MidiToCV::pushNote(uint8_t note) {
	if (findNote(note) != -1) return;

	// Push the new note in the stack
	if (current_stack_size_ < kNoteStackSize) {
		note_stack_[current_stack_size_] = note;
		current_stack_size_++;
	}
}

void MidiToCV::popNote(uint8_t note) {
	int note_index = findNote(note);
	if (note_index == -1) return;

	for (size_t i = note_index; i < current_stack_size_ - 1; i++) {
		note_stack_[i] = note_stack_[i + 1];
	}

	note_stack_[current_stack_size_ - 1] = 255;
	current_stack_size_--;
}

/**
 * Return -1 if note can't be found
 */
int MidiToCV::findNote(uint8_t note) {
	for (size_t i = 0; i < kNoteStackSize; i++) {
		if (note_stack_[i] == note) {
			return i;
		}
		if (note_stack_[i] == 255) {
			return -1;
		}
	}
	return -1;
}

void MidiToCV::resetNoteStack() {
	current_stack_size_ = 0;
	for (size_t i = 0; i < kNoteStackSize; i++) {
		// Use 255 as default value for each note in the stack because MIDI notes go up only until 127
		note_stack_[i] = 255;
	}
}

void MidiToCV::setCV() {
	uint8_t note;

	// Keep last note on the CV output
	if (current_stack_size_ > 0) {
		note = note_stack_[current_stack_size_ - 1];
		last_note_ = note;
	} else {
		note = last_note_;
	}

	float voltage = (note - kZeroCVMidiNote) / 12.0f;
	dac_.setVoltage(cv_channel_, voltage);
}

}