#pragma once

#include "brain-storage/storage-common.h"

namespace brain::storage {

#ifndef BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT
#define BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT 0
#endif

constexpr bool kAllowUnprotectedLayout =
	BRAIN_STORAGE_ALLOW_UNPROTECTED_LAYOUT != 0;

// Returns true when the firmware binary ends before the reserved app-data
// sector starts, meaning storage regions are linker-protected.
bool is_layout_protected();

}  // namespace brain::storage
