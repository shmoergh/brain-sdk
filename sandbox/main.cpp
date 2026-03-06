#include "apps/midi_to_cv_test.h"

int main() {
	sandbox::apps::MidiToCvTest app;
	app.init();
	while (true) {
		app.update();
	}
	return 0;
}
