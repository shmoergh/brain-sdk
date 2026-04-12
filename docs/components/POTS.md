# Pots Component

`Pots` class reads the Brain potentiometers. The Brain hardware uses a multiplexer to read its three pots through a single ADC channel (the other two channels are used by the two audio/CV inputs). The multiplexer needs some time between two reads, otherwise pot readings will "bleed/crosstalk", ie. the reading of one pot will have an effect on another. To avoid crosstalk and jitter we use buffered pot reads which means we read a stored pot value that was captured earlier.


## Quick Start
```cpp
#define BRAIN_USE_POTS 1
#include "brain/brain.h"

Brain brain;

int main() {
	BrainInitStatus status = brain.init_pots(create_default_pots_config(3, 8));
	if (!brain_init_succeeded(status)) {
		return 1;
	}

	while (true) {
		brain.update_pots();
		uint16_t p0 = brain.pots.get(0); // default: buffered/safe read

		// Do stuff with p0
		(void)p0;
	}
}

```


## **Config helpers**

- `PotsConfig create_default_config(uint8_t num_pots = 3, uint8_t output_resolution = kDefaultPotsOutputResolution);`

	Returns a ready-to-use default config from pots-core.h.

- `PotsConfig create_default_pots_config(uint8_t num_pots = 3, uint8_t output_resolution = kDefaultPotsOutputResolution);`

	Inline wrapper in pots.h that calls create_default_config(...) (same behavior, nicer name for users).


## **Pots class API**

- `Pots();`

	Constructor. Initializes internal buffers/state.

- `void init(const PotsConfig& cfg);`

	Initializes ADC + mux pins using cfg, primes internal values, and makes buffered reads valid.

- `void reconfigure(const PotsConfig& cfg);`

	Re-applies configuration by reinitializing with new settings.


### **Sampling behavior controls**

- `void set_simple(bool simple);`

	Enables/disables simple read mode. Also invalidates buffered cache so next buffered read is refreshed.

- `void set_optimized_sampling_enabled(bool enabled);`

	Turns optimized sampling path on/off. Also invalidates buffered cache.

- `bool is_optimized_sampling_enabled() const;`

	Returns current optimized-sampling flag.

- `void set_output_resolution(uint8_t resolution);`

	Changes output resolution scaling (e.g. 8-bit, etc.). Invalidates buffer.

- `void set_settling_delay_us(uint32_t delay);`

	Sets mux/ADC settling delay. Invalidates buffer.

- `void set_samples_per_read(uint8_t samples);`

	Sets averaging sample count for non-simple path. Invalidates buffer.

- `void set_change_threshold(uint16_t threshold);`

	Sets threshold for change callback triggering.


### **Read/update functions**

- `void scan();`

	Reads all configured pots, updates buffered values, and fires on_change callback when threshold is exceeded.

- `uint16_t get_single(uint8_t index);`

	Immediate direct read of one pot (maps ADC raw to configured output resolution).

- `uint16_t get(uint8_t index);`

	Buffered-safe read. If buffer is invalid, it performs scan() first, then returns buffered value.

- `uint16_t get_buffered(uint8_t index) const;`

	Returns currently buffered value only (no ADC read).

- `uint16_t get_raw(uint8_t index);`

	Immediate raw ADC read (12-bit style raw value, before output-resolution scaling).

- `uint8_t get_output_resolution() const;`

	Returns current configured output resolution.

- `uint16_t get_output_max() const;`

	Returns max value for current output resolution (e.g. 255 for 8-bit).


### **Callback / metadata**

- `void set_on_change(std::function<void(uint8_t, uint16_t)> cb);`

	Registers callback called from scan() when value change exceeds threshold.

- `uint8_t get_num_pots() const;`

	Returns number of active pots in current config.

---

## PotMultiFunction
Use `PotMultiFunction` when one physical pot controls multiple logical parameters. Without it, one pot always maps to one value. With PotMultiFunction, the same pot can become, for example:

- cutoff in one mode
- resonance in another
- tempo in another

It also handles the “feel” of switching between mappings via modes:

- `kPotMultiFunctionModeDirect`: value follows knob immediately
- `kPotMultiFunctionModePickup`: value changes only after knob “catches” the stored value
- `kPotMultiFunctionModeValueScale`: value changes relatively around an anchor. This feels pretty smooth!

## Example

```cpp
#define BRAIN_USE_POTS 1
#define BRAIN_USE_POT_MULTI_FUNCTION 1
#include "brain/brain.h"

#include <pico/stdlib.h>

Brain brain;

enum FunctionId : uint8_t {
	kCutoff = 1,
	kResonance = 2
};

int main() {
	stdio_init_all();

	// 1) Init pots through Brain
	BrainInitStatus status = brain.init_pots(create_default_pots_config(3, 8));
	if (!brain_init_succeeded(status)) return 1;

	// 2) Init pot multi-function through Brain
	status = brain.init_pot_multi();
	if (!brain_init_succeeded(status)) return 1;

	// 3) Register two logical functions on the SAME physical pot (pot 0)
	PotFunctionConfig cutoff{};
	cutoff.function_id = kCutoff;
	cutoff.pot_index = 0;
	cutoff.min_value = 20;
	cutoff.max_value = 20000;
	cutoff.initial_value = 800;
	cutoff.mode = kPotMultiFunctionModeValueScale; // smooth mode switching
	cutoff.pickup_hysteresis = 1;
	brain.pot_multi.register_function(cutoff);

	PotFunctionConfig resonance{};
	resonance.function_id = kResonance;
	resonance.pot_index = 0;
	resonance.min_value = 0;
	resonance.max_value = 100;
	resonance.initial_value = 30;
	resonance.mode = kPotMultiFunctionModeValueScale;
	resonance.pickup_hysteresis = 1;
	brain.pot_multi.register_function(resonance);

	bool edit_cutoff = true;

	while (true) {
		// 4) Decide which logical function pot 0 is controlling right now
		brain.pot_multi.set_active_function(0, edit_cutoff ? kCutoff : kResonance);

		// 5) Update from buffered pot scan
		brain.update_pot_multi(true);

		// 6) Read both logical values (active one may change this tick)
		int32_t cutoff_value = brain.pot_multi.get_value(kCutoff);
		int32_t resonance_value = brain.pot_multi.get_value(kResonance);

		// Use values in your DSP/control code
		(void)cutoff_value;
		(void)resonance_value;

		// Demo: switch target every second
		static uint32_t last_switch = 0;
		uint32_t now = to_ms_since_boot(get_absolute_time());
		if (now - last_switch > 1000) {
			last_switch = now;
			edit_cutoff = !edit_cutoff;
		}

		sleep_ms(1);
	}
}
```

## **Constants and config**

### **Constants**

- `kPotMultiFunctionModeDirect`
	mapping from pot position to value range.
- `kPotMultiFunctionModePickup`
	does not move until knob position “meets” current stored value (with hysteresis tolerance).
- `kPotMultiFunctionModeValueScale`
	Relative/scaled movement mode to avoid jumps after context/mode switches.

### **PotFunctionConfig fields**

- `function_id`
	Unique logical ID for the parameter.
- `pot_index`
	Physical pot lane this function is attached to.
- `min_value, max_value`
	Logical output range.
- `initial_value`
	Start value for that function (clamped into range).
- `mode`
	Behavior mode (`kPotMultiFunctionModeDirect`, `kPotMultiFunctionModePickup`, `kPotMultiFunctionModeValueScale`).
- `pickup_hysteresis`
	Tolerance window for pickup crossing/catch.

## **Class API and lifecycle**

- `PotMultiFunction()`

	Creates the mapper with empty registration and no active mappings.

- `init(uint8_t max_functions = kMaxFunctions)`

	Resets all internal state and sets usable registration capacity (capped at 16).

- `register_function(const PotFunctionConfig& config)`

	Registers one logical function. Fails for invalid config, duplicate function_id, or capacity full.

- `reset_for_mode_change(bool clear_active_mappings = true)`

	Resets transition/runtime tracking used by pickup/value-scale behavior. Optionally clears active mappings.


## **Active function mapping API**

- `set_active_function(uint8_t pot_index, uint8_t function_id)`

	Maps one pot lane to one function ID.

- `set_active_functions(const uint8_t* per_pot_function_ids, uint8_t count)`

	Bulk mapping for multiple pot lanes in one call.


## **Update / sampling path API**

- `update(Pots& pots)`

	Default convenience path. Uses buffered semantics with scan.

- `update_buffered(Pots& pots, bool perform_scan = true)`

	Uses buffered pot values. Can scan first (perform_scan=true) or use existing buffer (false).

- `update_single(Pots& pots)`

	Uses direct per-call pot reads (get_single) instead of buffered data.


## **Value / change-query API**

- `get_value(uint8_t function_id) const`

	Returns current logical value for function ID (returns 0 if not found).

- `get_changed(uint8_t function_id) const`

	Returns whether that function changed since last clear (or false if not found).

- `clear_changed_flags()`

	Clears changed flags for all registered functions.


## **Class constants**

- `kMaxFunctions = 16`
	Maximum registered logical functions.
- `kMaxPots = 4`
	Maximum pot lanes supported by mapper (this is just because the multiplexer can theoretically scan 4 pots, even though there's physically 3 pots in the Brain hardware)

## **Practical notes**

- Registered functions do nothing until they are active on their lane.
- Function IDs must be unique.
- Values are always clamped to configured min/max.
- Pickup/value-scale modes rely on activation history, so call reset_for_mode_change(...) when changing higher-level app modes.
