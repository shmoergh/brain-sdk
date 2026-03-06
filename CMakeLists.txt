cmake_minimum_required(VERSION 3.22)

# ============================================================================
# BOARD CONFIGURATION - Comment/uncomment to switch between boards
# ============================================================================

# Raspberry Pi Pico (RP2040 - ARM Cortex-M0+) - ACTIVE
# set(PICO_BOARD pico)
# set(PICO_PLATFORM rp2040)

# Raspberry Pi Pico 2 (RP2350 - ARM Cortex-M33)
set(PICO_BOARD pico2)
set(PICO_PLATFORM rp2350-arm-s)

# Alternative: Raspberry Pi Pico 2 with RISC-V cores
# set(PICO_BOARD pico2)
# set(PICO_PLATFORM rp2350-riscv)

# ============================================================================

# Include and initialize Pico SDK
# This works whether brain-sdk is the top-level project or a submodule
include(pico_sdk_import.cmake)

project(brain-sdk VERSION 1.0.0)

# Initialize the Pico SDK
pico_sdk_init()

# Add subdirectories for libraries
add_subdirectory(lib)

# Only build sandbox firmware when this is the top-level project
# (not when included as a submodule in another project)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(sandbox)
endif()
