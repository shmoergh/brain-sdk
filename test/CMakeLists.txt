# Test firmware
# This executable contains manual hardware test apps.

add_executable(test
    apps/adc_engine_test.cpp
    apps/audio_processor_test.cpp
    apps/audio_volume_test.cpp
    apps/audio_dual_stream_test.cpp
    main.cpp
    apps/leds_test.cpp
    apps/midi_to_cv_test.cpp
    apps/multipot_test.cpp
    apps/pot_live_monitor_test.cpp
    apps/pot_read_stability_test.cpp
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
