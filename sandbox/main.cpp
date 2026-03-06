#include "apps/multipot_test.h"

int main() {
	sandbox::apps::MultipotTest app;
	app.init();
	while (true) {
		app.update();
	}
	return 0;
}
