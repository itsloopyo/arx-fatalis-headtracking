// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "path_utils.h"

#include <windows.h>

#include <cstring>

namespace ArxHeadTracking {

namespace {

void DummyAddress() {}

// The module this DLL was loaded from, by an address inside it rather than by name -
// the ASI loader is free to rename us.
HMODULE SelfModule() {
    HMODULE hModule = nullptr;
    if (!GetModuleHandleExA(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCSTR>(&DummyAddress),
            &hModule) || hModule == nullptr) {
        return nullptr;
    }
    return hModule;
}

// GetModuleFileName does NOT report a path longer than the buffer as a failure: it
// fills the buffer, terminates it, and returns the buffer size, leaving a TRUNCATED
// path behind. Silently resolving the config and the log against a truncated directory
// writes them somewhere the game never reads. Treat it as no path at all.
bool Truncated(DWORD written, size_t capacity) {
    return written == 0 || written >= capacity;
}

// The same question asked of a path this file BUILDS rather than reads. Truncated covers
// what GetModuleFileName handed back; a directory that fits says nothing about the
// directory plus a filename.
bool FitsMaxPath(size_t length) {
    return length + 1 <= MAX_PATH;
}

template <typename Char>
std::basic_string<Char> DirectoryOf(const Char* path, size_t length) {
    const std::basic_string<Char> full(path, length);
    const Char separators[] = { static_cast<Char>(0x5C), static_cast<Char>(0x2F),
                                static_cast<Char>(0) };
    const size_t lastSlash = full.find_last_of(separators);
    if (lastSlash == std::basic_string<Char>::npos) {
        return {};
    }
    return full.substr(0, lastSlash + 1);
}

// The directory this DLL was loaded from, read WIDE. Every path this file produces is
// derived from this one: reading it narrow first and converting afterwards is what loses
// the characters (see GetModulePathW).
std::wstring ModuleDirectoryW() {
    HMODULE hModule = SelfModule();
    if (!hModule) {
        return {};
    }

    wchar_t modulePath[MAX_PATH];
    const DWORD written = GetModuleFileNameW(hModule, modulePath, MAX_PATH);
    if (Truncated(written, MAX_PATH)) {
        return {};
    }
    return DirectoryOf(modulePath, written);
}

}  // namespace

std::wstring GetModulePathW(const char* filename) {
    // Read the path WIDE rather than converting the ANSI one. GetModuleFileNameA
    // renders every character the active ANSI codepage cannot represent as '?', so an
    // install path with non-ASCII in it came back as a directory that does not exist
    // and the log was never created - on the machines whose logs are hardest to get.
    std::wstring dir = ModuleDirectoryW();
    if (dir.empty()) {
        return {};
    }

    if (!FitsMaxPath(dir.size() + std::strlen(filename))) {
        return {};
    }

    // The filename is a compile-time ASCII literal, so widening it is a byte-for-byte
    // copy with no codepage involved.
    for (const char* p = filename; *p; ++p) {
        dir.push_back(static_cast<wchar_t>(static_cast<unsigned char>(*p)));
    }
    return dir;
}

}  // namespace ArxHeadTracking
