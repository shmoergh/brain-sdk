#pragma once

#include <hardware/spi.h>

#include <cstdint>

#include "gpio-setup.h"
#include "storage.h"

enum class AudioCvOutChannel { kChannelA = 0, kChannelB = 1 };

enum class AudioCvOutRange {
	kRange0To10V = 0,
	kRangeMinus5To5V = 1
};

// Convenience aliases for less verbose call sites.
constexpr AudioCvOutChannel kOutputsChannelA = AudioCvOutChannel::kChannelA;
constexpr AudioCvOutChannel kOutputsChannelB = AudioCvOutChannel::kChannelB;

constexpr AudioCvOutRange kOutputsRange0To10V = AudioCvOutRange::kRange0To10V;
constexpr AudioCvOutRange kOutputsRangeMinus5To5V = AudioCvOutRange::kRangeMinus5To5V;

class Outputs {
public:
	static constexpr uint8_t kMCP4822_CHANNEL_A = 0;
	static constexpr uint8_t kMCP4822_CHANNEL_B = 1;
	static constexpr uint8_t kMCP4822_GAIN = 0;
	static constexpr uint8_t kMCP4822_ACTIVE = 1;

	static constexpr int32_t kUnipolarMinMillivolts = 0;
	static constexpr int32_t kUnipolarMaxMillivolts = 10000;
	static constexpr int32_t kBipolarMinMillivolts = -5000;
	static constexpr int32_t kBipolarMaxMillivolts = 5000;
	static constexpr int32_t kBipolarOffsetMillivolts = 5000;
	static constexpr int32_t kMaxOutputMillivolts = 10000;
	static constexpr uint16_t kMaxDacValue = 4095;
	static constexpr uint32_t kSpiFrequency = 1000000;

	/**
	 * @brief Creates an `Outputs` instance for Brain CV/audio DAC and pulse output control.
	 * @param pulse_out_gpio GPIO pin used for the digital pulse output (active-low hardware drive).
	 */
	explicit Outputs(uint pulse_out_gpio = GPIO_BRAIN_PULSE_OUTPUT);

	/**
	 * @brief Initializes DAC/SPI hardware for channels A and B and applies per-channel voltage ranges.
	 * @param spi_instance SPI peripheral used to talk to the MCP4822 DAC (`spi0` or `spi1`).
	 * @param cs_pin Chip-select GPIO for the DAC.
	 * @param sck_pin SPI clock GPIO for the DAC.
	 * @param tx_pin SPI MOSI GPIO for the DAC.
	 * @param coupling_pin_a Coupling/range control GPIO for output channel A.
	 * @param coupling_pin_b Coupling/range control GPIO for output channel B.
	 * @param range_a Startup range for channel A:
	 * - `kOutputsRange0To10V`: accepts only 0..10000 mV requests.
	 * - `kOutputsRangeMinus5To5V`: accepts -5000..5000 mV requests and applies +5000 mV DAC offset.
	 * @param range_b Startup range for channel B (same options and behavior as `range_a`).
	 * @return `true` when SPI/DAC setup succeeded; `false` when `spi_instance` is invalid.
	 */
	bool init_audio_cv(spi_inst_t* spi_instance = spi0, uint cs_pin = GPIO_BRAIN_AUDIO_CV_OUT_CS,
		uint sck_pin = GPIO_BRAIN_AUDIO_CV_OUT_SCK, uint tx_pin = GPIO_BRAIN_AUDIO_CV_OUT_TX,
		uint coupling_pin_a = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_A,
		uint coupling_pin_b = GPIO_BRAIN_AUDIO_CV_OUT_COUPLING_B,
		AudioCvOutRange range_a = kOutputsRange0To10V,
		AudioCvOutRange range_b = kOutputsRange0To10V);

	/**
	 * @brief Replaces the `Storage` backend used for calibration load/save operations.
	 * @param storage Pointer to caller-owned `Storage`. If `nullptr`, the current backend is kept unchanged.
	 */
	void set_dependencies(Storage* storage);

	/**
	 * @brief Initializes the digital pulse output GPIO and resets logical pulse state to LOW.
	 */
	void init_pulse();

	/**
	 * @brief Convenience initializer that calls `init_pulse()` and then `init_audio_cv()` with defaults.
	 * @return `true` when both subsystems initialize; `false` when audio/CV init fails.
	 */
	bool init();

	/**
	 * @brief Writes an uncalibrated output voltage to one DAC channel.
	 * @param channel Output channel:
	 * - `kOutputsChannelA`
	 * - `kOutputsChannelB`
	 * @param millivolts Requested output voltage in mV, validated against the channel's current range.
	 * @return `true` when the value is in-range and written to DAC; `false` if audio/CV is not initialized or
	 * the requested value is out of range.
	 */
	bool set_voltage_millivolts(AudioCvOutChannel channel, int32_t millivolts);

	/**
	 * @brief Writes voltage using the calibration table (with clamping) on one DAC channel.
	 * @param channel Output channel (`kOutputsChannelA` or `kOutputsChannelB`).
	 * @param target_millivolts Requested output voltage in mV. Out-of-range values are clamped to the channel range.
	 * @return `true` when audio/CV is initialized and DAC write succeeds; `false` if hardware is not initialized.
	 */
	bool set_voltage_calibrated_millivolts(AudioCvOutChannel channel, int32_t target_millivolts);

	/**
	 * @brief Loads calibration offsets into RAM for both CV channels.
	 * @param cal Calibration table with 10 offset points for channel A and 10 for channel B (LSB units).
	 * @return Always returns `true` after copying calibration into internal state.
	 */
	bool set_calibration(const CvCalibrationV1& cal);

	/**
	 * @brief Clears all calibration offsets and disables calibrated output compensation.
	 */
	void clear_calibration();

	/**
	 * @brief Loads CV calibration from `Storage` into `Outputs`.
	 * @return `true` when calibration record is present and valid; `false` when storage is unavailable,
	 * not initialized, or contains no valid calibration data.
	 */
	bool load_calibration_from_flash();

	/**
	 * @brief Reports whether `Outputs` currently has calibration data loaded.
	 * @return `true` after `set_calibration()` or successful `load_calibration_from_flash()`.
	 */
	bool has_calibration() const;

	/**
	 * @brief Reports whether audio/CV DAC path is initialized.
	 * @return `true` when SPI pins, coupling pins, and DAC communication are configured.
	 */
	bool is_audio_cv_initialized() const;

	/**
	 * @brief Reports whether pulse output GPIO is initialized.
	 * @return `true` when `init_pulse()` has run successfully.
	 */
	bool is_pulse_initialized() const;

	/**
	 * @brief Reports whether both pulse and audio/CV parts are initialized.
	 * @return `true` only when `is_audio_cv_initialized()` and `is_pulse_initialized()` are both true.
	 */
	bool is_initialized() const;

	/**
	 * @brief Returns the last raw DAC code written to a channel.
	 * @param channel Output channel (`kOutputsChannelA` or `kOutputsChannelB`).
	 * @return Last 12-bit DAC code (0..4095) sent for that channel.
	 */
	uint16_t get_last_dac_value(AudioCvOutChannel channel) const;

	/**
	 * @brief Returns the last requested voltage value for a channel.
	 * @param channel Output channel (`kOutputsChannelA` or `kOutputsChannelB`).
	 * @return Last requested mV value passed to a voltage write method for that channel
	 * (requested value, not necessarily clamped value).
	 */
	int32_t get_last_set_millivolts(AudioCvOutChannel channel) const;

	/**
	 * @brief Sets output range/coupling mode for one channel.
	 * @param channel Output channel (`kOutputsChannelA` or `kOutputsChannelB`).
	 * @param range New range:
	 * - `kOutputsRange0To10V`: unipolar range, accepted write range 0..10000 mV.
	 * - `kOutputsRangeMinus5To5V`: bipolar range, accepted write range -5000..5000 mV.
	 * @return Always returns `true`. If audio/CV is not initialized yet, the setting is stored and applied later.
	 */
	bool set_output_range(AudioCvOutChannel channel, AudioCvOutRange range);

	/**
	 * @brief Returns the currently configured range for one output channel.
	 * @param channel Output channel (`kOutputsChannelA` or `kOutputsChannelB`).
	 * @return Channel range enum currently stored in `Outputs`.
	 */
	AudioCvOutRange get_output_range(AudioCvOutChannel channel) const;

	/**
	 * @brief Sets logical pulse output state.
	 * @param on Logical gate state: `true` means gate high, `false` means gate low.
	 *
	 * Hardware line is active-low, so the physical GPIO level is inverted internally.
	 */
	void pulse_set(bool on);

	/**
	 * @brief Reads back the logical pulse/gate state.
	 * @return `true` when pulse output is logically high (gate on), `false` when logically low.
	 */
	bool pulse_get() const;

private:
	void write_dac_channel(AudioCvOutChannel channel, uint16_t dac_value);
	bool to_dac_input_millivolts(
		AudioCvOutChannel channel,
		int32_t requested_millivolts,
		bool clamp_to_range,
		int32_t* dac_input_millivolts) const;
	uint16_t millivolts_to_dac(int32_t dac_input_millivolts) const;
	int16_t interpolated_offset_lsb(AudioCvOutChannel channel, int32_t dac_input_millivolts) const;
	const char* range_to_string(AudioCvOutRange range) const;

	uint pulse_out_gpio_ = 0;
	bool pulse_initialized_ = false;
	bool pulse_state_ = false;

	uint cs_pin_ = 0;
	uint sck_pin_ = 0;
	uint tx_pin_ = 0;
	uint coupling_pin_a_ = 0;
	uint coupling_pin_b_ = 0;
	spi_inst_t* spi_instance_ = nullptr;
	bool audio_cv_initialized_ = false;

	bool calibration_loaded_ = false;
	int16_t calibration_a_offset_lsb_[10] = {0};
	int16_t calibration_b_offset_lsb_[10] = {0};
	Storage* storage_ = nullptr;
	Storage default_storage_{};

	AudioCvOutRange range_a_ = kOutputsRange0To10V;
	AudioCvOutRange range_b_ = kOutputsRange0To10V;
	int32_t last_set_millivolts_a_ = 0;
	int32_t last_set_millivolts_b_ = 0;
	uint16_t last_dac_value_a_ = 0;
	uint16_t last_dac_value_b_ = 0;
};

using AudioCvOut = Outputs;
