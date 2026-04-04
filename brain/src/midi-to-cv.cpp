#include "midi-to-cv.h"

namespace brain::utils {

MidiToCV* MidiToCV::instance_ = nullptr;

namespace {

int32_t div_round_nearest_i32(int32_t numerator, int32_t denominator) {
	if (denominator == 0) return 0;
	if (numerator >= 0) {
		return (numerator + denominator / 2) / denominator;
	}
	return (numerator - denominator / 2) / denominator;
}

}  // namespace

bool MidiToCV::init(AudioCvOutChannel cv_channel, uint8_t midi_channel) {
	instance_ = this;
	midi_channel_ = midi_channel;

	// Set default mode
	set_mode(Mode::kDefault);

	// Let bits settle
	sleep_ms(200);

	// Init DAC
	if (!outputs_.init()) {
		printf("[ERROR] Brain SDK / Midi to CV: DAC failed to initialize.\n");
		return false;
	}

	// Default CV range is 0..10V on both channels.
	outputs_.set_output_range(AudioCvOutChannel::kChannelA, AudioCvOutRange::kRange0To10V);
	outputs_.set_output_range(AudioCvOutChannel::kChannelB, AudioCvOutRange::kRange0To10V);
	write_cv_millivolts(AudioCvOutChannel::kChannelA, 0);
	write_cv_millivolts(AudioCvOutChannel::kChannelB, 0);

	// Enable CV
	enable_cv();

	// Init Gate and set to low
	set_gate(false);

	// Set MIDI parser stuff
	midi_parser_.set_channel(midi_channel_);

	midi_parser_.set_note_on_callback(note_on_callback);
	midi_parser_.set_note_off_callback(note_off_callback);
	midi_parser_.set_control_change_callback(control_change_callback);
	midi_parser_.set_pitch_bend_callback(pitch_bend_callback);

	if (!midi_parser_.init_uart()) {
		printf("[ERROR] Brain SDK / Midi to CV: MIDI parser failed to initialize.\n");
		return false;
	}

	// Reset note stack & last played note
	reset_note_stack();
	last_note_ = {kZeroCVMidiNote, 0};
	duo_latched_primary_note_ = last_note_;
	duo_latched_secondary_note_ = last_note_;
	duo_prev_stack_size_ = 0;

	// Modwheel
	modwheel_value_ = 0;
	pitch_bend_value_ = 0;
	pitch_bend_range_semitones_ = kDefaultPitchBendRangeSemitones;

	// Set up CV
	max_cc_voltage_ = static_cast<uint8_t>(Outputs::kMaxOutputMillivolts / 1000);
	set_pitch_channel(cv_channel);

	return true;
}

void MidiToCV::set_mode(Mode mode) {
	mode_ = mode;
	if (mode_ == Mode::kDuo) {
		if (current_stack_size_ > 1) {
			duo_latched_primary_note_ = note_stack_[current_stack_size_ - 2];
			duo_latched_secondary_note_ = note_stack_[current_stack_size_ - 1];
		} else if (current_stack_size_ == 1) {
			duo_latched_primary_note_ = note_stack_[0];
			duo_latched_secondary_note_ = note_stack_[0];
		} else {
			duo_latched_primary_note_ = last_note_;
			duo_latched_secondary_note_ = last_note_;
		}
		duo_prev_stack_size_ = current_stack_size_;
	}
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

void MidiToCV::pitch_bend_callback(int16_t value, uint8_t channel) {
	if (instance_) {
		instance_->pitch_bend(value, channel);
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
	(void)channel;

	// Modwheel
	if (cc == 1 && mode_ == Mode::kModWheel) {
		modwheel_value_ = value;
		set_cc_cv(midi_value_to_millivolts(modwheel_value_));
	}
}

void MidiToCV::pitch_bend(int16_t value, uint8_t channel) {
	(void)channel;

	if (value < kPitchBendMin) {
		value = kPitchBendMin;
	} else if (value > kPitchBendMax) {
		value = kPitchBendMax;
	}

	pitch_bend_value_ = value;

	if (cv_enabled_) {
		set_cv();
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

void MidiToCV::set_pitch_channel(AudioCvOutChannel cv_channel) {
	write_cv_millivolts(AudioCvOutChannel::kChannelA, 0);
	write_cv_millivolts(AudioCvOutChannel::kChannelB, 0);

	cv_channel_ = cv_channel;
	cv_other_channel_ = cv_channel == AudioCvOutChannel::kChannelA ? AudioCvOutChannel::kChannelB : AudioCvOutChannel::kChannelA;
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
	duo_latched_primary_note_ = last_note_;
	duo_latched_secondary_note_ = last_note_;
	duo_prev_stack_size_ = 0;
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

	int32_t note_millivolts =
		div_round_nearest_i32(
			(static_cast<int32_t>(play_note.note) - static_cast<int32_t>(kZeroCVMidiNote)) * 1000,
			12);
	note_millivolts += pitch_bend_to_millivolts(pitch_bend_value_);

	int32_t cc_millivolts = 0;

	switch (mode_) {
		case kDuo: {
			if (current_stack_size_ > 1) {
				// Two or more notes: play the newest two held notes.
				duo_latched_primary_note_ = note_stack_[current_stack_size_ - 2];
				duo_latched_secondary_note_ = note_stack_[current_stack_size_ - 1];
			} else if (current_stack_size_ == 1 && duo_prev_stack_size_ == 0) {
				// New phrase after all notes were released: reset both outputs to the new first note.
				duo_latched_primary_note_ = note_stack_[0];
				duo_latched_secondary_note_ = note_stack_[0];
			} else if (current_stack_size_ == 0) {
				// No notes held: keep duo outputs latched until the next phrase starts.
			}

			int32_t primary_note_millivolts =
				div_round_nearest_i32(
					(static_cast<int32_t>(duo_latched_primary_note_.note) - static_cast<int32_t>(kZeroCVMidiNote)) * 1000,
					12);
			int32_t secondary_note_millivolts =
				div_round_nearest_i32(
					(static_cast<int32_t>(duo_latched_secondary_note_.note) - static_cast<int32_t>(kZeroCVMidiNote)) * 1000,
					12);
			primary_note_millivolts += pitch_bend_to_millivolts(pitch_bend_value_);
			secondary_note_millivolts += pitch_bend_to_millivolts(pitch_bend_value_);
			write_cv_millivolts(cv_channel_, primary_note_millivolts);
			set_cc_cv(secondary_note_millivolts);
			duo_prev_stack_size_ = current_stack_size_;
			return;
		}

		case kUnison: {
			cc_millivolts = note_millivolts;
			break;
		}

		case kModWheel: {
			cc_millivolts = midi_value_to_millivolts(modwheel_value_);
			break;
		}

		default: {
			cc_millivolts = midi_value_to_millivolts(play_note.velocity);
			break;
		}
	}

	write_cv_millivolts(cv_channel_, note_millivolts);
	set_cc_cv(cc_millivolts);
	duo_prev_stack_size_ = current_stack_size_;
}

void MidiToCV::set_cc_cv(int32_t cc_millivolts) {
	// Handling modes
	write_cv_millivolts(cv_other_channel_, cc_millivolts);
}

void MidiToCV::set_gate(bool state) {
	outputs_.pulse_set(state);
	gate_on_ = state;
}

void MidiToCV::enable_cv() {
	cv_enabled_ = true;
}

void MidiToCV::disable_cv() {
	cv_enabled_ = false;
}

bool MidiToCV::enable_calibrated_output(bool load_from_flash) {
	if (load_from_flash && !outputs_.load_calibration_from_flash()) {
		calibrated_output_enabled_ = false;
		return false;
	}

	calibrated_output_enabled_ = true;
	return true;
}

void MidiToCV::disable_calibrated_output() {
	calibrated_output_enabled_ = false;
}

bool MidiToCV::set_cv_calibration(const CvCalibrationV1& calibration) {
	outputs_.set_calibration(calibration);
	calibrated_output_enabled_ = true;
	return true;
}

bool MidiToCV::is_calibrated_output_enabled() const {
	return calibrated_output_enabled_;
}

bool MidiToCV::write_cv_millivolts(AudioCvOutChannel channel, int32_t millivolts) {
	if (calibrated_output_enabled_) {
		return outputs_.set_voltage_calibrated_millivolts(channel, millivolts);
	}
	return outputs_.set_voltage_millivolts(channel, millivolts);
}

void MidiToCV::set_max_cc_voltage(uint8_t max_voltage) {
	max_cc_voltage_ =
		clamp(0, static_cast<int32_t>(Outputs::kMaxOutputMillivolts / 1000), max_voltage);
}

int32_t MidiToCV::midi_value_to_millivolts(uint8_t value) {
	const int32_t max_millivolts = static_cast<int32_t>(max_cc_voltage_) * 1000;
	return div_round_nearest_i32(static_cast<int32_t>(value) * max_millivolts, 127);
}

int32_t MidiToCV::pitch_bend_to_millivolts(int16_t value) const {
	if (value == 0) {
		return 0;
	}

	// Use integer fixed-point math (milli-semitones) and only convert to float at the end.
	static constexpr int32_t kMilliSemitoneScale = 1000;
	const int32_t bend_denominator = (value > 0) ? kPitchBendMax : -kPitchBendMin;
	const int64_t scaled_numerator =
		static_cast<int64_t>(value) *
		static_cast<int64_t>(pitch_bend_range_semitones_) *
		kMilliSemitoneScale;
	const int32_t bend_milli_semitones = static_cast<int32_t>(scaled_numerator / bend_denominator);

	// 1 octave = 1000 mV = 12 semitones => 1 semitone = 1000/12 mV.
	return div_round_nearest_i32(bend_milli_semitones, 12);
}

}
