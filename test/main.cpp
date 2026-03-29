#include "apps/storage_test.h"
#include "apps/storage_persistence_check_test.h"

int main() {
	sandbox::apps::StoragePersistenceCheckTest app;
	app.init();
	while (true) {
		app.update();
	}
	return 0;
}
