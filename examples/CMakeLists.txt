# Example firmware package
# Simple runnable apps for Brain SDK modules/components.

add_executable(examples
    main.cpp
    apps/leds_example.cpp
    apps/inputs_example.cpp
    apps/io_passthrough_example.cpp
    apps/pots_multi_example.cpp
    apps/midi_parser_example.cpp
    apps/midi_to_cv_example.cpp
    apps/storage_example.cpp
    apps/audio_processor_example.cpp
    apps/sample_and_hold.cpp
)

target_link_libraries(examples
    pico_stdlib
    brain
)

target_compile_definitions(examples PRIVATE
    BRAIN_USE_ALL=1
)

pico_enable_stdio_usb(examples 1)
pico_enable_stdio_uart(examples 1)

pico_add_extra_outputs(examples)
