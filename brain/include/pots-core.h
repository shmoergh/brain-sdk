#pragma once

#include <cstdint>
#include <functional>

#include "gpio-setup.h"

static constexpr uint8_t kMaxPots = 4;

struct PotsConfig {
	bool simple;
	uint8_t adc_gpio;
	uint8_t s0_gpio;
	uint8_t s1_gpio;
	uint8_t num_pots;
	uint8_t channel_map[kMaxPots];
	uint8_t output_resolution;
	uint32_t settling_delay_us;
	uint8_t samples_per_read;
	uint16_t change_threshold;
};

PotsConfig create_default_config(uint8_t num_pots = 3, uint8_t output_resolution = 7);

class Pots {
public:
	Pots();
	void init(const PotsConfig& cfg);

	void set_simple(bool simple);
	void set_output_resolution(uint8_t resolution);
	void set_settling_delay_us(uint32_t delay);
	void set_samples_per_read(uint8_t samples);
	void set_change_threshold(uint16_t threshold);

	void scan();
	uint16_t get(uint8_t index);
	uint16_t get_buffered(uint8_t index) const;
	uint16_t get_raw(uint8_t index);

	void set_on_change(std::function<void(uint8_t, uint16_t)> cb);

private:
	void set_mux_channel(uint8_t ch);
	uint16_t read_channel_once(uint8_t ch);

	PotsConfig config_;
	uint16_t last_values_[kMaxPots];
	uint16_t buffered_values_[kMaxPots];
	std::function<void(uint8_t, uint16_t)> on_change_;
};

namespace brain {
namespace ui {
using ::PotsConfig;
using ::create_default_config;
using ::Pots;
using ::kMaxPots;
}  // namespace ui
}  // namespace brain
