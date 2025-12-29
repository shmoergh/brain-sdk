#include "brain-io/midi-parser.h"

#include <hardware/gpio.h>
#include <hardware/uart.h>
#include <hardware/structs/uart.h>
#include <cstdio>

#include "brain-common/brain-gpio-setup.h"

// Debug flag for MIDI parser internals
// Set to 1 to enable detailed MIDI byte logging (useful for debugging hardware issues)
// Set to 0 for production (reduces overhead and prevents blocking)
#define DEBUG_MIDI_PARSER 0

namespace brain::io {

MidiParser::MidiParser(uint8_t channel, bool omni) : channel_filter_(channel), omni_mode_(omni) {
	set_channel(channel);  // Clamp to valid range
	reset();
	buffer_.init(data_buffer_, kBufferSize);
}

void MidiParser::reset() {
	state_ = State::Idle;
	running_status_ = 0;
	current_status_ = 0;
	data_[0] = 0;
	data_[1] = 0;
	expected_data_bytes_ = 0;
}

void MidiParser::set_channel(uint8_t ch) {
	// Clamp to valid MIDI channel range (1-16)
	if (ch < 1) {
		channel_filter_ = 1;
	} else if (ch > 16) {
		channel_filter_ = 16;
	} else {
		channel_filter_ = ch;
	}
}

uint8_t MidiParser::channel() const {
	return channel_filter_;
}

void MidiParser::set_omni(bool enabled) {
	omni_mode_ = enabled;
}

bool MidiParser::omni() const {
	return omni_mode_;
}

void MidiParser::parse(uint8_t byte) noexcept {
	// Handle real-time messages immediately at any time
	if (is_realtime_byte(byte)) {
		handle_realtime_byte(byte);
		return;
	}

	// Ignore System Common messages (SysEx, etc.) for v1
	if (is_system_common_byte(byte)) {
		reset();  // Clear any partial message
		return;
	}

	// Handle status bytes
	if (is_status_byte(byte)) {

		// New status byte received
		current_status_ = byte;
		running_status_ = byte;	 // Update running status
		expected_data_bytes_ = get_expected_data_bytes(byte);

		if (expected_data_bytes_ == 0) {
			// No data bytes expected, process immediately
			process_message();
			state_ = State::Idle;
		} else {
			state_ = State::AwaitData1;
		}

	// Handle data byte
	} else if (is_data_byte(byte)) {

		// Data byte received
		switch (state_) {
			case State::Idle:
				// Use running status if available
				if (running_status_ != 0) {
					current_status_ = running_status_;
					expected_data_bytes_ = get_expected_data_bytes(current_status_);
					data_[0] = byte;

					if (expected_data_bytes_ == 1) {
						process_message();
						state_ = State::Idle;
					} else {
						state_ = State::AwaitData2;
					}
				}
				break;

			case State::AwaitData1:
				data_[0] = byte;

				if (expected_data_bytes_ == 1) {
					process_message();
					state_ = State::Idle;
				} else {
					state_ = State::AwaitData2;
				}
				break;

			case State::AwaitData2:
				data_[1] = byte;
				process_message();
				state_ = State::Idle;
				break;
		}
	} else {
		reset();
	}
}

void MidiParser::set_note_on_callback(NoteOnCallback callback) {
	note_on_callback_ = callback;
}

void MidiParser::set_note_off_callback(NoteOffCallback callback) {
	note_off_callback_ = callback;
}

void MidiParser::set_control_change_callback(ControlChangeCallback callback) {
	control_change_callback_ = callback;
}

void MidiParser::set_pitch_bend_callback(PitchBendCallback callback) {
	pitch_bend_callback_ = callback;
}

void MidiParser::set_realtime_callback(RealtimeCallback callback) {
	realtime_callback_ = callback;
}

bool MidiParser::init_uart(uint32_t baud_rate) {
	// Use default Brain module configuration: UART1 with GPIO_BRAIN_MIDI_RX
	return init_uart(uart1, GPIO_BRAIN_MIDI_RX, baud_rate);
}

bool MidiParser::init_uart(uart_inst_t* uart, uint8_t rx_gpio, uint32_t baud_rate) {
	if (uart == nullptr) {
		return false;
	}

	uart_ = uart;

	// Initialize UART for MIDI input
	uart_init(uart_, baud_rate);

	// Set up GPIO pin for MIDI RX
	gpio_set_function(rx_gpio, GPIO_FUNC_UART);

	// Set UART format for MIDI (8 data bits, 1 stop bit, no parity)
	uart_set_format(uart_, 8, 1, UART_PARITY_NONE);

	// Enable UART FIFOs to handle burst MIDI data
	// This is critical for fast MIDI messages (e.g., rapid note on/off)
	uart_set_fifo_enabled(uart_, true);

	// Disable hardware flow control
	uart_set_hw_flow(uart_, false, false);

	uart_initialized_ = true;
	return true;
}

void MidiParser::process_uart() {
	if (!uart_initialized_ || uart_ == nullptr) {
		return;
	}

	// UART error bits mask for efficient error checking
	static constexpr uint32_t kUartErrorMask =
		UART_UARTDR_OE_BITS | UART_UARTDR_BE_BITS |
		UART_UARTDR_PE_BITS | UART_UARTDR_FE_BITS;

	// Read any available MIDI bytes and feed them to the parser
	while (uart_is_readable(uart_)) {

		// Read the byte - this also reads the error flags atomically
		uint32_t data_reg = uart_get_hw(uart_)->dr;
		uint8_t data = data_reg & 0xFF;

		// Check for UART errors (these are in the same register read)
		if (data_reg & kUartErrorMask) {
			reset();
			continue;
		}

		// If there's no error, write the data to the ringbuffer
		if (!buffer_.write_byte(data)) {
			// Handle buffer overflow
		}
	}

	// Read the buffer and process it
	while (!buffer_.is_empty()) {
		uint8_t byte = 0;
		buffer_.read_byte(byte);
		parse(byte);
	}
}

bool MidiParser::is_uart_initialized() const {
	return uart_initialized_;
}

bool MidiParser::should_process_channel(uint8_t messageChannel) const {
	if (omni_mode_) {
		return true;
	}
	// messageChannel is 0-15, channel_filter_ is 1-16
	return (messageChannel + 1) == channel_filter_;
}

void MidiParser::process_message() {
	uint8_t status_type = get_status_type(current_status_);
	uint8_t message_channel = get_status_channel(current_status_);

	// Check channel filter
	if (!should_process_channel(message_channel)) {
		return;
	}

	// Convert channel from 0-15 to 1-16 for callbacks
	uint8_t callback_channel = message_channel + 1;

	switch (status_type) {
		case kNoteOnMask: {
			uint8_t note = data_[0];
			uint8_t velocity = data_[1];

			// Treat Note On with velocity 0 as Note Off per MIDI spec
			if (velocity == 0) {
				if (note_off_callback_) {
					note_off_callback_(note, velocity, callback_channel);
				}
			} else {
				if (note_on_callback_) {
					note_on_callback_(note, velocity, callback_channel);
				}
			}
			break;
		}

		case kNoteOffMask: {
			if (note_off_callback_) {
				uint8_t note = data_[0];
				uint8_t velocity = data_[1];
				note_off_callback_(note, velocity, callback_channel);
			}
			break;
		}

		case kControlChangeMask: {
			if (control_change_callback_) {
				uint8_t cc = data_[0];
				uint8_t value = data_[1];
				control_change_callback_(cc, value, callback_channel);
			}
			break;
		}

		case kPitchBendMask: {
			if (pitch_bend_callback_) {
				uint8_t lsb = data_[0];
				uint8_t msb = data_[1];

				// Combine to 14-bit value (0..16383)
				uint16_t bend_value = (static_cast<uint16_t>(msb) << 7) | lsb;

				// Convert to signed range (-8192..+8191)
				int16_t signed_bend = static_cast<int16_t>(bend_value) - 8192;

				pitch_bend_callback_(signed_bend, callback_channel);
			}
			break;
		}

		default:
			// Unknown or unsupported message type
			break;
	}
}

void MidiParser::handle_realtime_byte(uint8_t byte) {
	if (realtime_callback_) {
		realtime_callback_(byte);
	}
}

uint8_t MidiParser::get_expected_data_bytes(uint8_t status) const {
	switch (get_status_type(status)) {
		case kNoteOnMask:
		case kNoteOffMask:
		case kControlChangeMask:
		case kPitchBendMask:
			return 2;

		default:
			return 0;
	}
}

}  // namespace brain::io
