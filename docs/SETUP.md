# Brain SDK – Development Setup Guide

## Basic folder structure
```
brain-sdk/
├── build/         # Default CMake build output
├── build-rp2040/  # Optional RP2040 build output
├── build-rp2350/  # Optional RP2350 build output
├── docs/          # Documentation and conventions
├── lib/           # Reusable libraries (e.g. brain-io, brain-ui)
├── pico-sdk/      # Pico SDK (as a git submodule)
├── scripts/       # Helper scripts (e.g. new-brain-app.sh)
├── test/          # Manual hardware test apps
└── sandbox/       # Thin sandbox firmware wrapper
```

## Prerequisites
- XCode (for build tools on macOS)
- VSCode (recommended editor)
- Homebrew (for package management)
- CMake (build system)
- GNU ARM Embedded Toolchain (`arm-none-eabi-gcc`)
- OpenOCD (for debugging with Picoprobe)
- Minicom (optional, for serial console)

## Installing the Pico SDK as a submodule
Run these commands in your project root:
```sh
git submodule add https://github.com/raspberrypi/pico-sdk.git pico-sdk
git submodule update --init --recursive
```
If the folder already exists, remove it from git tracking first:
```sh
git rm -r --cached pico-sdk
rm -rf pico-sdk
```
Then add as a submodule as above.

## Recommended VSCode and clang-format settings for C/C++

Add the following to your VSCode `settings.json` for consistent tab-based formatting in C and C++:

```jsonc
// All for C
"C_Cpp.clang_format_style": "file",
"C_Cpp.clang_format_fallbackStyle": "none",
"[c]": {
	"editor.insertSpaces": false,
	"editor.tabSize": 4,
	"editor.detectIndentation": false,
	"editor.formatOnSave": true,
	"editor.defaultFormatter": "xaver.clang-format"
},
"[cpp]": {
	"editor.insertSpaces": false,
	"editor.tabSize": 4,
	"editor.detectIndentation": false,
	"editor.formatOnSave": true,
	"editor.defaultFormatter": "xaver.clang-format"
},
```

This ensures that both VSCode and clang-format use tabs (width 4) for indentation in all C/C++ files.

## Creating a new program
Use the helper script:
```sh
./scripts/new-brain-app.sh <program-name>
```
This creates a new app folder (by default one level above `brain-sdk`) with boilerplate files and `brain-sdk` as a submodule.
Generated apps already include the required flash-reservation CMake snippet for SDK-managed calibration/app storage.
If the checked-out SDK revision does not provide the reservation helper yet, configure prints a warning and build continues without protected storage.

After running, re-run CMake configure/build:
```sh
rm -rf build
cmake -B build -G "Unix Makefiles"
cmake --build build
```

## Migration note for existing firmware repos
If your firmware repo was created before storage support was added, update its `CMakeLists.txt` so calibration survives UF2 updates:

```cmake
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()
```

Place those lines after `project(...)` and before `pico_sdk_init()`.
Without this reservation, app code can overlap the storage sectors and calibration persistence is not guaranteed.
If your repo uses `brain-sdk` as submodule, make sure the submodule commit is updated to a revision that contains `cmake/brain-storage-reserve-flash.cmake`.
On RP2350, the SDK reserves an extra top guard sector (12KB total) to avoid UF2 absolute-block side effects on calibration data.


## C++ Linting & Formatting
To enforce code style and formatting (Google C++ Style, 4-space indent, etc.), use clang-format:

### Install clang-format
```sh
brew install clang-format
```

### Usage
- Format a file in-place:
	```sh
	clang-format -style=file -i path/to/file.cpp
	```
- Or use the Clang-Format VSCode extension for auto-formatting on save.

The `.clang-format` file in the project root configures formatting rules.

## Debugging a program
Use the predefined VSCode launch profiles in `.vscode/launch.json`:
- `Test - RP2040`
- `Test - RP2350`
- `Sandbox - RP2040`
- `Sandbox - RP2350`

Each profile has a matching pre-launch task in `.vscode/tasks.json` that configures and builds the correct board/target combo in either `build-rp2040/` or `build-rp2350/`.

Select the desired debug configuration in VSCode and hit F5 to start debugging with Picoprobe.
