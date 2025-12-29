#pragma once

#include <cstdint>

#include "brain-utils/ringbuffer.h"

// Forward declarations for UART types
typedef struct uart_inst uart_inst_t;

namespace brain::io {

/**
 * @brief MIDI parser with integrated UART input for channel voice messages.
 * Handles UART MIDI input and parsing with channel filtering and Omni mode support.
 * ISR-safe parse() method for real-time parsing or use initUart() for integrated UART handling.
 */
class MidiParser {

public:
	// Callback function types
	using NoteOnCallback = void (*)(uint8_t note, uint8_t velocity, uint8_t channel);
	using NoteOffCallback = void (*)(uint8_t note, uint8_t velocity, uint8_t channel);
	using ControlChangeCallback = void (*)(uint8_t cc, uint8_t value, uint8_t channel);
	using PitchBendCallback = void (*)(int16_t value, uint8_t channel);
	using RealtimeCallback = void (*)(uint8_t status);

	/**
	 * @brief Constructor with optional configuration
	 * @param channel Channel to filter (1-16), default: 1
	 * @param omni If true, accept all channels, default: false
	 */
	explicit MidiParser(uint8_t channel = 1, bool omni = false);

	/**
	 * @brief Reset parser state and running status
	 */
	void reset();

	/**
	 * @brief Set MIDI channel filter (1-16)
	 * @param ch Channel number, clamped to 1-16 range
	 */
	void set_channel(uint8_t ch);

	/**
	 * @brief Get current channel filter
	 * @return Channel number (1-16)
	 */
	uint8_t channel() const;

	/**
	 * @brief Enable/disable Omni mode
	 * @param enabled If true, accept messages from all channels
	 */
	void set_omni(bool enabled);

	/**
	 * @brief Check if Omni mode is enabled
	 * @return True if Omni mode is enabled
	 */
	bool omni() const;

	/**
	 * @brief Feed a raw MIDI byte to the parser
	 * @param byte Raw MIDI byte (ISR-safe, noexcept)
	 */
	void parse(uint8_t byte) noexcept;

	/**
	 * @brief Initialize UART for MIDI input using default Brain module GPIO pins
	 * Uses GPIO_BRAIN_MIDI_RX and uart1 by default
	 * @param baud_rate MIDI baud rate (default: 31250)
	 * @return true if initialization successful
	 */
	bool init_uart(uint32_t baud_rate = 31250);

	/**
	 * @brief Initialize UART for MIDI input (optional integrated approach)
	 * @param uart UART instance (e.g., uart0, uart1)
	 * @param rx_gpio GPIO pin for UART RX
	 * @param baud_rate MIDI baud rate (default: 31250)
	 * @return true if initialization successful
	 */
	bool init_uart(uart_inst_t* uart, uint8_t rx_gpio, uint32_t baud_rate = 31250);

	/**
	 * @brief Process any available UART MIDI input (call regularly in main loop)
	 * Only works if initUart() was called first
	 */
	void process_uart();

	/**
	 * @brief Check if UART MIDI input is initialized and ready
	 * @return true if UART is initialized
	 */
	bool is_uart_initialized() const;

	/**
	 * @brief Set callback for Note On messages
	 */
	void set_note_on_callback(NoteOnCallback callback);

	/**
	 * @brief Set callback for Note Off messages
	 */
	void set_note_off_callback(NoteOffCallback callback);

	/**
	 * @brief Set callback for Control Change messages
	 */
	void set_control_change_callback(ControlChangeCallback callback);

	/**
	 * @brief Set callback for Pitch Bend messages
	 */
	void set_pitch_bend_callback(PitchBendCallback callback);

	/**
	 * @brief Set callback for Real-time messages (optional)
	 */
	void set_realtime_callback(RealtimeCallback callback);

private:
	// Parser state machine states
	enum class State : uint8_t { Idle, AwaitData1, AwaitData2 };

	// MIDI status byte constants
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

	// Check if byte is a status byte
	static constexpr bool is_status_byte(uint8_t byte) {
		return (byte & 0x80) != 0;
	}

	// Check if byte is a data byte
	static constexpr bool is_data_byte(uint8_t byte) {
		return (byte & 0x80) == 0;
	}

	// Check if byte is a real-time message
	static constexpr bool is_realtime_byte(uint8_t byte) {
		return byte >= kRealtimeMin;
	}

	// Check if byte is a system common message
	static constexpr bool is_system_common_byte(uint8_t byte) {
		return byte >= kSystemCommonMin && byte <= kSystemCommonMax;
	}

	// Get channel from status byte (0-15)
	static constexpr uint8_t get_status_channel(uint8_t status) {
		return status & kChannelMask;
	}

	// Get status type from status byte
	static constexpr uint8_t get_status_type(uint8_t status) {
		return status & kStatusMask;
	}

	// Check if message should be processed based on channel filter
	bool should_process_channel(uint8_t messageChannel) const;

	// Process a complete MIDI message
	void process_message();

	// Handle real-time byte
	void handle_realtime_byte(uint8_t byte);

	// Get expected data bytes for status
	uint8_t get_expected_data_bytes(uint8_t status) const;

	// State
	brain::utils::RingBuffer buffer_;
	uint8_t data_buffer_[kBufferSize];
	State state_ = State::Idle;
	uint8_t running_status_ = 0;
	uint8_t current_status_ = 0;
	uint8_t data_[2] = {0, 0};
	uint8_t expected_data_bytes_ = 0;

	// Configuration
	uint8_t channel_filter_ = 1;  // 1-16
	bool omni_mode_ = false;

	// UART configuration (when using integrated UART)
	uart_inst_t* uart_ = nullptr;
	bool uart_initialized_ = false;

	// Callbacks
	NoteOnCallback note_on_callback_ = nullptr;
	NoteOffCallback note_off_callback_ = nullptr;
	ControlChangeCallback control_change_callback_ = nullptr;
	PitchBendCallback pitch_bend_callback_ = nullptr;
	RealtimeCallback realtime_callback_ = nullptr;
};

}  // namespace brain::io
