# Brain SDK Setup

## Repository Structure
```text
brain-sdk/
├── brain/         # SDK module
│   ├── brain.h    # Brain wrapper
│   ├── include/   # Public component headers
│   └── src/       # Component implementations
├── docs/
├── pico-sdk/
├── scripts/
├── test/
└── sandbox/
```

## Prerequisites
- CMake
- `arm-none-eabi-gcc`
- OpenOCD (optional debug)
- Pico SDK submodule

## Install Pico SDK Submodule
```sh
git submodule add https://github.com/raspberrypi/pico-sdk.git pico-sdk
git submodule update --init --recursive
```

## Build
```sh
cmake -S . -B build
cmake --build build
```

## New Firmware App
```sh
./scripts/new-brain-app.sh <program-name>
```

## Storage Reservation in External Firmware Repos
Before `pico_sdk_init()` in your firmware `CMakeLists.txt`:
```cmake
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()
```
