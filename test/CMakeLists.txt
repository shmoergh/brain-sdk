# Test firmware
# This executable contains manual hardware test apps.

add_executable(test
    apps/audio_processor_test.cpp
    apps/basic_pot_reads_test.cpp
    apps/inputs_and_pots_test.cpp
    main.cpp
    apps/leds_test.cpp
    apps/midi_to_cv_test.cpp
    apps/multipot_test.cpp
    apps/output_engine_timing_test.cpp
    apps/output_ownership_test.cpp
    apps/storage_test.cpp
    apps/storage_persistence_check_test.cpp
)

# Link against the simplified Brain SDK target
target_link_libraries(test
    pico_stdlib
    brain
)

target_compile_definitions(test PRIVATE
    BRAIN_USE_ALL=1
)

# Enable USB output for printf
pico_enable_stdio_usb(test 1)
pico_enable_stdio_uart(test 1)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(test)
