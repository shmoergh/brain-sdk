add_library(brain-common
    ringbuffer.cpp
)

target_include_directories(brain-common PUBLIC
    include
    ..
    ${PICO_SDK_PATH}/src/common/include
)

target_link_libraries(brain-common
    pico_stdlib)