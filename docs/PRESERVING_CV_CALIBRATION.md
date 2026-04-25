# Preserving Calibration Data

Brain stores per-board CV output calibration in a reserved sector at the top of the Pico's flash memory. This calibration is hardware-specific — it's measured once and written via the [Brain CV tuner firmware](https://github.com/shmoergh/brain-cv-tuner/) — and it has to survive every subsequent firmware flash for the life of the board. Losing it means re-running the entire calibration procedure on a multimeter rig.

Whether or not your firmware preserves calibration depends on **how your firmware is _built_**, not how it's used. If your build doesn't reserve the calibration sector at link time, the firmware image is free to grow into that region, and the next time someone drags your UF2 onto the Brain, calibration gets overwritten. This page walks through the CMake setup that keeps that from happening.

## What you need to do

Two things, both in your firmware project (not in the SDK):

1. **At build time:** call `brain_storage_configure_flash_reservation()` in your `CMakeLists.txt`, before `pico_sdk_init()`. This tells the linker to keep the firmware image out of the calibration sector.
2. **At runtime:** call `outputs.load_calibration_from_flash()` once during boot, after `brain.init_all()` succeeds. Without this call, calibration data sits unused in flash.

The first step is what protects calibration across flashes; the second is what makes your firmware actually use it. You need both.

## CMake setup

The reservation helper lives in the SDK at `brain-sdk/cmake/brain-storage-reserve-flash.cmake`. Include the helper and call its function at the right point in your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.22)

# Default board target. Override with -DPICO_BOARD=... on the command line.
if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2)
endif()
if(NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM rp2350-arm-s)
endif()

include(brain-sdk/pico_sdk_import.cmake)
project(my-firmware C CXX ASM)

# Reserve top-of-flash sectors for Brain SDK calibration storage.
# MUST come before pico_sdk_init() so the linker excludes those sectors
# from the firmware image. Without this, flashing this UF2 can overwrite
# CV calibration that lives in the same flash region.
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()

pico_sdk_init()

add_subdirectory(brain-sdk/brain)

add_executable(my-firmware main.cpp)
target_compile_definitions(my-firmware PRIVATE BRAIN_USE_ALL=1)
target_link_libraries(my-firmware PRIVATE brain pico_stdlib)
pico_add_extra_outputs(my-firmware)
```

The critical part is the **order**: `brain_storage_configure_flash_reservation()` must be called *after* `project()` (which sets up CMake's variables) and *before* `pico_sdk_init()` (which generates the linker script using the reservation as input). Calling it later has no effect — the linker script is already written by the time you get there.

If your project includes the SDK at a different path than `brain-sdk/`, adjust the two `include(...)` lines accordingly.

## Verifying the reservation worked

When you run `cmake`, you should see a log line confirming the reservation was applied:

```
-- [brain-storage] Reserved 12288 bytes at top-of-flash. Linker flash length set to ((4 * 1024 * 1024)) - (12288).
```

The number depends on the platform:

| Platform | Reserved bytes | Sectors |
|---|---|---|
| RP2040 (Pico 1) | 8192 | 2 × 4 KB (app blob + calibration) |
| RP2350 (Pico 2) | 12288 | 3 × 4 KB (app blob + calibration + guard) |

The RP2350 reserves an extra 4 KB guard sector at the very top of flash because the UF2 absolute-block format on RP2350 can write into that top sector during firmware updates. The guard prevents that absolute block from clobbering calibration.

If the line doesn't appear in your configure output, the helper isn't being called or is being called too late — re-check the order in your `CMakeLists.txt`.

For extra confidence, you can inspect the generated `.map` file after building to confirm the firmware image stays below the calibration region. The total flash size minus the reserved bytes is the maximum size your firmware image can be.

## Loading calibration at runtime

Once flash is reserved, your firmware also has to load the calibration on boot for the CV outputs to use it:

```cpp
#define BRAIN_USE_ALL 1
#include "brain/brain.h"

Brain brain;

int main() {
    brain.init_all();

    // Load CV calibration from the reserved sector. If the board hasn't been
    // calibrated yet (or the data is corrupt), this returns false — the CV
    // outputs will still work, just without calibrated voltage scaling.
    brain.outputs.load_calibration_from_flash();

    while (true) {
        brain.update();
        // ... your firmware logic
    }
}
```

After `load_calibration_from_flash()` succeeds, calls to `outputs.set_voltage_calibrated_millivolts(channel, mv)` apply the per-step calibration table. Calls to `outputs.set_voltage_millivolts()` bypass calibration and write the raw DAC value — useful for hardware-level tests like trimmer adjustment, but not what you want in normal use.

Application firmware should only ever **read** calibration. Calibration data is owned by the Brain CV tuner firmware, which is the only firmware that should call `write_cv_calibration` or `clear_cv_calibration`.

## What not to do

The SDK provides an escape hatch for development scenarios where you intentionally want to skip the reservation:

```bash
# DON'T DO THIS for production firmware
cmake -DBRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=ON ...
```

This option exists so that ad-hoc test builds can run without reserving flash, but **never enable it for any firmware that gets distributed to users**. With this flag on, the firmware image can quietly grow into the calibration sector and silently destroy a board's calibration on the next flash. There's no warning at runtime — the data is just gone.

Similarly, don't set `BRAIN_STORAGE_ENABLE_FLASH_RESERVATION=OFF` — that disables the reservation entirely.

For production firmware, leave both at their defaults (`BRAIN_STORAGE_ENABLE_FLASH_RESERVATION=ON`, `BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=OFF`).

## Summary

To preserve calibration in your firmware:

1. In `CMakeLists.txt`, call `brain_storage_configure_flash_reservation()` after `project()` but before `pico_sdk_init()`.
2. In your firmware, call `brain.outputs.load_calibration_from_flash()` after `brain.init_all()`.
3. Look for `[brain-storage] Reserved ... bytes at top-of-flash.` in the `cmake` configure output to confirm step 1 worked.
4. Never set `BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=ON` or `BRAIN_STORAGE_ENABLE_FLASH_RESERVATION=OFF` for distributed firmware.

---

A few notes on choices:

- The full CMakeLists.txt example is the same skeleton brain-diagnostics uses, so the diagnostics repo doubles as a working reference if anyone wants to see it in context.
- The "What not to do" section is deliberately scary because the failure mode is silent — calibration just disappears with no error.
- Cross-link target: if `STORAGE.md` already exists in the docs folder, the bottom of this page could link to it as "for the underlying flash layout details, see STORAGE.md" — your call whether to add that.