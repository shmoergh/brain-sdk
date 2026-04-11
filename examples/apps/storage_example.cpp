#include "storage_example.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace examples::apps {

void StorageExample::init() {
	printf("\n--------\n");
	printf("Example: storage app blob write/read\n");

	if (!brain_init_succeeded(brain_.init_storage())) {
		printf("[ERROR] init_storage failed\n");
		return;
	}
	if (!brain_init_succeeded(brain_.init_leds(LedMode::kSimple))) {
		printf("[ERROR] init_leds failed\n");
		return;
	}

	DemoBlob payload{};
	payload.magic = 0x42524149u;
	payload.mode = 2;
	payload.brightness = 120;

	const StorageStatus w = brain_.storage.write_app_blob(&payload, sizeof(payload));
	printf("write_app_blob status=%u\n", static_cast<unsigned>(w));

	DemoBlob loaded{};
	size_t actual_size = 0;
	const StorageStatus r = brain_.storage.read_app_blob(&loaded, sizeof(loaded), &actual_size);
	printf("read_app_blob  status=%u size=%u magic=0x%08lx mode=%u brightness=%u\n",
		static_cast<unsigned>(r),
		static_cast<unsigned>(actual_size),
		static_cast<unsigned long>(loaded.magic),
		static_cast<unsigned>(loaded.mode),
		static_cast<unsigned>(loaded.brightness));

	initialized_ = true;
	last_toggle_ms_ = to_ms_since_boot(get_absolute_time());
}

void StorageExample::update() {
	if (!initialized_) {
		sleep_ms(10);
		return;
	}

	brain_.update_leds();
	const uint32_t now = to_ms_since_boot(get_absolute_time());
	if ((now - last_toggle_ms_) >= 500) {
		last_toggle_ms_ = now;
		led_state_ = !led_state_;
		if (led_state_) {
			brain_.leds.on(0);
		} else {
			brain_.leds.off(0);
		}
	}
}

}  // namespace examples::apps
