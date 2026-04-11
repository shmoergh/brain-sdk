# MidiParser

## Overview
`MidiParser` parses MIDI bytes and optionally reads UART directly.

Supported callbacks:
- note on/off
- control change
- pitch bend
- realtime messages

## Include
```cpp
#include "brain/include/midi-parser.h"
```

## Quick Start (UART)
```cpp
MidiParser parser(1, false); // channel 1, not omni

parser.set_note_on_callback([](uint8_t note, uint8_t vel, uint8_t ch) {
	printf("NoteOn n=%u v=%u ch=%u\n", note, vel, ch);
});

if (!parser.init_uart()) {
	return;
}

while (true) {
	parser.process_uart();
	sleep_ms(1);
}
```

## Manual Feed
```cpp
MidiParser parser(1);
parser.parse(0x90); // note on ch1
parser.parse(60);
parser.parse(100);
```

## API

### Setup
- `MidiParser(uint8_t channel = 1, bool omni = false)`
- `void reset()`
- `void set_channel(uint8_t ch)`
- `uint8_t channel() const`
- `void set_omni(bool enabled)`
- `bool omni() const`

### Parse / UART
- `void parse(uint8_t byte) noexcept`
- `bool init_uart(uint32_t baud_rate = 31250)`
- `bool init_uart(uart_inst_t* uart, uint8_t rx_gpio, uint32_t baud_rate = 31250)`
- `void process_uart()`
- `void process_uart_budgeted(uint16_t byte_budget)`
- `bool is_uart_initialized() const`

### Callbacks
- `set_note_on_callback(...)`
- `set_note_off_callback(...)`
- `set_control_change_callback(...)`
- `set_pitch_bend_callback(...)`
- `set_realtime_callback(...)`

## Defaults
- `init_uart()` uses `uart1` on `GPIO_BRAIN_MIDI_RX`.
- In current pin config this is GPIO `9`.
