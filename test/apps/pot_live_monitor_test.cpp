#include "pot_live_monitor_test.h"

#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/time.h"

namespace sandbox::apps {

namespace {

constexpr uint8_t kBarWidth = 24;

void render_bar(uint8_t value_u8, char* buf, size_t buf_len) {
	if (buf_len < kBarWidth + 3) return;
	const uint32_t filled = (static_cast<uint32_t>(value_u8) * kBarWidth) / 255u;
	buf[0] = '[';
	for (uint8_t i = 0; i < kBarWidth; ++i) {
		buf[1 + i] = (i < filled) ? '#' : ' ';
	}
	buf[1 + kBarWidth] = ']';
	buf[2 + kBarWidth] = '\0';
}

}  // namespace

void PotLiveMonitorTest::init() {
	stdio_init_all();
	sleep_ms(800);

	printf("\033[2J\033[H");
	printf("===============================================\n");
	printf(" Pot Live Monitor\n");
	printf(" Turn each pot fully left and right to verify.\n");
	printf("===============================================\n\n");

	BrainInitStatus status = brain_.init_pots(create_default_pots_config(kNumPots, 8));
	if (!brain_init_succeeded(status)) {
		printf("[ERROR] brain.init_pots() failed (status=%d).\n", static_cast<int>(status));
		printf("Cannot continue. Check wiring and Brain SDK version.\n");
		initialized_ = false;
		return;
	}

	for (uint8_t i = 0; i < kNumPots; ++i) {
		raw_min_[i] = 0xFFFFu;
		raw_max_[i] = 0u;
		mapped_min_[i] = 0xFFu;
		mapped_max_[i] = 0u;
	}

	printf("init_pots OK. num_pots=%u, output resolution=8 bit.\n", kNumPots);
	printf("Live values follow. Press the BOOTSEL button to reset.\n\n");
	initialized_ = true;
	first_print_ = true;
}

void PotLiveMonitorTest::update() {
	if (!initialized_) {
		sleep_ms(100);
		return;
	}

	// Sample every tick (cheap — pots.get is cached).
	for (uint8_t i = 0; i < kNumPots; ++i) {
		const uint16_t raw = brain_.pots.get_raw(i);
		const uint16_t mapped16 = brain_.pots.get(i);
		const uint8_t mapped = static_cast<uint8_t>(mapped16 > 255u ? 255u : mapped16);

		if (raw < raw_min_[i]) raw_min_[i] = raw;
		if (raw > raw_max_[i]) raw_max_[i] = raw;
		if (mapped < mapped_min_[i]) mapped_min_[i] = mapped;
		if (mapped > mapped_max_[i]) mapped_max_[i] = mapped;
	}
	++update_count_;

	const uint32_t now_us = to_us_since_boot(get_absolute_time());
	if ((now_us - last_print_us_) < kPrintIntervalUs) {
		sleep_ms(2);
		return;
	}
	last_print_us_ = now_us;

	// Cursor up to overwrite the previous block (10 lines: header + 3 pots
	// × 2 + summary block). On first print just print fresh; afterwards
	// rewind to the top of the live block.
	if (first_print_) {
		first_print_ = false;
	} else {
		// 11 lines: 3 pots (1 line each) + 1 blank + 3 summary lines + 1 blank + 1 footer + leading blank guard
		printf("\033[12A");
	}

	for (uint8_t i = 0; i < kNumPots; ++i) {
		const uint16_t raw = brain_.pots.get_raw(i);
		const uint16_t mapped16 = brain_.pots.get(i);
		const uint8_t mapped = static_cast<uint8_t>(mapped16 > 255u ? 255u : mapped16);

		char bar[kBarWidth + 3];
		render_bar(mapped, bar, sizeof(bar));

		printf("Pot %u  raw=%4u  mapped=%3u  %s\033[K\n",
			   static_cast<unsigned>(i),
			   static_cast<unsigned>(raw),
			   static_cast<unsigned>(mapped),
			   bar);
	}

	printf("\033[K\n");
	printf("Min observed (mapped):  [%3u, %3u, %3u]\033[K\n",
		   mapped_min_[0], mapped_min_[1], mapped_min_[2]);
	printf("Max observed (mapped):  [%3u, %3u, %3u]\033[K\n",
		   mapped_max_[0], mapped_max_[1], mapped_max_[2]);
	printf("Updates: %lu\033[K\n", static_cast<unsigned long>(update_count_));
	printf("\033[K\n");
	printf("Each pot should reach min ~0 and max ~255 after a full sweep.\033[K\n");

	fflush(stdout);
	sleep_ms(2);
}

}  // namespace sandbox::apps
