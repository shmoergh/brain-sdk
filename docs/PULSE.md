# PULSE Component

## Overview
The Pulse component provides digital pulse input and output handling for the Brain module. It supports hardware-inverted transistor-driven I/O, edge detection, glitch filtering, and both polling and interrupt-driven operation.

## Features
- Combined input and output on separate GPIO pins
- Hardware inversion handled transparently (transistor-driven)
- Edge detection (rising/falling)
- Glitch filtering for input pulses
- Event callbacks for edge events
- Polling-based or interrupt-driven operation
- Eurorack-compatible (5V logic)

## Usage
1. **Initialization**: Create a `Pulse` instance with optional GPIO pins, then call `begin()`
2. **Input**: Use `read()` to get logical state, or register callbacks for edges
3. **Output**: Use `set()` to control output state
4. **Polling**: Call `poll()` in main loop for edge detection (if not using interrupts)
5. **Callbacks**: Register callbacks for rising/falling edge events

## Example - Basic Input/Output
```cpp
#include "brain-io/pulse.h"

// Create with default Brain module pins
brain::io::Pulse pulse;
pulse.begin();

while (true) {
    // Read input state (hardware inversion handled)
    bool input_state = pulse.read();

    // Set output state
    pulse.set(true);   // Output HIGH
    sleep_ms(100);
    pulse.set(false);  // Output LOW
    sleep_ms(100);
}
```

## Example - Edge Detection with Callbacks
```cpp
#include "brain-io/pulse.h"

brain::io::Pulse pulse;
pulse.begin();

// Callback for rising edge (low → high)
pulse.on_rise([]() {
    printf("Rising edge detected!\n");
});

// Callback for falling edge (high → low)
pulse.on_fall([]() {
    printf("Falling edge detected!\n");
});

while (true) {
    pulse.poll();  // Check for edges
    sleep_ms(1);
}
```

## Example - Interrupt-Driven Edge Detection
```cpp
#include "brain-io/pulse.h"

brain::io::Pulse pulse;
pulse.begin();

pulse.on_rise([]() {
    printf("Clock pulse!\n");
});

// Enable hardware interrupts for immediate response
pulse.enable_interrupts();

while (true) {
    // No polling needed - interrupts handle edge detection
    sleep_ms(100);
}
```

## Example - Glitch Filtering
```cpp
#include "brain-io/pulse.h"

brain::io::Pulse pulse;
pulse.begin();

// Filter out glitches shorter than 100µs
pulse.set_input_glitch_filter_us(100);

pulse.on_rise([]() {
    printf("Clean rising edge\n");
});

while (true) {
    pulse.poll();
    sleep_ms(1);
}
```

## Example - Custom GPIO Pins
```cpp
#include "brain-io/pulse.h"

// Custom input and output pins
brain::io::Pulse pulse(10, 11);  // Input: GPIO 10, Output: GPIO 11
pulse.begin();

while (true) {
    if (pulse.read()) {
        pulse.set(true);
    } else {
        pulse.set(false);
    }
}
```

## Example - Gate/Trigger Output
```cpp
#include "brain-io/pulse.h"

brain::io::Pulse gate_out;
gate_out.begin();

// Send gate on
gate_out.set(true);
sleep_ms(100);

// Send gate off
gate_out.set(false);

// Check current output state
bool is_high = gate_out.get();
```

## API Reference

### Constructor
```cpp
Pulse(uint in_gpio = GPIO_BRAIN_PULSE_INPUT,
      uint out_gpio = GPIO_BRAIN_PULSE_OUTPUT)
```
- `in_gpio`: GPIO pin for input (default: Brain module pulse input)
- `out_gpio`: GPIO pin for output (default: Brain module pulse output)

### Initialization
```cpp
void begin()
```
- Initialize GPIO pins and set safe output state (low)
- Must be called before using the Pulse object

```cpp
void end()
```
- Return pins to input/high-impedance state
- Use for cleanup or power management

### Input
```cpp
bool read() const
```
- Read logical input state (hardware inversion handled)
- Returns `true` when input is logically HIGH

```cpp
bool read_raw() const
```
- Read raw GPIO level (for debugging)
- Returns actual GPIO pin state (not inverted)

### Output
```cpp
void set(bool on)
```
- Set logical output state (hardware inversion handled)
- `true` = active/high, `false` = idle/low

```cpp
bool get() const
```
- Get last commanded logical output state
- Returns `true` if output was set to active

### Edge Detection
```cpp
void poll()
```
- Poll for edge detection and trigger callbacks
- Call regularly in main loop if not using interrupts

```cpp
void on_rise(std::function<void()> callback)
```
- Set callback for logical rising edge (low → high)

```cpp
void on_fall(std::function<void()> callback)
```
- Set callback for logical falling edge (high → low)

### Advanced Features
```cpp
void set_input_glitch_filter_us(uint32_t us)
```
- Set glitch filter duration in microseconds
- Filters out pulses shorter than specified duration
- `0` = disabled (default)

```cpp
void enable_interrupts()
```
- Enable interrupt-driven edge detection
- Provides immediate response without polling
- Callbacks triggered from ISR context

```cpp
void disable_interrupts()
```
- Disable interrupt-driven edge detection
- Return to polling-based operation

## Hardware Details

### Signal Inversion
- Brain module uses transistor-driven I/O (inverted signals)
- SDK handles inversion transparently:
  - `read()` returns logical state (not raw GPIO)
  - `set(true)` drives output HIGH logically
- Use `read_raw()` only for debugging hardware

### Input Circuit
- Transistor switch provides safety and level shifting
- Compatible with Eurorack 5V/0V signals
- Internal pull-up resistor configured automatically

### Output Circuit
- Transistor switch drives Eurorack loads
- Safe output current for typical Eurorack inputs
- Logic level: 0V (low) to 5V (high)

## Performance Notes

### Polling Mode
- Call `poll()` at least every 1ms for responsive edge detection
- Suitable for most applications
- Lower CPU overhead than interrupts

### Interrupt Mode
- Immediate response to edges (sub-microsecond latency)
- Callbacks execute in ISR context - keep them short!
- Better for time-critical applications (clock sync, etc.)

### Glitch Filtering
- Hardware and software filtering available
- Prevents false triggers from noise or contact bounce
- Typical value: 50-200µs for mechanical switches
- Lower values for clean digital signals

## Common Use Cases
- **Clock Input**: Receive tempo/sync from external sequencer
- **Gate Output**: Send note gates to envelope generators
- **Trigger Input**: Detect drum triggers or events
- **Gate Input**: Track note gates from keyboard/sequencer
- **Clock Output**: Send tempo to other modules
- **Trigger Output**: Fire one-shots or drum sounds

## Important Notes
- Designed for Eurorack pulse signals (0-5V)
- Hardware inversion is handled transparently
- Call `begin()` before use, `end()` for cleanup
- In polling mode, call `poll()` regularly
- In interrupt mode, keep callbacks short (ISR context)
- Glitch filtering adds latency (usually acceptable)
- Compatible with gates, triggers, and clocks
- Both input and output can be used simultaneously
