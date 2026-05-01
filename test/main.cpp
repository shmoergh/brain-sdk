#include <stdio.h>

#include "pico/stdlib.h"

#include "apps/adc_engine_test.h"
#include "apps/leds_test.h"
#include "apps/midi_to_cv_test.h"
#include "apps/multipot_test.h"
#include "apps/audio_dual_stream_test.h"
#include "apps/audio_processor_test.h"
#include "apps/audio_volume_test.h"
#include "apps/pot_cross_bleed_test.h"
#include "apps/pot_live_monitor_test.h"
#include "apps/pot_read_stability_test.h"
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
	printf("  1) Pot read stability regression (mux bleed)\n");
	printf("  2) Multi-function pot behavior\n");
	printf("  3) MIDI to CV path\n");
	printf("  4) Storage write/read smoke test\n");
	printf("  5) Storage persistence check\n");
	printf("  6) LED full manual suite\n");
	printf("  7) AudioProcessor effect + guardrails\n");
	printf("  8) AdcEngine 2.1 unified ADC test\n");
	printf("  9) Pot live monitor (turn knobs to verify)\n");
	printf(" 10) Audio volume (In1 -> Out1, Pot1 = volume)\n");
	printf(" 11) Audio dual stream (In1->Out1 Pot1, In2->Out2 Pot2)\n");
	printf(" 12) Pot cross-bleed regression (set Pot1 min, Pot3 max)\n");
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
				run_selected_app<sandbox::apps::PotReadStabilityTest>();
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
				run_selected_app<sandbox::apps::AdcEngineTest>();
			case 9:
				run_selected_app<sandbox::apps::PotLiveMonitorTest>();
			case 10:
				run_selected_app<sandbox::apps::AudioVolumeTest>();
			case 11:
				run_selected_app<sandbox::apps::AudioDualStreamTest>();
			case 12:
				run_selected_app<sandbox::apps::PotCrossBleedTest>();
			default:
				printf("\nInvalid selection: %d\n", selection);
				sleep_ms(900);
				break;
		}
	}
}
