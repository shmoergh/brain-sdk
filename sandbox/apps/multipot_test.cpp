#include "multipot_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

#include "brain-common/brain-common.h"

namespace {

constexpr uint8_t kPotX = 0;
constexpr uint8_t kPotY = 1;
constexpr uint8_t kPotZ = 2;

constexpr uint8_t kFunctionVelocity = 0;
constexpr uint8_t kFunctionTempo = 1;
constexpr uint8_t kFunctionScale = 2;

// Sandbox knob: choose the mode used by Pot X.
constexpr brain::ui::PotMode kValuePotMode = brain::ui::PotMode::kValueScale;

// Function id table [function] for the selected static mode.
constexpr uint8_t kFunctionIdsByFunction[3] = {
	1, 2, 3
};

constexpr uint8_t mode_to_index(brain::ui::PotMode mode) {
	switch (mode) {
		case brain::ui::PotMode::kDirect:
			return 0;
		case brain::ui::PotMode::kPickup:
			return 1;
		case brain::ui::PotMode::kValueScale:
			return 2;
	}
	return 0;
}

const char* mode_to_name(brain::ui::PotMode mode) {
	switch (mode) {
		case brain::ui::PotMode::kDirect:
			return "Direct";
		case brain::ui::PotMode::kPickup:
			return "Pickup";
		case brain::ui::PotMode::kValueScale:
			return "ValueScale";
	}
	return "Unknown";
}

const char* function_to_name(uint8_t function_index) {
	if (function_index == kFunctionVelocity) return "Velocity";
	if (function_index == kFunctionTempo) return "Tempo";
	return "Scale";
}

}  // namespace

namespace sandbox::apps {

MultipotTest::MultipotTest()
	: button_a_(GPIO_BRAIN_BUTTON_1),
	  button_b_(GPIO_BRAIN_BUTTON_2),
	  button_a_pressed_(false),
	  button_b_pressed_(false),
	  last_print_us_(0),
	  last_status_len_(0) {}

void MultipotTest::init() {
	stdio_init_all();
	last_status_len_ = 0;

	button_a_.init();
	button_b_.init();
	button_a_.set_on_press([this]() { button_a_pressed_ = true; });
	button_a_.set_on_release([this]() { button_a_pressed_ = false; });
	button_b_.set_on_press([this]() { button_b_pressed_ = true; });
	button_b_.set_on_release([this]() { button_b_pressed_ = false; });

	leds_.init();
	leds_.startup_animation();

	brain::ui::PotsConfig config = brain::ui::create_default_config(3, 8);
	config.simple = false;
	pots_.init(config);

	register_functions();

	printf("\n\r--------\n\r");
	printf("UI Sandbox started\n");
	printf("Pot X = value source\n");
	printf("Value mode = %s (set in code)\n", mode_to_name(kValuePotMode));
	printf("Button A/B = function selector (Tempo/Scale)\n");
}

void MultipotTest::update() {
	button_a_.update();
	button_b_.update();
	pots_.scan();

	uint8_t function_index = resolve_function();
	uint8_t function_id = kFunctionIdsByFunction[function_index];

	multi_.set_active_function(kPotX, function_id);
	multi_.update(pots_);

	render_leds(function_index);
	print_status(function_index);
	multi_.clear_changed_flags();
}

void MultipotTest::register_functions() {
	multi_.init();

	for (uint8_t function_index = 0; function_index < 3; function_index++) {
		brain::ui::PotFunctionConfig cfg;
		cfg.function_id = kFunctionIdsByFunction[function_index];
		cfg.pot_index = kPotX;
		cfg.pickup_hysteresis = 1;
		cfg.mode = kValuePotMode;

		if (function_index == kFunctionVelocity) {
			cfg.min_value = 0;
			cfg.max_value = 127;
			cfg.initial_value = 12;
		} else if (function_index == kFunctionTempo) {
			cfg.min_value = 20;
			cfg.max_value = 240;
			cfg.initial_value = 120;
		} else {
			cfg.min_value = 0;
			cfg.max_value = 11;
			cfg.initial_value = 0;
		}

		multi_.register_function(cfg);
	}
}

uint8_t MultipotTest::resolve_function() const {
	if (button_a_pressed_ && !button_b_pressed_) return kFunctionTempo;
	if (button_b_pressed_ && !button_a_pressed_) return kFunctionScale;
	return kFunctionVelocity;
}

void MultipotTest::render_leds(uint8_t function_index) {
	// LEDs 1-3 = mode, 4-6 = function
	uint8_t mask = 0;
	mask |= (1 << mode_to_index(kValuePotMode));
	mask |= (1 << (3 + function_index));
	leds_.set_from_mask(mask);
}

void MultipotTest::print_status(uint8_t function_index) {
	uint32_t now = to_us_since_boot(get_absolute_time());
	if (now - last_print_us_ < 100000) return;
	last_print_us_ = now;

	const char* function_name = function_to_name(function_index);

	int32_t velocity_value = multi_.get_value(kFunctionIdsByFunction[kFunctionVelocity]);
	int32_t tempo_value = multi_.get_value(kFunctionIdsByFunction[kFunctionTempo]);
	int32_t scale_value = multi_.get_value(kFunctionIdsByFunction[kFunctionScale]);
	uint16_t pot_x = pots_.get_buffered(kPotX);

	char line[160];
	int len = snprintf(
		line,
		sizeof(line),
		"Function=%s | Velocity=%ld Tempo=%ld Scale=%ld PotX=%u",
		function_name,
		static_cast<long>(velocity_value),
		static_cast<long>(tempo_value),
		static_cast<long>(scale_value),
		static_cast<unsigned>(pot_x));
	if (len < 0) return;

	uint16_t current_len = static_cast<uint16_t>(len);
	printf("\r%s", line);
	if (current_len < last_status_len_) {
		printf("%*s", static_cast<int>(last_status_len_ - current_len), "");
	}
	last_status_len_ = current_len;
}

}  // namespace sandbox::apps
