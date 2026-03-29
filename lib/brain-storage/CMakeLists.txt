add_library(brain-storage
	storage.cpp
)

target_include_directories(brain-storage PUBLIC
	include
	..
	${PICO_SDK_PATH}/src/common/include
	${PICO_SDK_PATH}/src/rp2_common/hardware_flash/include
	${PICO_SDK_PATH}/src/rp2_common/pico_flash/include
)

target_link_libraries(brain-storage
	pico_stdlib
	hardware_flash
	pico_flash
)

target_compile_definitions(brain-storage PUBLIC
	BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT=$<BOOL:${BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT}>
)

target_include_directories(brain-storage PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
