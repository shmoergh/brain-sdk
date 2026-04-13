# Installation

## Option 1: as a git submodule (recommended)

You can add the Brain SDK to your project as a git submodule if you want all future SDK updates and tracable versions. To do so you need to have a git repo in your firmware (`git init`) and run the following commands in the firmware folder:

```
git submodule add git@github.com:shmoergh/brain-sdk.git brain-sdk
git submodule update --init --recursive
```

Once you added the Brain SDK as a submodule you need to make some updates in the firmware's `CMakeLists.txt` file (replace `my_firmware` with your firmware's project name):

```
cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico2)
endif()
if(NOT DEFINED PICO_PLATFORM)
    set(PICO_PLATFORM rp2350-arm-s)
endif()

include(brain-sdk/pico_sdk_import.cmake)

project(my_firmware C CXX ASM)

# Brain SDK Storage helper to preserve CV calibration
include(brain-sdk/cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()

pico_sdk_init()

add_subdirectory(brain-sdk)

add_executable(my_firmware main.cpp)
target_link_libraries(my_firmware pico_stdlib brain)
pico_add_extra_outputs(my_firmware)
```

## Option 2: copy only the `brain` folder

Use this option when you want a minimal setup and you are okay maintaining SDK versions manually.

1. Copy the `brain/` folder into your firmware repo (keep folder name as `brain`)
2. Copy `cmake/brain-storage-reserve-flash.cmake` — 

Example structure:

``` sh
my-firmware/
  CMakeLists.txt
  main.cpp
  brain/
    brain.h
    include/
    src/
  cmake/
    brain-storage-reserve-flash.cmake
```

In the firmware's `CMakeLists.txt`:

```
include(pico_sdk_import.cmake)

project(my_firmware C CXX ASM)

# Brain SDK Storage helper to preserve CV calibration
include(cmake/brain-storage-reserve-flash.cmake)
brain_storage_configure_flash_reservation()

pico_sdk_init()

add_subdirectory(brain)

add_executable(my_firmware main.cpp)
target_link_libraries(my_firmware pico_stdlib brain)
pico_add_extra_outputs(my_firmware)
```