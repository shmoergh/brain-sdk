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

	explicit MidiParser(uint8_t channel = 1, bool omni = false);

	void reset();
	void set_channel(uint8_t ch);
	uint8_t channel() const;
	void set_omni(bool enabled);
	bool omni() const;
	void parse(uint8_t byte) noexcept;
	bool init_uart(uint32_t baud_rate = 31250);
	bool init_uart(uart_inst_t* uart, uint8_t rx_gpio, uint32_t baud_rate = 31250);
	void process_uart();
	void process_uart_budgeted(uint16_t byte_budget);
	bool is_uart_initialized() const;

	void set_note_on_callback(NoteOnCallback callback);
	void set_note_off_callback(NoteOffCallback callback);
	void set_control_change_callback(ControlChangeCallback callback);
	void set_pitch_bend_callback(PitchBendCallback callback);
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

