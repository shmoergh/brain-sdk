#include "midi_parser_example.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace {

void on_note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
	printf("\nNote On  ch=%u note=%u vel=%u", channel, note, velocity);
}

void on_note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
	printf("\nNote Off ch=%u note=%u vel=%u", channel, note, velocity);
}

void on_cc(uint8_t cc, uint8_t value, uint8_t channel) {
	printf("\nCC      ch=%u cc=%u value=%u", channel, cc, value);
}

void on_pitch_bend(int16_t value, uint8_t channel) {
	printf("\nPitch   ch=%u value=%d", channel, static_cast<int>(value));
}

}  // namespace

namespace examples::apps {

void MidiParserExample::init() {
	printf("\n--------\n");
	printf("Example: MIDI parser monitor (channel 1)\n");

	if (!brain_init_succeeded(brain_.init_midi_parser(31250))) {
		printf("[ERROR] init_midi_parser failed\n");
		return;
	}

	brain_.midi_parser.set_note_on_callback(on_note_on);
	brain_.midi_parser.set_note_off_callback(on_note_off);
	brain_.midi_parser.set_control_change_callback(on_cc);
	brain_.midi_parser.set_pitch_bend_callback(on_pitch_bend);

	initialized_ = true;
	printf("Ready. Send MIDI on channel 1.\n");
}

void MidiParserExample::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.midi_parser.process_uart_budgeted(64);
}

}  // namespace examples::apps
