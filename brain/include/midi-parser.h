#pragma once

#include <cstdint>

#include "ringbuffer.h"

typedef struct uart_inst uart_inst_t;

class MidiParser {
public:
	using NoteOnCallback = void (*)(uint8_t note, uint8_t velocity, uint8_t channel);
	using NoteOffCallback = void (*)(uint8_t note, uint8_t velocity, uint8_t channel);
	using ControlChangeCallback = void (*)(uint8_t cc, uint8_t value, uint8_t channel);
	using PitchBendCallback = void (*)(int16_t value, uint8_t channel);
	using RealtimeCallback = void (*)(uint8_t status);

	/**
	 * @brief Creates a MIDI byte parser with channel filter and optional omni mode.
	 * @param channel MIDI channel filter in user-facing 1..16 format. Values outside range are clamped.
	 * @param omni `true` accepts channel messages from all channels. `false` accepts only `channel`.
	 */
	explicit MidiParser(uint8_t channel = 1, bool omni = false);

	/**
	 * @brief Resets parser state machine and clears any partially received MIDI message.
	 *
	 * Callback registrations and channel/omni settings are kept.
	 */
	void reset();

	/**
	 * @brief Sets MIDI channel filter.
	 * @param ch Target channel in 1..16 format. Values <1 become 1; values >16 become 16.
	 */
	void set_channel(uint8_t ch);

	/**
	 * @brief Returns the currently selected MIDI channel filter.
	 * @return Channel filter value in 1..16 format.
	 */
	uint8_t channel() const;

	/**
	 * @brief Enables/disables omni receive mode.
	 * @param enabled `true` accepts channel messages from all MIDI channels.
	 * `false` applies the configured `channel()` filter.
	 */
	void set_omni(bool enabled);

	/**
	 * @brief Returns whether all MIDI channels are accepted.
	 * @return `true` when channel filtering is bypassed.
	 */
	bool omni() const;

	/**
	 * @brief Feeds one raw MIDI byte into the parser state machine.
	 * @param byte Raw byte from MIDI stream (status or data).
	 *
	 * Handles running status, filters system-common bytes, and dispatches callbacks for supported messages.
	 */
	void parse(uint8_t byte) noexcept;

	/**
	 * @brief Initializes default Brain MIDI UART input.
	 * @param baud_rate Serial baud rate in bits per second.
	 * @return `true` when default UART and GPIO configuration succeeds.
	 */
	bool init_uart(uint32_t baud_rate = 31250);

	/**
	 * @brief Initializes a specific UART instance and RX pin for MIDI input.
	 * @param uart UART peripheral instance to bind for MIDI input.
	 * @param rx_gpio GPIO identifier used for this signal.
	 * @param baud_rate Serial baud rate in bits per second.
	 * @return `true` when UART pointer is valid and initialization succeeds; `false` when `uart` is `nullptr`.
	 */
	bool init_uart(uart_inst_t* uart, uint8_t rx_gpio, uint32_t baud_rate = 31250);

	/**
	 * @brief Reads and parses all currently available UART MIDI bytes.
	 */
	void process_uart();

	/**
	 * @brief Processes UART MIDI bytes with an explicit per-call work budget.
	 * @param byte_budget Maximum bytes to read and maximum bytes to parse in this call.
	 * `0` means unbounded (process until UART and parser queues are empty).
	 */
	void process_uart_budgeted(uint16_t byte_budget);

	/**
	 * @brief Reports whether UART MIDI input is initialized.
	 * @return `true` after successful `init_uart(...)`.
	 */
	bool is_uart_initialized() const;

	/**
	 * @brief Registers callback for MIDI Note On messages.
	 * @param callback Called with `(note, velocity, channel)` where channel is 1..16.
	 * Note On with velocity 0 is emitted as Note Off and will not trigger this callback.
	 */
	void set_note_on_callback(NoteOnCallback callback);

	/**
	 * @brief Registers callback for MIDI Note Off events.
	 * @param callback Called with `(note, velocity, channel)` where channel is 1..16.
	 * Also receives Note On messages with velocity 0.
	 */
	void set_note_off_callback(NoteOffCallback callback);

	/**
	 * @brief Registers callback for MIDI Control Change messages.
	 * @param callback Called with `(cc, value, channel)` where `cc` and `value` are 0..127 and channel is 1..16.
	 */
	void set_control_change_callback(ControlChangeCallback callback);

	/**
	 * @brief Registers callback for MIDI Pitch Bend messages.
	 * @param callback Called with `(value, channel)` where `value` is signed -8192..+8191 and channel is 1..16.
	 */
	void set_pitch_bend_callback(PitchBendCallback callback);

	/**
	 * @brief Registers callback for MIDI real-time status bytes (`0xF8` and above).
	 * @param callback Called with the raw real-time status byte.
	 */
	void set_realtime_callback(RealtimeCallback callback);

private:
	enum class State : uint8_t { Idle, AwaitData1, AwaitData2 };

	static constexpr uint8_t kNoteOffMask = 0x80;
	static constexpr uint8_t kNoteOnMask = 0x90;
	static constexpr uint8_t kControlChangeMask = 0xB0;
	static constexpr uint8_t kPitchBendMask = 0xE0;
	static constexpr uint8_t kChannelMask = 0x0F;
	static constexpr uint8_t kStatusMask = 0xF0;
	static constexpr uint8_t kDataMask = 0x7F;
	static constexpr uint8_t kRealtimeMin = 0xF8;
	static constexpr uint8_t kSystemCommonMin = 0xF0;
	static constexpr uint8_t kSystemCommonMax = 0xF7;
	static constexpr uint16_t kBufferSize = 120;

	static constexpr bool is_status_byte(uint8_t byte) {
		return (byte & 0x80) != 0;
	}

	static constexpr bool is_data_byte(uint8_t byte) {
		return (byte & 0x80) == 0;
	}

	static constexpr bool is_realtime_byte(uint8_t byte) {
		return byte >= kRealtimeMin;
	}

	static constexpr bool is_system_common_byte(uint8_t byte) {
		return byte >= kSystemCommonMin && byte <= kSystemCommonMax;
	}

	static constexpr uint8_t get_status_channel(uint8_t status) {
		return status & kChannelMask;
	}

	static constexpr uint8_t get_status_type(uint8_t status) {
		return status & kStatusMask;
	}

	bool should_process_channel(uint8_t messageChannel) const;
	void process_message();
	void handle_realtime_byte(uint8_t byte);
	uint8_t get_expected_data_bytes(uint8_t status) const;

	brain::utils::RingBuffer buffer_;
	uint8_t data_buffer_[kBufferSize];
	State state_ = State::Idle;
	uint8_t running_status_ = 0;
	uint8_t current_status_ = 0;
	uint8_t data_[2] = {0, 0};
	uint8_t expected_data_bytes_ = 0;

	uint8_t channel_filter_ = 1;
	bool omni_mode_ = false;
	uart_inst_t* uart_ = nullptr;
	bool uart_initialized_ = false;

	NoteOnCallback note_on_callback_ = nullptr;
	NoteOffCallback note_off_callback_ = nullptr;
	ControlChangeCallback control_change_callback_ = nullptr;
	PitchBendCallback pitch_bend_callback_ = nullptr;
	RealtimeCallback realtime_callback_ = nullptr;
};
