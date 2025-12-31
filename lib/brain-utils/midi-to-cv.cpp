#include "brain-utils/midi-to-cv.h"

namespace brain::utils {

MidiToCV* MidiToCV::instance_ = nullptr;

bool MidiToCV::init(brain::io::AudioCvOutChannel cv_channel, uint8_t midi_channel) {
	instance_ = this;
	midi_channel_ = midi_channel;

	// Set default mode
	set_mode(Mode::kDefault);

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

	// Enable CV
	enable_cv();

	// Init Gate and set to low
	gate_.begin();
	set_gate(false);

	// Set MIDI parser stuff
	midi_parser_.set_channel(midi_channel_);

	midi_parser_.set_note_on_callback(note_on_callback);
	midi_parser_.set_note_off_callback(note_off_callback);
	midi_parser_.set_control_change_callback(control_change_callback);

	if (!midi_parser_.init_uart()) {
		printf("[ERROR] Brain SDK / Midi to CV: MIDI parser failed to initialize.\n");
		return false;
	}

	// Reset note stack & last played note
	reset_note_stack();
	last_note_ = {kZeroCVMidiNote, 0};

	// Modwheel
	modwheel_value_ = 0;

	// Set CV channel
	set_pitch_channel(cv_channel);

	return true;
}

void MidiToCV::set_mode(Mode mode) {
	mode_ = mode;
}

MidiToCV::Mode MidiToCV::get_mode() const {
	return mode_;
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

void MidiToCV::control_change_callback(uint8_t cc, uint8_t value, uint8_t channel) {
	if (instance_) {
		instance_->control_change(cc, value, channel);
	}
}

void MidiToCV::note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
	// Handle velocity 0 as note off
	if (velocity == 0) {
		note_off(note, velocity, channel);
		return;
	}

	// Push note to the note stack
	push_note(note, velocity);

	// Convert MIDI note to voltage
	if (cv_enabled_) {
		set_cv();
	}

	// Set gate high
	set_gate(true);

	// Callback note on
	if (note_on_callback_) {
		note_on_callback_(note, velocity, channel);
	}
}

void MidiToCV::note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
	pop_note(note);

	if (cv_enabled_) {
		set_cv();
	}

	if (current_stack_size_ == 0) {
		set_gate(false);
	}

	// Callback note off
	if (note_off_callback_) {
		note_off_callback_(note, velocity, channel);
	}
}

void MidiToCV::control_change(uint8_t cc, uint8_t value, uint8_t channel) {
	// Modwheel implementation
	if (cc == 1) {
		modwheel_value_ = value;
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
	cv_other_channel_ = cv_channel == brain::io::AudioCvOutChannel::kChannelA ? brain::io::AudioCvOutChannel::kChannelB : brain::io::AudioCvOutChannel::kChannelA;
}

void MidiToCV::update() {
	midi_parser_.process_uart();
}

bool MidiToCV::is_note_playing() {
	return gate_on_;
}

void MidiToCV::push_note(uint8_t note, uint8_t velocity) {
	if (find_note(note) != -1) return;

	// Push the new note in the stack
	if (current_stack_size_ < kNoteStackSize) {
		note_stack_[current_stack_size_] = {note, velocity};
		current_stack_size_++;
	}
}

void MidiToCV::pop_note(uint8_t note) {
	int note_index = find_note(note);
	if (note_index == -1) return;

	for (size_t i = note_index; i < current_stack_size_ - 1; i++) {
		note_stack_[i] = note_stack_[i + 1];
	}

	note_stack_[current_stack_size_ - 1] = {255, 0};
	current_stack_size_--;
}

/**
 * Return -1 if note can't be found
 */
int MidiToCV::find_note(uint8_t note) {
	for (size_t i = 0; i < kNoteStackSize; i++) {
		if (note_stack_[i].note == note) {
			return i;
		}
		if (note_stack_[i].note == 255) {
			return -1;
		}
	}
	return -1;
}

void MidiToCV::reset_note_stack() {
	current_stack_size_ = 0;
	for (size_t i = 0; i < kNoteStackSize; i++) {
		// Use 255 as default value for each note in the stack because MIDI notes go up only until 127
		note_stack_[i] = {255, 0};
	}
}

void MidiToCV::set_cv() {
	NoteVelocity play_note;

	// Keep last note on the CV output even after releasing all keys
	if (current_stack_size_ > 0) {
		play_note = note_stack_[current_stack_size_ - 1];
		last_note_ = play_note;
	} else {
		play_note = last_note_;
	}

	float note_voltage = (play_note.note - kZeroCVMidiNote) / 12.0f;
	dac_.set_voltage(cv_channel_, note_voltage);

	// Handling modes
	switch (mode_) {
		case kUnison: {
			dac_.set_voltage(cv_other_channel_, note_voltage);
			break;
		}

		case kModWheel: {
			float velocity_voltage = midi_value_to_voltage(modwheel_value_);
			dac_.set_voltage(cv_other_channel_, velocity_voltage);
		}

		default: {
			float velocity_voltage = midi_value_to_voltage(play_note.velocity);
			dac_.set_voltage(cv_other_channel_, velocity_voltage);
			break;
		}
	}
}

void MidiToCV::set_gate(bool state) {
	gate_.set(state);
	gate_on_ = state;
}

void MidiToCV::enable_cv() {
	cv_enabled_ = true;
}

void MidiToCV::disable_cv() {
	cv_enabled_ = false;
}

float MidiToCV::midi_value_to_voltage(uint8_t value) {
	return value * brain::io::AudioCvOut::kMaxVoltage / 127.0;
}

}