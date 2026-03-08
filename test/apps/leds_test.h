#pragma once

#include <cstddef>
#include <cstdint>

#include "brain-ui/led.h"
#include "brain-ui/leds.h"

namespace sandbox::apps {

/**
 * @brief Manual sandbox app that executes LED test cases and reports results on UART.
 */
class LedsTest {
public:
	LedsTest();

	void init();
	void update();

private:
	enum class CaseResult : uint8_t {
		kPass,
		kFail,
		kSkip
	};

	brain::ui::Leds leds_;
	brain::ui::Led direct_led_;
	bool initialized_;
	bool completed_;
	bool aborted_;
	uint32_t pass_count_;
	uint32_t fail_count_;
	uint32_t skip_count_;

	void run_all_tests();
	void print_test_menu() const;
	bool prompt_test_selection(bool& run_all, uint8_t& selected_case);
	void run_case(uint8_t case_index);
	void ensure_leds_mode(brain::ui::LedMode mode);
	void print_led_map() const;
	void record_case(const char* name, CaseResult result);
	void clear_pending_input();
	bool read_line(char* buffer, size_t size);
	void wait_for_enter(const char* step_name);
	CaseResult prompt_validation(const char* name, const char* prompt);

	bool test_startup_animation(const char* mode_name);
	bool test_repeated_init(bool simple_mode);
	void test_on_off_toggle_per_led(const char* mode_name);
	void test_brightness(const char* mode_name, bool simple_mode);
	void test_blink_duration_per_led(const char* mode_name);
	void test_start_stop_blink_per_led(const char* mode_name);
	void test_on_all_off_all(const char* mode_name);
	void test_mask_mode(const char* mode_name);
	void test_invalid_indices(const char* mode_name);
	void test_state_transitions_while_blinking(const char* mode_name);
	void test_no_update_no_blink_progress(const char* mode_name);

	bool test_direct_led_finite_blink();
	bool test_direct_led_callbacks();

	void announce_expected_mask(uint8_t mask) const;
	void spin_wait_ms(uint32_t ms) const;
	void wait_with_leds_update(brain::ui::Leds& leds, uint32_t ms) const;
	void wait_with_led_update(brain::ui::Led& led, uint32_t ms) const;
};

}  // namespace sandbox::apps
