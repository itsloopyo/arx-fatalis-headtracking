// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

namespace ArxHeadTracking {

// Creates and enables one MinHook detour, naming the step that failed in the
// log. `what` is the engine function's name, which is what a player's log has
// to carry for the failure to mean anything.
bool InstallDetour(void* target, void* detour, void** original, const char* what);

}  // namespace ArxHeadTracking
