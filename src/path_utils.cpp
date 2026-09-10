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
// path behind. Silently resolving the INI and the log against a truncated directory
// writes them somewhere the game never reads. Treat it as no path at all.
bool Truncated(DWORD written, size_t capacity) {
    return written == 0 || written >= capacity;
}

// The same question asked of a path this file BUILDS rather than reads. Truncated covers
// what GetModuleFileName handed back; a directory that fits says nothing about the
// directory plus a filename, and neither GetPrivateProfileStringA nor fopen takes a long
// path without a manifest opt-in this module does not have.
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

// @p wide converted to the active ANSI codepage, or empty when it cannot be represented
// there without loss.
//
// The loss is not hypothetical and it is not obvious: WideCharToMultiByte substitutes a
// '?' for every character the codepage lacks and reports success, so the caller gets a
// plausible-looking path to a directory that does not exist.
std::string ToAnsiLossless(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    // A process whose ANSI codepage IS UTF-8 (an opt-in since Windows 10) can represent
    // everything, and WideCharToMultiByte is documented to reject both the flag and the
    // used-default pointer for that codepage. Ask without them there.
    const bool acpIsUtf8 = GetACP() == CP_UTF8;
    const DWORD flags = acpIsUtf8 ? 0u : WC_NO_BEST_FIT_CHARS;
    BOOL usedDefault = FALSE;
    BOOL* const usedDefaultOut = acpIsUtf8 ? nullptr : &usedDefault;

    const int needed = WideCharToMultiByte(CP_ACP, flags, wide.c_str(), -1, nullptr, 0,
                                           nullptr, usedDefaultOut);
    if (needed <= 0) {
        return {};
    }
    std::string narrow(static_cast<size_t>(needed - 1), '\0');
    if (WideCharToMultiByte(CP_ACP, flags, wide.c_str(), -1, &narrow[0], needed, nullptr,
                            usedDefaultOut) <= 0 || usedDefault) {
        return {};
    }
    return narrow;
}

std::string GetModuleDirectory() {
    const std::wstring wideDir = ModuleDirectoryW();
    if (wideDir.empty()) {
        return {};
    }

    // The common case: the install path is representable, so use it as it is.
    std::string ansi = ToAnsiLossless(wideDir);
    if (!ansi.empty()) {
        return ansi;
    }

    // It is not. The INI goes through cameraunlock-core's IniReader/IniWriter, which are
    // ANSI throughout (GetPrivateProfileStringA, fopen), so a wide path cannot be handed
    // to them - and a lossy narrow one resolves to a directory that does not exist, which
    // is how a game installed under a non-ASCII path got "the game directory is not
    // writable by this account" and no config at all.
    //
    // The 8.3 short name is the documented way out: it is ASCII by construction. It is
    // taken of the DIRECTORY, which exists, rather than of the INI, which may not yet.
    wchar_t shortDir[MAX_PATH];
    const DWORD written = GetShortPathNameW(wideDir.c_str(), shortDir, MAX_PATH);
    if (Truncated(written, MAX_PATH)) {
        return {};
    }
    // GetShortPathNameW is not documented to keep the trailing separator, and without one
    // the filename would be concatenated onto the last directory component instead of
    // placed inside it.
    std::wstring shortened(shortDir, written);
    if (shortened.back() != L'\\' && shortened.back() != L'/') {
        shortened.push_back(L'\\');
    }

    // Short-name generation can be disabled per volume, in which case this hands back the
    // long name unchanged and the conversion fails again. Failing closed is the point:
    // the caller reports it, rather than writing the config somewhere the player is not
    // looking.
    return ToAnsiLossless(shortened);
}

}  // namespace

std::string GetModulePath(const char* filename) {
    std::string dir = GetModuleDirectory();
    if (dir.empty()) {
        // Fail closed, like the wide variant below. Returning the bare filename resolved
        // it against the process working directory, so the INI was created and read
        // somewhere the player never looks: their edits next to dinput8.dll were ignored
        // while the log reported the config as loaded.
        return {};
    }
    if (!FitsMaxPath(dir.size() + std::strlen(filename))) {
        return {};
    }
    return dir + filename;
}

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
