# MIDI Parser Utility

`MidiParser` turns incoming MIDI bytes into useful musical events you can consume in firmware logic.

It handles:

- Note On / Note Off
- Control Change (CC)
- Pitch Bend
- Realtime messages

You can feed it bytes manually, or connect it to UART and let it parse stream data continuously.


## MIDI input parsing
`MidiParser` supports two workflows:

- **Manual feed**: call `parse(byte)` yourself
- **UART feed**: initialize UART, then call `process_uart()` or `process_uart_budgeted(...)` in loop

Channel filtering is built in through channel + omni settings.

## Example for MIDI parsing
```cpp
#define BRAIN_USE_MIDI_PARSER 1
#include "brain/brain.h"

#include <pico/stdlib.h>
#include <stdio.h>

Brain brain;

static void on_note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
	printf("Note On  ch=%u note=%u vel=%u\n", channel, note, velocity);
}

static void on_note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
	printf("Note Off ch=%u note=%u vel=%u\n", channel, note, velocity);
}

int main() {
	stdio_init_all();

	BrainInitStatus status = brain.init_midi_parser(31250);
	if (!brain_init_succeeded(status)) return 1;

	brain.midi_parser.set_note_on_callback(on_note_on);
	brain.midi_parser.set_note_off_callback(on_note_off);

	while (true) {
		// Keep parser consuming UART MIDI data
		brain.midi_parser.process_uart_budgeted(64);
		sleep_ms(1);
	}
}
```


## MIDI parser API methods categorized and explained

### Construction and reset
- `MidiParser(uint8_t channel = 1, bool omni = false)`
  Creates parser with initial channel filter and omni mode.
- `void reset()`
  Clears parser state machine/running status state.

### Channel filtering
- `void set_channel(uint8_t ch)`
  Sets MIDI channel filter.
- `uint8_t channel() const`
  Returns active channel filter.
- `void set_omni(bool enabled)`
  Enables/disables omni mode.
- `bool omni() const`
  Returns omni mode state.

### Parsing and stream consumption
- `void parse(uint8_t byte) noexcept`
  Parses one MIDI byte manually.
- `bool init_uart(uint32_t baud_rate = 31250)`
  Initializes default UART path.
- `bool init_uart(uart_inst_t* uart, uint8_t rx_gpio, uint32_t baud_rate = 31250)`
  Initializes parser with explicit UART instance + RX pin.
- `void process_uart()`
  Consumes available UART bytes with no explicit byte budget.
- `void process_uart_budgeted(uint16_t byte_budget)`
  Consumes up to `byte_budget` bytes per call (good for bounded loop timing).
- `bool is_uart_initialized() const`
  Reports UART initialization status.

### Event callbacks
- `void set_note_on_callback(NoteOnCallback callback)`
- `void set_note_off_callback(NoteOffCallback callback)`
- `void set_control_change_callback(ControlChangeCallback callback)`
- `void set_pitch_bend_callback(PitchBendCallback callback)`
- `void set_realtime_callback(RealtimeCallback callback)`

Each callback receives decoded event data and MIDI channel context.

## Manual feed example
```cpp
#include "brain/include/midi-parser.h"

MidiParser parser(1, false);

int main() {
	parser.set_note_on_callback([](uint8_t note, uint8_t vel, uint8_t ch) {
		(void)note; (void)vel; (void)ch;
	});

	// Simulate Note On: status + note + velocity
	parser.parse(0x90); // channel 1 note on
	parser.parse(60);   // middle C
	parser.parse(100);  // velocity
}
```

## UART feed example
```cpp
#include "brain/include/midi-parser.h"

MidiParser parser;

int main() {
	if (!parser.init_uart(31250)) return 1;

	while (true) {
		parser.process_uart_budgeted(64);
	}
}
```

## API usage notes
- Call parser processing regularly in your main loop to avoid event latency.
- If messages seem missing, verify channel filter (`set_channel`) and omni mode (`set_omni`).
- `process_uart_budgeted(...)` is usually better for predictable loop timing than unbounded processing.