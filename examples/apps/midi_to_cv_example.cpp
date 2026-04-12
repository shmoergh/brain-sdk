#include "midi_to_cv_example.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace {

void on_note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
	printf("\nNote On  ch=%u note=%u vel=%u", channel, note, velocity);
}

void on_note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
	printf("\nNote Off ch=%u note=%u vel=%u", channel, note, velocity);
}

}  // namespace

void MidiToCvExample::init() {
	printf("\n--------\n");
	printf("Example: MIDI to CV (channel 1, pitch on output A, duo mode)\n");

	if (!brain_init_succeeded(brain_.init_midi_to_cv(AudioCvOutChannel::kChannelA, 1, 31250))) {
		printf("[ERROR] init_midi_to_cv failed\n");
		return;
	}

	brain_.midi_to_cv.set_mode(MidiToCV::Mode::kDuo);
	brain_.midi_to_cv.set_note_on_callback(on_note_on);
	brain_.midi_to_cv.set_note_off_callback(on_note_off);
	initialized_ = true;
	printf("Ready. Send MIDI notes on channel 1.\n");
}

void MidiToCvExample::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.update_midi_to_cv();
}
