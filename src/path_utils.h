// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <string>

namespace ArxHeadTracking {

// The path of @p filename in the folder this module was loaded from. Reads the module
// path with the WIDE API rather than converting an ANSI one: GetModuleFileNameA renders
// anything the active ANSI codepage cannot represent as '?', which turns a non-ASCII
// install path into a directory that does not exist.
//
// Returns an empty string for a module path GetModuleFileName truncated into its buffer,
// and for a directory that fits MAX_PATH but no longer does once the filename is on the
// end of it.
std::wstring GetModulePathW(const char* filename);

}  // namespace ArxHeadTracking
