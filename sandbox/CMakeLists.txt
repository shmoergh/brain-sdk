# Sandbox firmware wrapper
# Keeps a minimal entrypoint in sandbox/, while test apps live in ../test/apps.

add_executable(sandbox
    main.cpp
    apps/blink.cpp
)

# Link against all Brain SDK libraries
target_link_libraries(sandbox
    pico_stdlib
    brain-common
    brain-io
    brain-ui
    brain-utils
)

# Enable USB/UART output for printf
pico_enable_stdio_usb(sandbox 1)
pico_enable_stdio_uart(sandbox 1)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(sandbox)
