# Test firmware
# This executable contains manual hardware test apps.

add_executable(test
    main.cpp
    apps/leds_test.cpp
    apps/midi_to_cv_test.cpp
    apps/multipot_test.cpp
)

# Link against all Brain SDK libraries
target_link_libraries(test
    pico_stdlib
    brain-common
    brain-io
    brain-ui
    brain-utils
)

# Enable USB output for printf
pico_enable_stdio_usb(test 1)
pico_enable_stdio_uart(test 1)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(test)
