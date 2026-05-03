#include <stdio.h>

#include "pico/stdlib.h"

#include "apps/audio_passthrough_test.h"
#include "apps/audio_passthrough_v2_test.h"
#include "apps/audio_processor_test.h"
#include "apps/basic_pot_reads_test.h"
#include "apps/inputs_and_pots_test.h"
#include "apps/leds_test.h"
#include "apps/midi_to_cv_test.h"
#include "apps/multipot_test.h"
#include "apps/output_engine_timing_test.h"
#include "apps/output_ownership_test.h"
#include "apps/storage_persistence_check_test.h"
#include "apps/storage_test.h"

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
	printf("========================================\n");
	printf("Brain SDK - Critical Issues Test Package\n");
	printf("========================================\n\n");
	printf("Select a test app:\n");
	printf("  1) Basic pot reads (engine-driven, buffered)\n");
	printf("  2) Multi-function pot behavior\n");
	printf("  3) MIDI to CV path\n");
	printf("  4) Storage write/read smoke test\n");
	printf("  5) Storage persistence check\n");
	printf("  6) LED full manual suite\n");
	printf("  7) AudioProcessor effect + guardrails\n");
	printf("  8) Inputs + Pots concurrent (engine-shared)\n");
	printf("  9) OutputEngine timing (Slice 1)\n");
	printf(" 10) Output ownership (Slice 3)\n");
	printf(" 11) Audio passthrough (IN1 -> OUT A, simplest)\n");
	printf(" 12) Audio passthrough V2 (stereo: IN1 -> A, IN2 -> B)\n");
	printf("\n");
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
				run_selected_app<sandbox::apps::BasicPotReadsTest>();
			case 2:
				run_selected_app<sandbox::apps::MultipotTest>();
			case 3:
				run_selected_app<sandbox::apps::MidiToCvTest>();
			case 4:
				run_selected_app<sandbox::apps::StorageTest>();
			case 5:
				run_selected_app<sandbox::apps::StoragePersistenceCheckTest>();
			case 6:
				run_selected_app<sandbox::apps::LedsTest>();
			case 7:
				run_selected_app<sandbox::apps::AudioProcessorTest>();
			case 8:
				run_selected_app<sandbox::apps::InputsAndPotsTest>();
			case 9:
				run_selected_app<sandbox::apps::OutputEngineTimingTest>();
			case 10:
				run_selected_app<sandbox::apps::OutputOwnershipTest>();
			case 11:
				run_selected_app<sandbox::apps::AudioPassthroughTest>();
			case 12:
				run_selected_app<sandbox::apps::AudioPassthroughV2Test>();
			default:
				printf("\nInvalid selection: %d\n", selection);
				sleep_ms(900);
				break;
		}
	}
}
