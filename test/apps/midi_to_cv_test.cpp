/*
               ███████████████████         ██
		   ███████████████████████████   ██  ██
		 ███████████████████████████████     ████
████     ████████████████████████████████  ████
████████ ████████████████████████████████████
███████████████████████████████████████████
	███████████████████████████████████████
	███████████████████████████████████████
	███████████████████████████████████████
  ████  █  ████████████████████████████████
███████████████████████████████████████████
███████████████████████████████████████████
███████████████████████████████████████████
	███████████████████████████████████████
			   █████████████      ███████
		 ████  ██████       ████    █████
		 ████  ████         ████    █████
		███    ████       ████      ████
		███    ██         ██        ██

		SHMØERGH / www.shmoergh.com
 */

#include "midi_to_cv_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace {

constexpr uint8_t kMidiChannel = 1;
constexpr AudioCvOutChannel kPitchCvChannel = AudioCvOutChannel::kChannelA;
constexpr MidiToCV::Mode kMode = MidiToCV::Mode::kDuo;

void on_note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
	printf("Note On  | ch=%u note=%u vel=%u\n", channel, note, velocity);
}

void on_note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
	printf("Note Off | ch=%u note=%u vel=%u\n", channel, note, velocity);
}

}  // namespace

namespace sandbox::apps {

void MidiToCvTest::init() {
	stdio_init_all();

	printf("\n\r--------\n\r");
	printf("MIDI to CV Sandbox\n");
	printf("Hardcoded config (edit constants in source): MIDI channel=%u, pitch channel=A, mode=%u\n",
		kMidiChannel,
		static_cast<unsigned>(kMode));

	BrainInitStatus status = brain_.init_midi_to_cv(kPitchCvChannel, kMidiChannel);
	initialized_ = brain_init_succeeded(status);
	if (!initialized_) {
		printf("[ERROR] MidiToCV init failed.\n");
		return;
	}

	brain_.midi_to_cv.set_mode(kMode);
	brain_.midi_to_cv.set_note_on_callback(on_note_on);
	brain_.midi_to_cv.set_note_off_callback(on_note_off);

	printf("Ready. Send MIDI notes on channel %u.\n", kMidiChannel);
}

void MidiToCvTest::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.update_midi_to_cv();
}

}  // namespace sandbox::apps
