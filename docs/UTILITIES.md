# Utilities

## Includes
```cpp
#include "brain/include/ringbuffer.h"
#include "brain/include/helpers.h"
```

## RingBuffer
You can use either:
- `RingBuffer` (global alias)
- `brain::utils::RingBuffer`

### Quick Example
```cpp
uint8_t storage[256];
RingBuffer rb;
rb.init(storage, 256);

rb.write_byte(0x42);
uint8_t value = 0;
if (rb.read_byte(value)) {
	printf("%u\n", value);
}
```

## helpers.h
- `long map(long x, long in_min, long in_max, long out_min, long out_max)`
- `int clamp(int min, int max, int value)`

Both are inline utility helpers commonly used in embedded mapping/validation paths.
