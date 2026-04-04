add_library(brain
    src/button.cpp
    src/inputs.cpp
    src/leds.cpp
    src/midi-parser.cpp
    src/midi-to-cv.cpp
    src/outputs.cpp
    src/pot-multi-function.cpp
    src/pots.cpp
    src/ringbuffer.cpp
    src/storage.cpp
)

target_include_directories(brain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PICO_SDK_PATH}/src/common/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_gpio/include
    ${PICO_SDK_PATH}/src/rp2_common/pico_stdlib/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_flash/include
    ${PICO_SDK_PATH}/src/rp2_common/pico_flash/include
)

target_link_libraries(brain
    pico_stdlib
    hardware_gpio
    hardware_timer
    hardware_spi
    hardware_adc
    hardware_pwm
    hardware_flash
    pico_flash
)

target_compile_definitions(brain PUBLIC
    BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=$<BOOL:${BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT}>
)
