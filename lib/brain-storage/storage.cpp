#include "brain-storage/storage.h"

#include <cstdint>

namespace brain::storage {

extern "C" uint8_t __flash_binary_end;

bool is_layout_protected() {
	uintptr_t flash_binary_end_address =
		reinterpret_cast<uintptr_t>(&__flash_binary_end);
	if (flash_binary_end_address < XIP_BASE) {
		return false;
	}

	uint32_t flash_binary_end_offset =
		static_cast<uint32_t>(flash_binary_end_address - XIP_BASE);
	return flash_binary_end_offset <= layout::kAppDataRegionOffsetBytes;
}

}  // namespace brain::storage
