// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "hook_install.h"

#include "logging.h"

#include <MinHook.h>

namespace ArxHeadTracking {

bool InstallDetour(void* target, void* detour, void** original, const char* what) {
    if (MH_CreateHook(target, detour, original) != MH_OK) {
        Log::Line("ERROR: could not create the %s hook at %p", what, target);
        return false;
    }
    if (MH_EnableHook(target) != MH_OK) {
        Log::Line("ERROR: could not enable the %s hook at %p", what, target);
        // MH_CreateHook has already allocated a trampoline and written
        // `original`. Leaving them behind makes the contract here "either the
        // detour is live or the target is untouched" a lie, and both callers
        // rely on it - InstallCursorHook carries on after a failure and would
        // otherwise hold a trampoline pointer into a hook nothing enables.
        MH_RemoveHook(target);
        *original = nullptr;
        return false;
    }
    return true;
}

}  // namespace ArxHeadTracking
