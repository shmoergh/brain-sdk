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
	dac_.set_coupling(brain::io::AudioCvOutChannel::kChannelA, brain::io::AudioCvOutCoupling::kDcCoupled);
	dac_.set_coupling(brain::io::AudioCvOutChannel::kChannelB, brain::io::AudioCvOutCoupling::kDcCoupled);
	dac_.set_voltage(brain::io::AudioCvOutChannel::kChannelA, 0.0f);
	dac_.set_voltage(brain::io::AudioCvOutChannel::kChannelB, 0.0f);

	// Init Gate and set to low
	gate_.begin();
	set_gate(false);

	// Set MIDI parser channel
	midi_parser_.set_channel(midi_channel_);
	midi_parser_.set_note_on_callback(note_on_callback);
	midi_parser_.set_note_off_callback(note_off_callback);
	if (!midi_parser_.init_uart()) {
		printf("[ERROR] Brain SDK / Midi to CV: MIDI parser failed to initialize.\n");
		return false;
	}

	// Reset note stack & last played note
	reset_note_stack();
	last_note_ = kZeroCVMidiNote;

	return true;
}

void MidiToCV::note_on_callback(uint8_t note, uint8_t velocity, uint8_t channel) {
	if (instance_) {
		instance_->note_on(note, velocity, channel);
	}
}

void MidiToCV::note_off_callback(uint8_t note, uint8_t velocity, uint8_t channel) {
	if (instance_) {
		instance_->note_off(note, velocity, channel);
	}
}

void MidiToCV::note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
	// Handle velocity 0 as note off
	if (velocity == 0) {
		note_off(note, velocity, channel);
		return;
	}

	// Push note to the note stack
	push_note(note);

	// Convert MIDI note to voltage
	set_cv();

	// Set gate high
	set_gate(true);

	// Callback note on
	if (note_on_callback_) {
		note_on_callback_(note, velocity, channel);
	}
}

void MidiToCV::note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
	pop_note(note);
	set_cv();
	if (current_stack_size_ == 0) {
		set_gate(false);
	}

	// Callback note off
	if (note_off_callback_) {
		note_off_callback_(note, velocity, channel);
	}
}

void MidiToCV::set_note_on_callback(NoteOnCallback callback) {
	note_on_callback_ = callback;
}

void MidiToCV::set_note_off_callback(NoteOffCallback callback) {
	note_off_callback_ = callback;
}

void MidiToCV::set_midi_channel(uint8_t midi_channel) {
	midi_channel_ = midi_channel;
	midi_parser_.set_channel(midi_channel_);
}

void MidiToCV::set_pitch_channel(brain::io::AudioCvOutChannel cv_channel) {
	dac_.set_voltage(brain::io::AudioCvOutChannel::kChannelA, 0.0f);
	dac_.set_voltage(brain::io::AudioCvOutChannel::kChannelB, 0.0f);
	cv_channel_ = cv_channel;
}

void MidiToCV::update() {
	midi_parser_.process_uart();
}

bool MidiToCV::is_note_playing() {
	return gate_on_;
}

void MidiToCV::push_note(uint8_t note) {
	if (find_note(note) != -1) return;

	// Push the new note in the stack
	if (current_stack_size_ < kNoteStackSize) {
		note_stack_[current_stack_size_] = note;
		current_stack_size_++;
	}
}

void MidiToCV::pop_note(uint8_t note) {
	int note_index = find_note(note);
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
int MidiToCV::find_note(uint8_t note) {
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

void MidiToCV::reset_note_stack() {
	current_stack_size_ = 0;
	for (size_t i = 0; i < kNoteStackSize; i++) {
		// Use 255 as default value for each note in the stack because MIDI notes go up only until 127
		note_stack_[i] = 255;
	}
}

void MidiToCV::set_cv() {
	uint8_t note;

	// Keep last note on the CV output
	if (current_stack_size_ > 0) {
		note = note_stack_[current_stack_size_ - 1];
		last_note_ = note;
	} else {
		note = last_note_;
	}

	float voltage = (note - kZeroCVMidiNote) / 12.0f;
	dac_.set_voltage(cv_channel_, voltage);
}

void MidiToCV::set_gate(bool state) {
	gate_.set(state);
	gate_on_ = state;
}

}