cmake_minimum_required(VERSION 3.22)

# Include and initialize Pico SDK
# This works whether brain-sdk is the top-level project or a submodule
include(pico_sdk_import.cmake)

project(brain-sdk VERSION 1.0.0)

# Initialize the Pico SDK
pico_sdk_init()

# Add subdirectories for libraries
add_subdirectory(lib)

# Only build SDK test when this is the top-level project
# (not when included as a submodule in another project)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    add_subdirectory(sdk_test)
endif()
