#include "apps/leds_test.h"

int main() {
	sandbox::apps::LedsTest app;
	app.init();
	while (true) {
		app.update();
	}
	return 0;
}
