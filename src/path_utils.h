// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <string>

namespace ArxHeadTracking {

std::string GetModulePath(const char* filename);

// Wide variant for APIs that take wide paths (core logging::Open). Reads the module
// path with the WIDE API rather than converting the ANSI one: GetModuleFileNameA
// renders anything the active ANSI codepage cannot represent as '?', which turns a
// non-ASCII install path into a directory that does not exist.
//
// Both variants return an empty string rather than a path the ANSI file APIs cannot open:
// a module path GetModuleFileName truncated into its buffer, and a directory that fits
// MAX_PATH but no longer does once the filename is on the end of it.
std::wstring GetModulePathW(const char* filename);

}  // namespace ArxHeadTracking
