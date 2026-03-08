#include "leds_test.h"

#include <stdio.h>

#include "pico/stdlib.h"

namespace {

constexpr uint32_t kPollStepMs = 5;
constexpr uint8_t kMaskAll = static_cast<uint8_t>((1u << brain::ui::NO_OF_LEDS) - 1u);
constexpr uint32_t kPerLedDelayMs = 1000;
constexpr uint32_t kBetweenSequenceDelayMs = 300;
constexpr uint8_t kCaseCount = 24;

const char* kCaseNames[kCaseCount] = {
	"General/simple: startup animation",
	"General/simple: repeated init behavior",
	"Simple: grouped on/off + toggle",
	"Simple: grouped brightness behavior",
	"Simple: grouped blink_duration",
	"Simple: grouped start_blink/stop_blink",
	"Simple: on_all/off_all",
	"Simple: grouped mask patterns + one-hot",
	"Simple: invalid index no-op",
	"Simple: transitions while blinking",
	"Simple: no-update blink progression",
	"General/pwm: startup animation",
	"General/pwm: repeated init behavior",
	"PWM: grouped on/off + toggle",
	"PWM: grouped brightness sweeps",
	"PWM: grouped blink_duration",
	"PWM: grouped start_blink/stop_blink",
	"PWM: on_all/off_all",
	"PWM: grouped mask patterns + one-hot",
	"PWM: invalid index no-op",
	"PWM: transitions while blinking",
	"PWM: no-update blink progression",
	"Led direct: finite blink(times, interval)",
	"Led direct: callbacks"
};

const char* mode_label(bool simple_mode) {
	return simple_mode ? "simple" : "pwm";
}

}  // namespace

namespace sandbox::apps {

LedsTest::LedsTest()
	: leds_(brain::ui::LedMode::kSimple),
	  direct_led_(brain::ui::led_pins[0], false),
	  initialized_(false),
	  completed_(false),
	  aborted_(false),
	  pass_count_(0),
	  fail_count_(0),
	  skip_count_(0) {}

void LedsTest::init() {
	stdio_init_all();
	printf("\033[2J\033[H");
	fflush(stdout);

	leds_.init(brain::ui::LedMode::kSimple);
	direct_led_.init();

	leds_.off_all();
	direct_led_.off();

	print_led_map();
	initialized_ = true;
}

void LedsTest::update() {
	if (!initialized_ || completed_) {
		return;
	}

	run_all_tests();
	completed_ = true;

	printf("\n=== LED test summary ===\n");
	printf("PASS: %lu\n", static_cast<unsigned long>(pass_count_));
	printf("FAIL: %lu\n", static_cast<unsigned long>(fail_count_));
	printf("SKIP: %lu\n", static_cast<unsigned long>(skip_count_));
	printf("ABORTED: %s\n", aborted_ ? "yes" : "no");
	printf("========================\n");
}

void LedsTest::run_all_tests() {
	printf("\nRunning LED test cases with human validation.\n");
	printf("Respond to each check with: y=pass, n=fail, s=skip, q=abort.\n");
	printf("No external buttons are used; validation is done through UART only.\n");
	printf("After each test run, control returns to this menu.\n");

	while (true) {
		print_test_menu();

		bool run_all = false;
		uint8_t selected_case = 0;
		if (!prompt_test_selection(run_all, selected_case)) {
			return;
		}

		aborted_ = false;
		if (run_all) {
			for (uint8_t i = 1; i <= kCaseCount && !aborted_; i++) {
				run_case(i);
			}
		} else {
			run_case(selected_case);
		}

		leds_.off_all();
		direct_led_.off();

		if (aborted_) {
			printf("\n[Test run aborted, returning to menu]\n");
		} else {
			printf("\n[Test run finished, returning to menu]\n");
		}
		printf("Current summary: PASS=%lu FAIL=%lu SKIP=%lu\n",
			static_cast<unsigned long>(pass_count_),
			static_cast<unsigned long>(fail_count_),
			static_cast<unsigned long>(skip_count_));
	}
}

void LedsTest::print_test_menu() const {
	printf("\nAvailable tests:\n");
	for (uint8_t i = 0; i < kCaseCount; i++) {
		printf("  %u. %s\n", static_cast<unsigned>(i + 1), kCaseNames[i]);
	}
}

bool LedsTest::prompt_test_selection(bool& run_all, uint8_t& selected_case) {
	char line[16];

	clear_pending_input();
	while (true) {
		printf("\nSelect test to run: [a] all, [1-%u] single, [q] abort\n> ", static_cast<unsigned>(kCaseCount));
		fflush(stdout);
		if (!read_line(line, sizeof(line))) {
			continue;
		}

		if ((line[0] == 'a' || line[0] == 'A') && line[1] == '\0') {
			run_all = true;
			selected_case = 0;
			return true;
		}
		if ((line[0] == 'q' || line[0] == 'Q') && line[1] == '\0') {
			aborted_ = true;
			return false;
		}

		uint16_t value = 0;
		bool valid_number = true;
		for (size_t i = 0; line[i] != '\0'; i++) {
			if (line[i] < '0' || line[i] > '9') {
				valid_number = false;
				break;
			}
			value = static_cast<uint16_t>(value * 10 + static_cast<uint16_t>(line[i] - '0'));
		}

		if (valid_number && value >= 1 && value <= kCaseCount) {
			run_all = false;
			selected_case = static_cast<uint8_t>(value);
			return true;
		}

		printf("Invalid selection.\n");
	}
}

void LedsTest::run_case(uint8_t case_index) {
	switch (case_index) {
		case 1:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			record_case(
				"General/simple: startup animation final state",
				test_startup_animation("simple") ? CaseResult::kPass : CaseResult::kFail);
			if (!aborted_) {
				prompt_validation(
					"General/simple: startup animation visual",
					"Did you observe a one-by-one startup animation in simple mode?");
			}
			break;
		case 2:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			record_case(
				"General/simple: repeated init behavior",
				test_repeated_init(true) ? CaseResult::kPass : CaseResult::kFail);
			if (!aborted_) {
				prompt_validation(
					"General/simple: repeated init visual",
					"During repeated-init check, did LEDs remain controllable (no stuck or dead LED behavior)?");
			}
			break;
		case 3:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_on_off_toggle_per_led("simple");
			break;
		case 4:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_brightness("simple", true);
			break;
		case 5:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_blink_duration_per_led("simple");
			break;
		case 6:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_start_stop_blink_per_led("simple");
			break;
		case 7:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_on_all_off_all("simple");
			break;
		case 8:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_mask_mode("simple");
			break;
		case 9:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_invalid_indices("simple");
			break;
		case 10:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_state_transitions_while_blinking("simple");
			break;
		case 11:
			ensure_leds_mode(brain::ui::LedMode::kSimple);
			test_no_update_no_blink_progress("simple");
			break;
		case 12:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			record_case(
				"General/pwm: startup animation final state",
				test_startup_animation("pwm") ? CaseResult::kPass : CaseResult::kFail);
			if (!aborted_) {
				prompt_validation(
					"General/pwm: startup animation visual",
					"Did you observe a one-by-one startup animation in PWM mode?");
			}
			break;
		case 13:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			record_case(
				"General/pwm: repeated init behavior",
				test_repeated_init(false) ? CaseResult::kPass : CaseResult::kFail);
			if (!aborted_) {
				prompt_validation(
					"General/pwm: repeated init visual",
					"During repeated-init check in PWM mode, did LEDs remain controllable?");
			}
			break;
		case 14:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_on_off_toggle_per_led("pwm");
			break;
		case 15:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_brightness("pwm", false);
			break;
		case 16:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_blink_duration_per_led("pwm");
			break;
		case 17:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_start_stop_blink_per_led("pwm");
			break;
		case 18:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_on_all_off_all("pwm");
			break;
		case 19:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_mask_mode("pwm");
			break;
		case 20:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_invalid_indices("pwm");
			break;
		case 21:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_state_transitions_while_blinking("pwm");
			break;
		case 22:
			ensure_leds_mode(brain::ui::LedMode::kPwm);
			test_no_update_no_blink_progress("pwm");
			break;
		case 23:
			record_case(
				"Led direct: finite blink(times, interval) internal",
				test_direct_led_finite_blink() ? CaseResult::kPass : CaseResult::kFail);
			if (!aborted_) {
				prompt_validation(
					"Led direct: finite blink(times, interval) visual",
					"Did LED0 blink 3 times and stop OFF in the previous step?");
			}
			break;
		case 24:
			record_case("Led direct: callbacks fire", test_direct_led_callbacks() ? CaseResult::kPass : CaseResult::kFail);
			break;
		default:
			printf("Unknown test case index: %u\n", static_cast<unsigned>(case_index));
			break;
	}
}

void LedsTest::ensure_leds_mode(brain::ui::LedMode mode) {
	if (leds_.get_mode() != mode) {
		printf(
			"\n[MODE] Switching shared LED bank to %s mode.\n",
			mode == brain::ui::LedMode::kSimple ? "SIMPLE" : "PWM");
		leds_.set_mode(mode);
	}
	leds_.off_all();
}

void LedsTest::print_led_map() const {
	printf("\n\r--------\n\r");
	printf("LED Sandbox Test Runner\n");
	printf("Mapped LEDs:\n");
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		printf("  LED %u -> GPIO %u\n", static_cast<unsigned>(i), static_cast<unsigned>(brain::ui::led_pins[i]));
	}
}

void LedsTest::record_case(const char* name, CaseResult result) {
	switch (result) {
		case CaseResult::kPass:
			pass_count_++;
			printf("[PASS] %s\n", name);
			break;
		case CaseResult::kFail:
			fail_count_++;
			printf("[FAIL] %s\n", name);
			break;
		case CaseResult::kSkip:
			skip_count_++;
			printf("[SKIP] %s\n", name);
			break;
	}
}

void LedsTest::clear_pending_input() {
	while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {
	}
}

bool LedsTest::read_line(char* buffer, size_t size) {
	if (size == 0) {
		return false;
	}

	size_t pos = 0;
	while (true) {
		int ch = getchar_timeout_us(100000);
		if (ch == PICO_ERROR_TIMEOUT) {
			continue;
		}

		if (ch == '\r' || ch == '\n') {
			buffer[pos] = '\0';
			printf("\n");
			return pos > 0;
		}

		if ((ch == 8 || ch == 127) && pos > 0) {
			pos--;
			printf("\b \b");
			fflush(stdout);
			continue;
		}

		if (pos < size - 1) {
			buffer[pos++] = static_cast<char>(ch);
			putchar(ch);
			fflush(stdout);
		}
	}
}

void LedsTest::wait_for_enter(const char* step_name) {
	if (aborted_) {
		return;
	}

	clear_pending_input();
	printf("\n[READY] %s\n", step_name);
	printf("Press ENTER to run this step (or q then ENTER to abort): ");
	fflush(stdout);

	while (true) {
		int ch = getchar_timeout_us(100000);
		if (ch == PICO_ERROR_TIMEOUT) {
			continue;
		}

		if (ch == 'q' || ch == 'Q') {
			aborted_ = true;
			printf("q\n");
			return;
		}
		if (ch == '\r' || ch == '\n') {
			printf("\n");
			return;
		}
	}
}

LedsTest::CaseResult LedsTest::prompt_validation(const char* name, const char* prompt) {
	if (aborted_) {
		record_case(name, CaseResult::kSkip);
		return CaseResult::kSkip;
	}

	printf("\n[CHECK] %s\n", name);
	printf("%s\n", prompt);
	printf("Input [y/n/s/q]: ");
	fflush(stdout);

	while (true) {
		int ch = getchar_timeout_us(100000);
		if (ch == PICO_ERROR_TIMEOUT) {
			continue;
		}

		if (ch == 'y' || ch == 'Y') {
			printf("y\n");
			record_case(name, CaseResult::kPass);
			return CaseResult::kPass;
		}
		if (ch == 'n' || ch == 'N') {
			printf("n\n");
			record_case(name, CaseResult::kFail);
			return CaseResult::kFail;
		}
		if (ch == 's' || ch == 'S') {
			printf("s\n");
			record_case(name, CaseResult::kSkip);
			return CaseResult::kSkip;
		}
		if (ch == 'q' || ch == 'Q') {
			printf("q\n");
			aborted_ = true;
			record_case(name, CaseResult::kSkip);
			return CaseResult::kSkip;
		}
	}
}

bool LedsTest::test_startup_animation(const char* mode_name) {
	char step_name[128];
	snprintf(step_name, sizeof(step_name), "%s: startup_animation()", mode_name);
	wait_for_enter(step_name);
	if (aborted_) {
		return true;
	}

	leds_.off_all();
	leds_.startup_animation();

	// Keep this internal sanity check; visual correctness is separately user-validated.
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		if (leds_.is_on(i)) {
			printf("  %s startup_animation ended with LED%u still on\n", mode_name, static_cast<unsigned>(i));
			return false;
		}
	}
	return true;
}

bool LedsTest::test_repeated_init(bool simple_mode) {
	char step_name[128];
	snprintf(step_name, sizeof(step_name), "%s: repeated init internal check", mode_label(simple_mode));
	wait_for_enter(step_name);
	if (aborted_) {
		return true;
	}

	brain::ui::Leds leds(simple_mode);
	leds.init();
	leds.init();
	leds.on_all();
	leds.off_all();

	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		leds.on(i);
		if (!leds.is_on(i)) {
			printf(
				"  repeated init (%s): LED%u did not respond after second init\n",
				mode_label(simple_mode),
				static_cast<unsigned>(i));
			return false;
		}
		leds.off(i);
	}
	return true;
}

void LedsTest::test_on_off_toggle_per_led(const char* mode_name) {
	char name[128];

	wait_for_enter("Grouped on/off sequence for all LEDs");
	if (aborted_) return;
	leds_.off_all();
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		leds_.on(i);
		spin_wait_ms(kPerLedDelayMs);
		leds_.off(i);
		spin_wait_ms(120);
	}
	snprintf(name, sizeof(name), "%s: grouped on/off sequence", mode_name);
	prompt_validation(name, "Did each LED turn ON then OFF in order with ~1s per LED?");
	if (aborted_) return;

	wait_for_enter("Grouped toggle sequence for all LEDs");
	if (aborted_) return;
	leds_.off_all();
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		leds_.toggle(i);	// off -> on
		spin_wait_ms(kPerLedDelayMs);
		leds_.toggle(i);	// on -> off
		spin_wait_ms(120);
	}
	snprintf(name, sizeof(name), "%s: grouped toggle sequence", mode_name);
	prompt_validation(name, "Did each LED toggle ON then OFF in order?");
}

void LedsTest::test_brightness(const char* mode_name, bool simple_mode) {
	char name[128];

	if (simple_mode) {
		wait_for_enter("Grouped simple brightness non-zero->zero sequence");
		if (aborted_) return;
		leds_.off_all();
		for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
			leds_.set_brightness(i, 1);
			spin_wait_ms(kPerLedDelayMs);
			leds_.set_brightness(i, 0);
			spin_wait_ms(120);
		}
		snprintf(name, sizeof(name), "%s: grouped simple brightness behavior", mode_name);
		prompt_validation(name, "Did each LED turn ON for non-zero brightness and OFF for zero?");
		return;
	}

	wait_for_enter("Grouped PWM 10% brightness sweeps for all LEDs");
	if (aborted_) return;
	leds_.off_all();
	const uint8_t values[] = {0, 26, 51, 77, 102, 128, 153, 179, 204, 230, 255};
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		for (uint8_t value : values) {
			leds_.set_brightness(i, value);
			spin_wait_ms(120);
		}
		leds_.set_brightness(i, 0);
		spin_wait_ms(kBetweenSequenceDelayMs);
	}
	snprintf(name, sizeof(name), "%s: grouped PWM brightness sweeps", mode_name);
	prompt_validation(name, "Did each LED show a visible dim->bright->off PWM sweep?");
}

void LedsTest::test_blink_duration_per_led(const char* mode_name) {
	char name[128];

	wait_for_enter("Grouped blink_duration sequence for all LEDs");
	if (aborted_) return;
	leds_.off_all();
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		leds_.blink_duration(i, 900, 140);
		wait_with_leds_update(leds_, 1100);
		spin_wait_ms(120);
	}
	snprintf(name, sizeof(name), "%s: grouped blink_duration sequence", mode_name);
	prompt_validation(name, "Did each LED blink for ~0.9s and then stop OFF before the next LED?");
}

void LedsTest::test_start_stop_blink_per_led(const char* mode_name) {
	char name[128];

	wait_for_enter("Grouped start_blink sequence for all LEDs");
	if (aborted_) return;
	leds_.off_all();
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		leds_.start_blink(i, 120);
		wait_with_leds_update(leds_, kPerLedDelayMs);
		leds_.stop_blink(i);
		wait_with_leds_update(leds_, 160);
	}
	snprintf(name, sizeof(name), "%s: grouped start_blink/stop_blink sequence", mode_name);
	prompt_validation(name, "Did each LED blink continuously for ~1s then stop OFF before the next LED?");
}

void LedsTest::test_on_all_off_all(const char* mode_name) {
	char name[128];

	wait_for_enter("on_all()");
	if (aborted_) return;
	leds_.on_all();
	snprintf(name, sizeof(name), "%s: on_all()", mode_name);
	prompt_validation(name, "Are all 6 LEDs ON?");
	if (aborted_) return;

	wait_for_enter("off_all()");
	if (aborted_) return;
	leds_.off_all();
	snprintf(name, sizeof(name), "%s: off_all()", mode_name);
	prompt_validation(name, "Are all 6 LEDs OFF?");
}

void LedsTest::test_mask_mode(const char* mode_name) {
	char name[128];
	char prompt[220];

	const uint8_t masks[] = {0x00, kMaskAll, 0x2B, 0xC0, 0xFF};

	wait_for_enter("Grouped mask pattern sequence");
	if (aborted_) return;
	for (uint8_t mask : masks) {
		if (aborted_) return;
		leds_.set_from_mask(mask);
		spin_wait_ms(kPerLedDelayMs);
		announce_expected_mask(static_cast<uint8_t>(mask & kMaskAll));
	}

	// One-hot walk over all LEDs as part of the same grouped sequence.
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		leds_.set_from_mask(static_cast<uint8_t>(1u << i));
		spin_wait_ms(kPerLedDelayMs);
	}

	snprintf(name, sizeof(name), "%s: grouped mask patterns + one-hot walk", mode_name);
	snprintf(prompt, sizeof(prompt), "Did masks 0x00, all-on, mixed, high-bits handling and one-hot walk look correct?");
	prompt_validation(name, prompt);
}

void LedsTest::test_invalid_indices(const char* mode_name) {
	wait_for_enter("invalid index no-op checks");
	if (aborted_) return;

	leds_.set_from_mask(0x15);
	announce_expected_mask(0x15);

	const uint8_t invalid_0 = brain::ui::NO_OF_LEDS;
	const uint8_t invalid_1 = 255;

	leds_.on(invalid_0);
	leds_.off(invalid_0);
	leds_.toggle(invalid_0);
	leds_.set_brightness(invalid_0, 100);
	leds_.blink_duration(invalid_0, 300, 100);
	leds_.start_blink(invalid_0, 80);
	leds_.stop_blink(invalid_0);

	leds_.on(invalid_1);
	leds_.off(invalid_1);
	leds_.toggle(invalid_1);
	leds_.set_brightness(invalid_1, 100);
	leds_.blink_duration(invalid_1, 300, 100);
	leds_.start_blink(invalid_1, 80);
	leds_.stop_blink(invalid_1);

	char name[128];
	snprintf(name, sizeof(name), "%s: invalid indices (%u,255)", mode_name, static_cast<unsigned>(invalid_0));
	prompt_validation(name, "Did LED pattern remain unchanged (still mask 0x15)?");
}

void LedsTest::test_state_transitions_while_blinking(const char* mode_name) {
	char name[140];

	leds_.off_all();
	wait_for_enter("start transition-while-blinking sequence");
	if (aborted_) return;
	leds_.start_blink(0, 120);
	wait_with_leds_update(leds_, 900);
	snprintf(name, sizeof(name), "%s: baseline blinking LED0", mode_name);
	prompt_validation(name, "Is LED0 blinking?");
	if (aborted_) return;

	wait_for_enter("on() while blinking");
	if (aborted_) return;
	leds_.on(0);
	wait_with_leds_update(leds_, 650);
	snprintf(name, sizeof(name), "%s: on() while blinking", mode_name);
	prompt_validation(name, "Does LED0 continue blinking after calling on()?");
	if (aborted_) return;

	wait_for_enter("off() while blinking");
	if (aborted_) return;
	leds_.off(0);
	wait_with_leds_update(leds_, 650);
	snprintf(name, sizeof(name), "%s: off() while blinking", mode_name);
	prompt_validation(name, "Does LED0 continue blinking after calling off()?");
	if (aborted_) return;

	wait_for_enter("toggle() while blinking");
	if (aborted_) return;
	leds_.toggle(0);
	wait_with_leds_update(leds_, 650);
	snprintf(name, sizeof(name), "%s: toggle() while blinking", mode_name);
	prompt_validation(name, "Does LED0 continue blinking after calling toggle()?");
	if (aborted_) return;

	wait_for_enter("set_from_mask(0) while blinking");
	if (aborted_) return;
	leds_.set_from_mask(0x00);
	wait_with_leds_update(leds_, 650);
	snprintf(name, sizeof(name), "%s: set_from_mask(0) while blinking", mode_name);
	prompt_validation(name, "Does LED0 continue blinking after set_from_mask(0)?");
	if (aborted_) return;

	wait_for_enter("stop_blink() after transition tests");
	if (aborted_) return;
	leds_.stop_blink(0);
	wait_with_leds_update(leds_, 150);
	snprintf(name, sizeof(name), "%s: stop_blink after transitions", mode_name);
	prompt_validation(name, "Did LED0 stop blinking and stay OFF?");
}

void LedsTest::test_no_update_no_blink_progress(const char* mode_name) {
	char name[140];

	leds_.off_all();
	wait_for_enter("no-update no-progress check");
	if (aborted_) return;
	leds_.start_blink(1, 120);
	spin_wait_ms(900);	// no update on purpose
	snprintf(name, sizeof(name), "%s: no update() no blink progression", mode_name);
	prompt_validation(name, "Before update() calls, did LED1 stay steady (not blinking)?");
	if (aborted_) return;

	wait_for_enter("resume update() to allow blinking");
	if (aborted_) return;
	wait_with_leds_update(leds_, 900);
	snprintf(name, sizeof(name), "%s: blinking resumes with update()", mode_name);
	prompt_validation(name, "After update() loop started, did LED1 blink?");
	if (aborted_) return;

	wait_for_enter("stop_blink() after no-update test");
	if (aborted_) return;
	leds_.stop_blink(1);
	wait_with_leds_update(leds_, 150);
	snprintf(name, sizeof(name), "%s: stop_blink after no-update test", mode_name);
	prompt_validation(name, "After stop_blink(), is LED1 OFF?");
}

bool LedsTest::test_direct_led_finite_blink() {
	bool ok = true;

	wait_for_enter("direct Led blink(times, interval)");
	if (aborted_) return true;

	direct_led_.off();
	direct_led_.blink(3, 120);
	wait_with_led_update(direct_led_, 1500);

	if (direct_led_.is_blinking()) {
		printf("  direct Led finite blink: still blinking after expected end\n");
		ok = false;
	}
	if (direct_led_.is_on()) {
		printf("  direct Led finite blink: expected off at end\n");
		ok = false;
	}
	return ok;
}

bool LedsTest::test_direct_led_callbacks() {
	bool ok = true;
	uint32_t state_change_count = 0;
	uint32_t blink_end_count = 0;
	bool saw_state_on = false;
	bool saw_state_off = false;

	wait_for_enter("direct Led callback check");
	if (aborted_) return true;

	direct_led_.set_on_state_change([&](bool on) {
		state_change_count++;
		saw_state_on |= on;
		saw_state_off |= !on;
	});
	direct_led_.set_on_blink_end([&]() { blink_end_count++; });

	direct_led_.on();
	direct_led_.off();
	direct_led_.blink(1, 90);
	wait_with_led_update(direct_led_, 500);

	if (state_change_count == 0 || !saw_state_on || !saw_state_off) {
		printf("  direct Led callbacks: state-change callback did not receive expected events\n");
		ok = false;
	}
	if (blink_end_count == 0) {
		printf("  direct Led callbacks: blink-end callback did not fire\n");
		ok = false;
	}

	direct_led_.set_on_state_change({});
	direct_led_.set_on_blink_end({});
	return ok;
}

void LedsTest::announce_expected_mask(uint8_t mask) const {
	printf("  Expected ON LEDs:");
	bool any = false;
	for (uint8_t i = 0; i < brain::ui::NO_OF_LEDS; i++) {
		if ((mask >> i) & 0x1u) {
			printf(" %u", static_cast<unsigned>(i));
			any = true;
		}
	}
	if (!any) {
		printf(" (none)");
	}
	printf("\n");
}

void LedsTest::spin_wait_ms(uint32_t ms) const {
	sleep_ms(ms);
}

void LedsTest::wait_with_leds_update(brain::ui::Leds& leds, uint32_t ms) const {
	absolute_time_t start = get_absolute_time();
	while (static_cast<uint32_t>(absolute_time_diff_us(start, get_absolute_time()) / 1000) < ms) {
		leds.update();
		sleep_ms(kPollStepMs);
	}
}

void LedsTest::wait_with_led_update(brain::ui::Led& led, uint32_t ms) const {
	absolute_time_t start = get_absolute_time();
	while (static_cast<uint32_t>(absolute_time_diff_us(start, get_absolute_time()) / 1000) < ms) {
		led.update();
		sleep_ms(kPollStepMs);
	}
}

}  // namespace sandbox::apps
