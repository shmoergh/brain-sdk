# SDK Test Program
# This executable verifies that all Brain SDK libraries compile and link correctly

add_executable(sdk_test
    main.cpp
)

# Link against all Brain SDK libraries
target_link_libraries(sdk_test
    pico_stdlib
    brain-common
    brain-io
    brain-ui
)

# Enable USB output for printf
pico_enable_stdio_usb(sdk_test 1)
pico_enable_stdio_uart(sdk_test 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(sdk_test)
