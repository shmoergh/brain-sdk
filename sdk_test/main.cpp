#include <stdio.h>

#include "pico/stdlib.h"

#include "brain-common/brain-common.h"
#include "brain-ui/button.h"
#include "brain-ui/leds.h"
#include "brain-ui/pot-multi-function.h"
#include "brain-ui/pots.h"

namespace {

constexpr uint8_t kPotX = 0;
constexpr uint8_t kPotY = 1;
constexpr uint8_t kPotZ = 2;

constexpr uint8_t kContextVelocity = 0;
constexpr uint8_t kContextTempo = 1;
constexpr uint8_t kContextScale = 2;

constexpr uint8_t kBehaviorDirect = 0;
constexpr uint8_t kBehaviorPickup = 1;
constexpr uint8_t kBehaviorScale = 2;

// Function id table [behavior][context]
constexpr uint8_t kFunctionIds[3][3] = {
	{1, 2, 3},
	{4, 5, 6},
	{7, 8, 9},
};

class UiSandbox {
public:
	UiSandbox()
		: button_a_(GPIO_BRAIN_BUTTON_1),
		  button_b_(GPIO_BRAIN_BUTTON_2),
		  button_a_pressed_(false),
		  button_b_pressed_(false),
		  last_print_us_(0) {}

	void init() {
		stdio_init_all();

		button_a_.init();
		button_b_.init();
		button_a_.set_on_press([this]() { button_a_pressed_ = true; });
		button_a_.set_on_release([this]() { button_a_pressed_ = false; });
		button_b_.set_on_press([this]() { button_b_pressed_ = true; });
		button_b_.set_on_release([this]() { button_b_pressed_ = false; });

		leds_.init();
		leds_.startup_animation();

		brain::ui::PotsConfig config = brain::ui::create_default_config(3, 8);
		config.simple = true;
		pots_.init(config);

		register_functions();

		printf("UI Sandbox started\n");
		printf("Pot X = value source\n");
		printf("Pot Y = behavior (Direct/Pickup/ValueScale)\n");
		printf("Button A/B = context modifier (Tempo/Scale)\n");
	}

	void update() {
		button_a_.update();
		button_b_.update();
		pots_.scan();

		uint8_t behavior = resolve_behavior_from_pot_y();
		uint8_t context = resolve_context();
		uint8_t function_id = kFunctionIds[behavior][context];

		multi_.set_active_function(kPotX, function_id);
		multi_.update(pots_);

		render_leds(behavior, context);
		print_status(behavior, context);
		multi_.clear_changed_flags();
	}

private:
	brain::ui::Button button_a_;
	brain::ui::Button button_b_;
	brain::ui::Leds leds_;
	brain::ui::Pots pots_;
	brain::ui::PotMultiFunction multi_;

	bool button_a_pressed_;
	bool button_b_pressed_;
	uint32_t last_print_us_;

	void register_functions() {
		multi_.init();

		for (uint8_t behavior = 0; behavior < 3; behavior++) {
			for (uint8_t context = 0; context < 3; context++) {
				brain::ui::PotFunctionConfig cfg;
				cfg.function_id = kFunctionIds[behavior][context];
				cfg.pot_index = kPotX;
				cfg.pickup_hysteresis = 1;

				if (context == kContextVelocity) {
					cfg.min_value = 0;
					cfg.max_value = 127;
					cfg.initial_value = 12;
				} else if (context == kContextTempo) {
					cfg.min_value = 20;
					cfg.max_value = 240;
					cfg.initial_value = 120;
				} else {
					cfg.min_value = 0;
					cfg.max_value = 11;
					cfg.initial_value = 0;
				}

				if (behavior == kBehaviorDirect) {
					cfg.behavior = brain::ui::PotBehavior::kDirect;
				} else if (behavior == kBehaviorPickup) {
					cfg.behavior = brain::ui::PotBehavior::kPickup;
				} else {
					cfg.behavior = brain::ui::PotBehavior::kValueScale;
				}

				multi_.register_function(cfg);
			}
		}
	}

	uint8_t resolve_behavior_from_pot_y() {
		uint16_t raw = pots_.get(kPotY);  // 0..255
		if (raw < 85) return kBehaviorDirect;
		if (raw < 170) return kBehaviorPickup;
		return kBehaviorScale;
	}

	uint8_t resolve_context() const {
		if (button_a_pressed_ && !button_b_pressed_) return kContextTempo;
		if (button_b_pressed_ && !button_a_pressed_) return kContextScale;
		return kContextVelocity;
	}

	void render_leds(uint8_t behavior, uint8_t context) {
		// LEDs 1-3 = behavior, 4-6 = context
		uint8_t mask = 0;
		mask |= (1 << behavior);
		mask |= (1 << (3 + context));
		leds_.set_from_mask(mask);
	}

	void print_status(uint8_t behavior, uint8_t context) {
		uint32_t now = to_us_since_boot(get_absolute_time());
		if (now - last_print_us_ < 100000) return;
		last_print_us_ = now;

		const char* behavior_name = (behavior == kBehaviorDirect)
			? "Direct"
			: (behavior == kBehaviorPickup ? "Pickup" : "ValueScale");
		const char* context_name = (context == kContextVelocity)
			? "Velocity"
			: (context == kContextTempo ? "Tempo" : "Scale");

		uint8_t function_id = kFunctionIds[behavior][context];
		int32_t value = multi_.get_value(function_id);
		printf("Behavior=%s Context=%s Value=%ld PotX=%u PotY=%u PotZ=%u\n",
			behavior_name,
			context_name,
			static_cast<long>(value),
			static_cast<unsigned>(pots_.get(kPotX)),
			static_cast<unsigned>(pots_.get(kPotY)),
			static_cast<unsigned>(pots_.get(kPotZ)));
	}
};

}  // namespace

int main() {
	UiSandbox app;
	app.init();
	while (true) {
		app.update();
	}
	return 0;
}
