add_library(tools
    midi-to-cv.cpp
)
target_include_directories(tools PUBLIC
    include
    ..
    ${PICO_SDK_PATH}/src/common/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_gpio/include
    ${PICO_SDK_PATH}/src/rp2_common/pico_stdlib/include
)
target_link_libraries(tools
    brain-ui
    brain-io
    pico_stdlib
    hardware_adc
    hardware_pwm
)
target_include_directories(tools PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
