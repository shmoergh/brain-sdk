#include <stdio.h>

#include "pico/stdlib.h"

#include "apps/audio_processor_example.h"
#include "apps/inputs_example.h"
#include "apps/io_passthrough_example.h"
#include "apps/leds_example.h"
#include "apps/midi_parser_example.h"
#include "apps/midi_to_cv_example.h"
#include "apps/pots_multi_example.h"
#include "apps/storage_example.h"

namespace {

template <typename TApp>
[[noreturn]] void run_selected_app() {
	TApp app;
	app.init();
	while (true) {
		app.update();
		sleep_ms(1);
	}
}

void print_menu() {
	printf("\033[2J\033[H");
	printf("==================================\n");
	printf("Brain SDK - Examples Firmware\n");
	printf("==================================\n\n");
	printf("Select an example app:\n");
	printf("  1) LEDs running light\n");
	printf("  2) Inputs monitor\n");
	printf("  3) Input->Output passthrough\n");
	printf("  4) Pots + PotMultiFunction\n");
	printf("  5) MIDI parser monitor\n");
	printf("  6) MIDI to CV\n");
	printf("  7) Storage app blob demo\n");
	printf("  8) AudioProcessor lowpass demo\n\n");
	printf("Enter number then press Enter.\n");
	printf("> ");
	fflush(stdout);
}

int read_selection() {
	char line[16] = {0};
	size_t idx = 0;

	while (true) {
		int ch = getchar_timeout_us(1000);
		if (ch == PICO_ERROR_TIMEOUT) {
			continue;
		}

		if (ch == '\r' || ch == '\n') {
			if (idx == 0) {
				continue;
			}
			line[idx] = '\0';
			break;
		}

		if (idx < sizeof(line) - 1 && ch >= 32 && ch <= 126) {
			line[idx++] = static_cast<char>(ch);
			putchar(ch);
		}
	}
	printf("\n");

	int value = 0;
	for (size_t i = 0; line[i] != '\0'; ++i) {
		if (line[i] < '0' || line[i] > '9') {
			return -1;
		}
		value = value * 10 + (line[i] - '0');
	}
	return value;
}

}  // namespace

int main() {
	stdio_init_all();
	sleep_ms(1200);

	while (true) {
		print_menu();
		const int selection = read_selection();

		switch (selection) {
			case 1:
				run_selected_app<examples::apps::LedsExample>();
			case 2:
				run_selected_app<examples::apps::InputsExample>();
			case 3:
				run_selected_app<examples::apps::IoPassthroughExample>();
			case 4:
				run_selected_app<examples::apps::PotsMultiExample>();
			case 5:
				run_selected_app<examples::apps::MidiParserExample>();
			case 6:
				run_selected_app<examples::apps::MidiToCvExample>();
			case 7:
				run_selected_app<examples::apps::StorageExample>();
			case 8:
				run_selected_app<examples::apps::AudioProcessorExample>();
			default:
				printf("\nInvalid selection: %d\n", selection);
				sleep_ms(900);
				break;
		}
	}
}
