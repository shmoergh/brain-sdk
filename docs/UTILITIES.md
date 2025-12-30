# Brain Utilities

This document covers utility classes and helper functions in the Brain SDK.

---

## RingBuffer

### Overview
A thread-safe circular buffer (ring buffer) implementation for byte-oriented data. Useful for buffering data between interrupt handlers and main loop, or between different processing stages.

### Features
- Fixed-size circular buffer
- Thread-safe with volatile indices
- ISR-safe for single-producer, single-consumer scenarios
- No dynamic memory allocation (uses external buffer)
- Byte-oriented (uint8_t)
- Full/empty detection
- Peek without consuming

### Usage

#### Basic Setup
```cpp
#include "brain-utils/ringbuffer.h"

// Create buffer storage (256 bytes)
uint8_t buffer_storage[256];

// Create and initialize ring buffer
brain::utils::RingBuffer ring_buffer;
ring_buffer.init(buffer_storage, 256);
```

#### Writing Data
```cpp
uint8_t data = 0x42;
if (ring_buffer.write_byte(data)) {
    // Successfully written
} else {
    // Buffer was full
}
```

#### Reading Data
```cpp
uint8_t data;
if (ring_buffer.read_byte(data)) {
    // Successfully read, data now contains the byte
} else {
    // Buffer was empty
}
```

#### Peeking at Data
```cpp
uint8_t data;
if (ring_buffer.peek(data)) {
    // Successfully peeked, data contains next byte
    // But byte is still in buffer
} else {
    // Buffer was empty
}
```

#### Checking Buffer Status
```cpp
if (ring_buffer.is_empty()) {
    // No data available
}

if (ring_buffer.is_full()) {
    // Buffer is full, cannot write
}
```

### Example - UART ISR to Main Loop
```cpp
#include "brain-utils/ringbuffer.h"
#include <hardware/uart.h>

// Buffer for UART data
uint8_t uart_buffer_storage[512];
brain::utils::RingBuffer uart_buffer;

void uart_rx_isr() {
    while (uart_is_readable(uart0)) {
        uint8_t byte = uart_getc(uart0);
        uart_buffer.write_byte(byte);  // ISR-safe write
    }
}

int main() {
    uart_buffer.init(uart_buffer_storage, 512);

    // Set up UART and IRQ
    // ...

    while (true) {
        uint8_t byte;
        while (uart_buffer.read_byte(byte)) {  // Process all available bytes
            // Handle byte in main loop
            process_byte(byte);
        }
    }
}
```

### API Reference

#### Initialization
```cpp
void init(uint8_t* data_buffer, uint16_t buffer_size)
```
- Initialize ring buffer with external storage
- `data_buffer`: Pointer to byte array for storage
- `buffer_size`: Size of buffer in bytes (max 65535)

#### Writing
```cpp
bool write_byte(uint8_t data)
```
- Write one byte to buffer
- Returns `true` if successful, `false` if buffer full

#### Reading
```cpp
bool read_byte(uint8_t& data)
```
- Read and remove one byte from buffer
- `data`: Reference to store the read byte
- Returns `true` if successful, `false` if buffer empty

```cpp
bool peek(uint8_t& data) const
```
- Read byte without removing from buffer
- `data`: Reference to store the peeked byte
- Returns `true` if successful, `false` if buffer empty

#### Status
```cpp
bool is_full() const
```
- Check if buffer is full
- Returns `true` if full, `false` otherwise

```cpp
bool is_empty() const
```
- Check if buffer is empty
- Returns `true` if empty, `false` otherwise

### Important Notes
- Buffer size is one less than allocated size (circular buffer overhead)
- Thread-safe for single reader + single writer (e.g., ISR + main loop)
- Not safe for multiple readers or multiple writers without external locking
- Indices are volatile for ISR safety
- No built-in locking mechanism

---

## Helper Functions

### Overview
Collection of common utility functions for embedded development.

### Functions

#### map()
Maps a value from one range to another (similar to Arduino's map function).

```cpp
static inline long map(long x, long in_min, long in_max, long out_min, long out_max)
```

**Parameters:**
- `x`: Value to map
- `in_min`: Lower bound of input range
- `in_max`: Upper bound of input range
- `out_min`: Lower bound of output range
- `out_max`: Upper bound of output range

**Returns:** Mapped value

**Example:**
```cpp
#include "brain-utils/helpers.h"

// Map ADC value (0-4095) to MIDI range (0-127)
uint16_t adc_value = 2048;
int midi_value = map(adc_value, 0, 4095, 0, 127);  // Returns 63

// Map pot value to voltage
int pot_raw = 512;
int voltage_mv = map(pot_raw, 0, 1023, 0, 5000);  // 0-5V in millivolts
```

#### clamp()
Constrains a value to a specified range.

```cpp
static inline int clamp(int min, int max, int value)
```

**Parameters:**
- `min`: Minimum allowed value
- `max`: Maximum allowed value
- `value`: Value to constrain

**Returns:** Value clamped to [min, max] range

**Example:**
```cpp
#include "brain-utils/helpers.h"

// Ensure value stays in valid range
int user_input = 150;
int safe_value = clamp(0, 127, user_input);  // Returns 127

// Clamp voltage to safe range
int voltage = -2;
int safe_voltage = clamp(0, 10, voltage);  // Returns 0

// Works with in-range values too
int ok_value = clamp(0, 100, 50);  // Returns 50
```

### Use Cases

#### ADC Value Scaling
```cpp
// Read pot and scale to 0-127 range
uint16_t pot_raw = adc_read();  // 0-4095
int midi_value = map(pot_raw, 0, 4095, 0, 127);
```

#### Parameter Validation
```cpp
void set_brightness(int brightness) {
    // Ensure brightness is in valid range
    int safe_brightness = clamp(0, 255, brightness);
    pwm_set_level(led_pin, safe_brightness);
}
```

#### Voltage Conversion
```cpp
// Convert CV input to MIDI note
float cv_voltage = 2.5f;  // 2.5V input
int midi_note = map(cv_voltage * 100, 0, 1000, 24, 120);  // C1 to C9
midi_note = clamp(24, 127, midi_note);  // Ensure valid MIDI range
```

#### Combining map() and clamp()
```cpp
// Read sensor and map to parameter, ensuring valid range
int sensor = read_sensor();
int param = map(sensor, 0, 1023, -10, 110);  // Map to extended range
param = clamp(0, 100, param);  // Constrain to valid range
```

### Important Notes
- Both functions are `inline` for performance
- `map()` uses integer math - be aware of rounding
- `map()` does NOT clamp - use `clamp()` separately if needed
- For floating-point ranges, cast appropriately
- These functions are header-only (no .cpp file needed)

---

## Including Utilities

```cpp
// Include specific utilities
#include "brain-utils/ringbuffer.h"
#include "brain-utils/helpers.h"

// Or if you need both
#include "brain-utils/ringbuffer.h"
#include "brain-utils/helpers.h"
```

All utilities are in the `brain::utils` namespace (except helper functions which are global/inline).
