#include "pico/stdlib.h"

#include "apps/audio_processor_example.h"

// Switch examples by changing this include + class type.
AudioProcessorExample app;

int main() {
	stdio_init_all();
	app.init();

	while (true) {
		app.update();
	}
}
