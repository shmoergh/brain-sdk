# Inputs Component

`Inputs` is the Brain SDK class for reading the module's input side hardware. It combines two things in one API:

1. audio/CV input channels (via ADC)
2. pulse input (digital trigger/gate style input)

So instead of handling ADC setup, scaling, GPIO edge handling, and optional filtering yourself, you use `Inputs` to read raw or millivolt values of `In 1` / `In 2` and react to pulse rise/fall events happening on `Pulse In` via callbacks or polling.

Audio/CV channels are sampled continuously by the shared `AdcEngine` — `Inputs` simply subscribes to ADC inputs A and B at init and the engine pushes fresh values into the cache via DMA. Reads (`get_raw_*`, `get_voltage_millivolts_*`) never block. `update_audio_cv()` is kept for source compatibility but is no longer required to keep values fresh.

`Inputs` runs concurrently with `Pots` and `AudioProcessor` — there is no longer any restriction on which combination of these components can be active.

## Audio/CV inputs

The Brain input circuit scales the module signal before it reaches the RP2040/RP2350 ADC, so the SDK has to map ADC voltage back to signal voltage.

In the `Inputs` class, this is done in `get_voltage_millivolts_*` methods, in two steps:

1. Raw ADC (`0..4095`) is converted to ADC millivolts (`0..3300 mV`).
2. That ADC voltage is linearly remapped to signal voltage using Brain-specific calibration points:
	- `240 mV` at ADC corresponds to `-5000 mV` signal
	- `3000 mV` at ADC corresponds to `+5000 mV` signal

So it is not a simple full-range `0..3.3V -> -5..+5V` conversion.

Another important detail: this conversion does not clamp. If the ADC value falls outside the expected window, the computed signal voltage can go below `-5000 mV` or above `+5000 mV`. That is expected behavior in the current implementation, and downstream code should clamp if strict bounds are needed.

### Constants

- `kInputsChannelA`, `kInputsChannelB`
  Input channel selectors for APIs like `get_raw(int channel)` and `get_voltage_millivolts(int channel)`.

### Example

```cpp
#define BRAIN_USE_INPUTS 1
#include "brain/brain.h"

#include <pico/stdlib.h>
#include <stdio.h>

Brain brain;

int main() {
	stdio_init_all();

	// Init input subsystem via Brain wrapper
	BrainInitStatus status = brain.init_inputs();
	if (!brain_init_succeeded(status)) return 1;

	while (true) {
		// Refresh analog + pulse input state
		brain.update_inputs();

		// Sample audio/CV input A
		uint16_t raw_a = brain.inputs.get_raw_channel_a();
		int32_t mv_a = brain.inputs.get_voltage_millivolts_channel_a();

		// (Optional) also read channel B
		uint16_t raw_b = brain.inputs.get_raw_channel_b();
		int32_t mv_b = brain.inputs.get_voltage_millivolts_channel_b();

		printf("A: raw=%u mv=%ld | B: raw=%u mv=%ld\n",
			   raw_a, (long)mv_a, raw_b, (long)mv_b);

		sleep_ms(10);
	}
}

```

### `Inputs` API and lifecycle

- `Inputs(uint pulse_in_gpio = GPIO_BRAIN_PULSE_INPUT)`
  Creates an input handler for analog A/B plus pulse input. You can override pulse GPIO.

- `~Inputs()`
  Destructor. Releases the audio/CV `AdcEngine` subscriptions.

- `bool init_audio_cv()`
  Subscribes to audio/CV ADC channels A and B via `AdcEngine` and prepares voltage conversion parameters.

- `bool init_pulse()`
  Initializes pulse input GPIO path and related state.

- `bool init()`
  Initializes both analog and pulse paths.

- `void pulse_end()`
  De-initializes pulse path and interrupts for this instance.

### Update/runtime processing

- `void update_audio_cv()`
  Legacy no-op. Audio/CV values are kept fresh continuously by `AdcEngine`. Safe to keep calling for source compatibility.

- `void pulse_poll()`
  Polls pulse input state, applies glitch filter, and triggers callbacks on edges.

- `void update()`
  Convenience wrapper that calls `pulse_poll()` (and the legacy `update_audio_cv()` no-op).

### Analog read

- `uint16_t get_raw(int channel) const`
  Returns latest raw ADC sample for channel index (`kInputsChannelA` / `kInputsChannelB`).

- `uint16_t get_raw_channel_a() const`
  Returns latest raw ADC value for channel A.

- `uint16_t get_raw_channel_b() const`
  Returns latest raw ADC value for channel B.

- `int32_t get_voltage_millivolts(int channel) const`
  Returns latest converted signal voltage (mV) for selected channel.

- `int32_t get_voltage_millivolts_channel_a() const`
  Returns channel A converted signal voltage in mV.

- `int32_t get_voltage_millivolts_channel_b() const`
  Returns channel B converted signal voltage in mV.

### Legacy DMA-policy API (no-op shims)

These setters/getters are kept so apps written against earlier SDK versions keep compiling. Under the unified `AdcEngine`, audio/CV sampling is always DMA-driven.

- `void set_audio_cv_dma_enabled(bool)`
  No-op.

- `bool is_audio_cv_dma_enabled() const`
  Always returns `true`.

- `bool is_audio_cv_dma_active() const`
  Returns `true` when `init_audio_cv()` has subscribed to `AdcEngine`.


---

## Pulse input

Pulse input is the Brain module’s digital trigger/gate input path. Use it when the signal is event-like (on/off edges), not continuous analog voltage. Typical uses are:

- clock in / tempo sync
- trigger detection (envelopes, sequencer steps)
- gate state input (high/low control)

In SDK terms, `Inputs` gives you pulse as:

- polling (`pulse_read()` / `pulse_read_raw()`)
- edge callbacks (`pulse_on_rise`, `pulse_on_fall`)
- optional glitch filtering (`pulse_set_input_glitch_filter_us(...)`) to ignore short noise spikes

### Example

```cpp
#define BRAIN_USE_INPUTS 1
#include "brain/brain.h"

#include <pico/stdlib.h>
#include <stdio.h>

Brain brain;

int main() {
	stdio_init_all();

	// Init inputs (analog + pulse path)
	BrainInitStatus status = brain.init_inputs();
	if (!brain_init_succeeded(status)) return 1;

	// Optional: ignore very short spikes/noise on pulse input
	brain.inputs.pulse_set_input_glitch_filter_us(200);

	// Edge callbacks
	brain.inputs.pulse_on_rise([]() {
		printf("Pulse RISE\n");
	});

	brain.inputs.pulse_on_fall([]() {
		printf("Pulse FALL\n");
	});

	while (true) {
		// Updates analog sampling + pulse edge processing
		brain.update_inputs();

		// Optional polling view of current pulse state
		bool pulse_high = brain.inputs.pulse_read();
		(void)pulse_high;

		sleep_ms(1);
	}
}
```

### Pulse input API

- `bool pulse_read() const`
  Returns logical pulse state (active-low normalized for default pull-up wiring).

- `bool pulse_read_raw() const`
  Returns raw GPIO level without logical inversion.

### Pulse callback and filtering API

- `void pulse_on_rise(std::function<void()> cb)`
  Registers callback for pulse rising edge (logical edge).

- `void pulse_on_fall(std::function<void()> cb)`
  Registers callback for pulse falling edge (logical edge).

- `void pulse_set_input_glitch_filter_us(uint32_t us)`
  Sets software glitch-filter window in microseconds for pulse edge stabilization.

### Pulse interrupt control API

- `void pulse_enable_interrupts()`
  Enables GPIO edge interrupts for pulse input handling support.

- `void pulse_disable_interrupts()`
  Disables GPIO edge interrupts for pulse input.
