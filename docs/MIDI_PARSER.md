# MIDI Parser (`brain::io::MidiParser`)

## Overview
The MIDI Parser component provides comprehensive MIDI message parsing with integrated UART support for Brain module hardware. It handles channel voice messages, real-time messages, and maintains proper MIDI state machine behavior including running status.

## Features
- **Message Types**:
  - Note On/Off with velocity
  - Control Change (CC)
  - Pitch Bend (14-bit, -8192 to +8191)
  - Real-time messages (clock, start, stop, etc.)
- **Channel Filtering**: Filter by channel (1-16) or use Omni mode
- **Running Status**: Full MIDI spec compliance
- **ISR-Safe Parsing**: `parse()` method safe to call from interrupts
- **Integrated UART**: Built-in UART handling for MIDI DIN input
- **Ring Buffer**: Internal buffering for reliable data handling
- **Transport-Agnostic**: Can parse from any data source

## Usage

### Basic Setup with Integrated UART (Recommended)
1. Create `MidiParser` instance with channel and omni settings
2. Set callback functions for desired message types
3. Call `init_uart()` to initialize UART hardware
4. Call `process_uart()` in main loop

### Advanced: Manual Byte Feeding
1. Create `MidiParser` instance
2. Set callback functions
3. Feed bytes manually using `parse()` from your data source

## Example - Integrated UART (Simple)
```cpp
#include "brain-io/midi-parser.h"

void on_note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
    printf("Note On: %d, vel: %d, ch: %d\n", note, velocity, channel);
}

void on_note_off(uint8_t note, uint8_t velocity, uint8_t channel) {
    printf("Note Off: %d, ch: %d\n", note, channel);
}

// Listen to MIDI channel 1
brain::io::MidiParser parser(1, false);

parser.set_note_on_callback(on_note_on);
parser.set_note_off_callback(on_note_off);

// Initialize with default Brain module UART settings
if (parser.init_uart()) {
    while (true) {
        parser.process_uart();  // Process incoming MIDI
        sleep_ms(1);
    }
}
```

## Example - Custom UART Configuration
```cpp
#include "brain-io/midi-parser.h"
#include <hardware/uart.h>

brain::io::MidiParser parser(1, false);
parser.set_note_on_callback(on_note_on);

// Custom UART: uart0, GPIO 17 for RX, 31250 baud
if (parser.init_uart(uart0, 17, 31250)) {
    while (true) {
        parser.process_uart();
        sleep_ms(1);
    }
}
```

## Example - Omni Mode (All Channels)
```cpp
#include "brain-io/midi-parser.h"

void on_note_on(uint8_t note, uint8_t velocity, uint8_t channel) {
    printf("Ch %d: Note %d ON\n", channel, note);
}

// Omni mode: accept messages from all channels
brain::io::MidiParser parser(1, true);  // true = omni mode
parser.set_note_on_callback(on_note_on);
parser.init_uart();

while (true) {
    parser.process_uart();
}
```

## Example - All Message Types
```cpp
#include "brain-io/midi-parser.h"

void on_cc(uint8_t cc, uint8_t value, uint8_t channel) {
    printf("CC %d = %d\n", cc, value);
}

void on_pitch_bend(int16_t value, uint8_t channel) {
    printf("Pitch Bend: %d\n", value);  // -8192 to +8191
}

void on_realtime(uint8_t status) {
    printf("Realtime: 0x%02X\n", status);
}

brain::io::MidiParser parser(1);
parser.set_note_on_callback(on_note_on);
parser.set_note_off_callback(on_note_off);
parser.set_control_change_callback(on_cc);
parser.set_pitch_bend_callback(on_pitch_bend);
parser.set_realtime_callback(on_realtime);

parser.init_uart();

while (true) {
    parser.process_uart();
}
```

## Example - Manual Byte Feeding (Advanced)
```cpp
#include "brain-io/midi-parser.h"

brain::io::MidiParser parser(1);
parser.set_note_on_callback(on_note_on);

// Feed bytes from any source (USB-MIDI, file, etc.)
parser.parse(0x90);  // Note On, channel 1
parser.parse(60);    // Middle C
parser.parse(64);    // Velocity 64

// Running status example
parser.parse(0x90);  // Note On, channel 1
parser.parse(60);    // Note
parser.parse(100);   // Velocity
parser.parse(62);    // Another note (running status)
parser.parse(100);   // Velocity
```

## Example - Channel Switching
```cpp
#include "brain-io/midi-parser.h"

brain::io::MidiParser parser(1);  // Start on channel 1
parser.init_uart();

// Later, change to channel 5
parser.set_channel(5);

// Or enable omni mode
parser.set_omni(true);

// Check current settings
uint8_t ch = parser.channel();  // Get current channel
bool is_omni = parser.omni();   // Check omni mode
```

## Example - Reset Parser
```cpp
#include "brain-io/midi-parser.h"

brain::io::MidiParser parser(1);
parser.init_uart();

// If MIDI stream gets corrupted or stuck
parser.reset();  // Clear all state and running status
```

## API Reference

### Constructor
```cpp
explicit MidiParser(uint8_t channel = 1, bool omni = false)
```
- `channel`: MIDI channel to filter (1-16), default: 1
- `omni`: If true, accept all channels, default: false

### UART Initialization
```cpp
bool init_uart(uint32_t baud_rate = 31250)
```
- Initialize with default Brain module UART pins
- Returns `true` if successful

```cpp
bool init_uart(uart_inst_t* uart, uint8_t rx_gpio, uint32_t baud_rate = 31250)
```
- Initialize with custom UART and GPIO
- `uart`: UART instance (uart0 or uart1)
- `rx_gpio`: GPIO pin for UART RX
- `baud_rate`: MIDI baud rate (standard: 31250)
- Returns `true` if successful

### UART Processing
```cpp
void process_uart()
```
- Process any available UART MIDI input
- Call regularly in main loop (every 1-10ms)
- Only works if `init_uart()` was called

```cpp
bool is_uart_initialized() const
```
- Check if UART is initialized and ready
- Returns `true` if UART was initialized successfully

### Manual Parsing
```cpp
void parse(uint8_t byte) noexcept
```
- Feed a raw MIDI byte to the parser
- ISR-safe, can be called from interrupt context
- Use for custom transports (USB-MIDI, etc.)

### Configuration
```cpp
void set_channel(uint8_t ch)
```
- Set MIDI channel filter (1-16)
- Clamped to valid range automatically

```cpp
uint8_t channel() const
```
- Get current channel filter
- Returns 1-16

```cpp
void set_omni(bool enabled)
```
- Enable/disable Omni mode
- `true` = accept all channels, `false` = filter by channel

```cpp
bool omni() const
```
- Check if Omni mode is enabled
- Returns `true` if enabled

```cpp
void reset()
```
- Reset parser state and running status
- Use when MIDI stream gets corrupted

### Callbacks
```cpp
void set_note_on_callback(NoteOnCallback callback)
```
- Signature: `void callback(uint8_t note, uint8_t velocity, uint8_t channel)`

```cpp
void set_note_off_callback(NoteOffCallback callback)
```
- Signature: `void callback(uint8_t note, uint8_t velocity, uint8_t channel)`

```cpp
void set_control_change_callback(ControlChangeCallback callback)
```
- Signature: `void callback(uint8_t cc, uint8_t value, uint8_t channel)`

```cpp
void set_pitch_bend_callback(PitchBendCallback callback)
```
- Signature: `void callback(int16_t value, uint8_t channel)`
- Value range: -8192 to +8191 (14-bit signed)

```cpp
void set_realtime_callback(RealtimeCallback callback)
```
- Signature: `void callback(uint8_t status)`
- For clock, start, stop, continue, etc.

## MIDI Message Details

### Note On/Off
- Note: 0-127 (MIDI note number)
- Velocity: 0-127
- Note On with velocity 0 is treated as Note Off per MIDI spec

### Control Change
- CC number: 0-127
- Value: 0-127

### Pitch Bend
- Value: -8192 to +8191 (14-bit signed)
- Center (no bend): 0
- Full down: -8192
- Full up: +8191

### Real-time Messages
- Status bytes: 0xF8-0xFF
- Clock (0xF8), Start (0xFA), Stop (0xFC), Continue (0xFB), etc.
- Interleaved with other messages, handled immediately

## Channel Filtering

### Single Channel Mode
- Default behavior
- Only processes messages on specified channel
- Other channels are ignored

### Omni Mode
- Accepts messages from all channels
- Channel parameter still passed to callbacks
- Useful for monitoring or multi-channel apps

## Performance Notes
- Ring buffer: 120 bytes (handles burst MIDI input)
- `parse()` is ISR-safe and noexcept
- `process_uart()` is non-blocking and fast
- Callbacks are synchronous (blocking)
- Keep callbacks short to maintain MIDI timing

## Hardware Configuration
Default Brain module MIDI input:
- UART: uart1
- RX GPIO: GPIO 5 (or GPIO_BRAIN_MIDI_RX)
- Baud rate: 31250 (MIDI standard)
- Data bits: 8, Stop bits: 1, Parity: None

## Common Use Cases
- **MIDI to CV**: Parse note messages for pitch CV conversion
- **MIDI Controller**: Respond to CC messages for parameter control
- **MIDI Sequencer**: Track note on/off for sequencing
- **MIDI Clock Sync**: Use realtime messages for tempo sync
- **MIDI Monitor**: Log all MIDI messages (omni mode)
- **Multi-timbral**: Process different channels separately

## Important Notes
- Designed for hardware MIDI DIN input (5-pin DIN connector)
- Handles running status automatically
- Filters system exclusive (SysEx) messages (not currently supported)
- Real-time messages can interleave with other data
- Ring buffer prevents data loss during processing
- Call `process_uart()` regularly (at least every 10ms)
- For higher-level MIDI-to-CV conversion, see [MIDI to CV](MIDI_TO_CV.md)

## Troubleshooting
- **No messages received**: Check UART GPIO pin, baud rate (31250)
- **Wrong notes**: Verify channel number (1-16, not 0-15)
- **Stuck notes**: Call `reset()` to clear parser state
- **Missing messages**: Increase `process_uart()` call frequency
- **Channel filtering not working**: Check omni mode setting