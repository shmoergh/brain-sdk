# Brain SDK flash reservation helper.
#
# Must be called before pico_sdk_init() so that the generated linker flash
# region excludes the reserved storage sectors.

if (NOT DEFINED BRAIN_STORAGE_RESERVED_FLASH_BYTES)
	set(_brain_storage_default_reserved_flash_bytes 8192)
	if (DEFINED PICO_PLATFORM AND PICO_PLATFORM MATCHES "^rp2350")
		# RP2350 UF2 conversion may include an absolute block in the top sector.
		# Keep a guard sector above calibration/app data.
		set(_brain_storage_default_reserved_flash_bytes 12288)
	endif()
	set(BRAIN_STORAGE_RESERVED_FLASH_BYTES
		"${_brain_storage_default_reserved_flash_bytes}"
		CACHE STRING "Bytes reserved at top-of-flash for Brain SDK storage")
endif()
option(BRAIN_STORAGE_ENABLE_FLASH_RESERVATION
	"Reserve top-of-flash sectors for Brain SDK calibration/app storage" ON)
option(BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT
	"Allow storage APIs to run without linker flash reservation checks" OFF)

function(brain_storage_configure_flash_reservation)
	if (BRAIN_STORAGE_FLASH_RESERVATION_APPLIED)
		return()
	endif()
	set(BRAIN_STORAGE_FLASH_RESERVATION_APPLIED TRUE PARENT_SCOPE)

	if (NOT BRAIN_STORAGE_ENABLE_FLASH_RESERVATION)
		message(STATUS "[brain-storage] Flash reservation disabled.")
		return()
	endif()

	set(_brain_storage_min_reserved_flash_bytes 8192)
	if (DEFINED PICO_PLATFORM AND PICO_PLATFORM MATCHES "^rp2350")
		set(_brain_storage_min_reserved_flash_bytes 12288)
	endif()

	if (BRAIN_STORAGE_RESERVED_FLASH_BYTES LESS _brain_storage_min_reserved_flash_bytes)
		message(WARNING
			"[brain-storage] BRAIN_STORAGE_RESERVED_FLASH_BYTES="
			"${BRAIN_STORAGE_RESERVED_FLASH_BYTES} is too small for PICO_PLATFORM='${PICO_PLATFORM}'. "
			"Using ${_brain_storage_min_reserved_flash_bytes} instead.")
		set(BRAIN_STORAGE_RESERVED_FLASH_BYTES
			"${_brain_storage_min_reserved_flash_bytes}"
			CACHE STRING "Bytes reserved at top-of-flash for Brain SDK storage" FORCE)
	endif()

	if (NOT DEFINED PICO_FLASH_SIZE_BYTES)
		if (DEFINED PICO_DEFAULT_FLASH_SIZE_BYTES)
			set(_brain_flash_size_expr "${PICO_DEFAULT_FLASH_SIZE_BYTES}")
		else()
			message(FATAL_ERROR
				"[brain-storage] PICO_DEFAULT_FLASH_SIZE_BYTES is undefined. "
				"Set PICO_FLASH_SIZE_BYTES before calling brain_storage_configure_flash_reservation().")
		endif()
	else()
		set(_brain_flash_size_expr "${PICO_FLASH_SIZE_BYTES}")
	endif()

	set(PICO_FLASH_SIZE_BYTES
		"(${_brain_flash_size_expr}) - (${BRAIN_STORAGE_RESERVED_FLASH_BYTES})"
		PARENT_SCOPE)

	message(STATUS
		"[brain-storage] Reserved ${BRAIN_STORAGE_RESERVED_FLASH_BYTES} bytes at top-of-flash. "
		"Linker flash length set to (${_brain_flash_size_expr}) - (${BRAIN_STORAGE_RESERVED_FLASH_BYTES}).")
endfunction()
