# Getting started

Once the brain target is linked (ie. you added the Brain SDK to your project), there are two normal usage styles.

## 1. Use the `Brain` wrapper (most common)

The Brain wrapper is the SDK’s top-level “manager” class. Instead of you manually wiring Leds, Inputs, Outputs, Pots, MidiParser, Storage, and utilities one by one, Brain gives you one object that:

- owns those components
- initializes them through consistent init_*() calls
- updates them through update_*() / update_all()
- automatically wires shared dependencies between components (for example, connecting `AudioProcessor` to `Pots` when both are initialized so pot snapshots flow into the audio callback)

In practice, this is the easiest and safest way to build firmware because startup order, shared-resource rules, and feature selection are centralized in one place.

### Example

In the following example we're initializing all modules (leds, inputs, outputs etc.) in the Brain SDK, set the output range to 0 to 10V (simulated DC coupling), read the input voltage on Input 1 (Channel A) and set the same voltage on Output 1 (Channel A). If the output voltage is out of range then we light up the first LED on the LED strip. Yeah, not the most realistic example but it shows you how things work.

``` c++
// 1. Enable all Brain SDK modules at compile time. Alternatively you can use
//    modules individually, e.g. #define BRAIN_USE_LEDS 1 etc.
#define BRAIN_USE_ALL 1

// 2. Include the wrapper API (Brain class and related types).
#include "brain/brain.h"

#include <pico/stdlib.h>

// 3. Create one global Brain instance that will own and expose all enabled modules.
Brain brain;

int main() {
    stdio_init_all();

    // 4. Initialize all core modules enabled by BRAIN_USE_ALL
    if (!brain_init_succeeded(brain.init_all())) return 1;

    // 5. Further setup modules
    brain.outputs.set_output_range(
        kOutputsChannelA,
        kOutputsRange0To10V
    );

    while (true) {
        // 6. Update all enabled modules in one call
        brain.update_all();

        // 7. Read input A in mV and pass it to output A
        int32_t in_mv = brain.inputs.get_voltage_millivolts_channel_a();
        if (!brain.outputs.set_voltage_millivolts(kOutputsChannelA, in_mv)) {
            brain.leds.on(0);   // indicate out-of-range write
        } else {
            brain.leds.off(0);
        }

        sleep_ms(1);
    }
}
```

`update_all()` calls update functions for relevant modules (for example buttons, LEDs, inputs/pots, utilities if enabled), so periodic logic keeps running.

## 2. Use modules directly

If you don't want the `Brain` wrapper class, you can include component headers directly.

### Example

This is the same example as above just using the Brain SDK modules directly without the wrapper class. As we said above, why you would want to use the brain wrapper is because then you don't need to worry about ownership (e.g. the Brain wrapper initilizes all modules only once for memory efficiency).

``` c++
#include <pico/stdlib.h>

// 1. Include Brain components modules
#include "brain/include/leds.h"
#include "brain/include/inputs.h"
#include "brain/include/outputs.h"

int main() {
    stdio_init_all();

    // 2. Create direct module instances (no Brain wrapper)
    Leds leds(kLedsModeSimple);
    Inputs inputs;
    Outputs outputs;

    // 3. Initialize each module manually
    leds.init();
    if (!inputs.init()) return 1;
    if (!outputs.init()) return 1;

    // 4. Further module setup
    outputs.set_output_range(kOutputsChannelA, kOutputsRange0To10V);

    while (true) {

        // 5. Keep modules updated explicitly
        leds.update();
        inputs.update();

        // 6. Read input A in mV and pass it to output A (just a simple example)
        int32_t in_mv = inputs.get_voltage_millivolts_channel_a();
        if (!outputs.set_voltage_millivolts(kOutputsChannelA, in_mv)) {
            leds.on(0);   // indicate out-of-range write
        } else {
            leds.off(0);
        }

        sleep_ms(1);
    }
}
```

