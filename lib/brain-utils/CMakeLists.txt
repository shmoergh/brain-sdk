add_library(brain-utils
    ringbuffer.cpp
    midi-to-cv.cpp
)
target_include_directories(brain-utils PUBLIC
    include
    ..
    ${PICO_SDK_PATH}/src/common/include
    ${PICO_SDK_PATH}/src/rp2_common/hardware_gpio/include
    ${PICO_SDK_PATH}/src/rp2_common/pico_stdlib/include
)
target_link_libraries(brain-utils
    brain-ui
    brain-io
    pico_stdlib
    hardware_adc
    hardware_pwm
)
target_include_directories(brain-utils PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
