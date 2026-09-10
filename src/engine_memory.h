// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

// Reads and writes of the game's own globals, whose addresses come out of the
// build profile.
//
// Every access is volatile. These are written by the game from its own thread
// between our detours, so a compiler that hoisted a read out of a per-frame gate
// would test a value from an earlier frame.
namespace ArxHeadTracking {

inline int32_t ReadLong(uintptr_t addr) {
    return *reinterpret_cast<const volatile int32_t*>(addr);
}

inline int16_t ReadShort(uintptr_t addr) {
    return *reinterpret_cast<const volatile int16_t*>(addr);
}

inline float ReadFloat(uintptr_t addr) {
    return *reinterpret_cast<const volatile float*>(addr);
}

inline void* ReadPointer(uintptr_t addr) {
    return *reinterpret_cast<void* const volatile*>(addr);
}

inline void WriteLong(uintptr_t addr, int32_t value) {
    *reinterpret_cast<volatile int32_t*>(addr) = value;
}

}  // namespace ArxHeadTracking
