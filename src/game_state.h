// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"

namespace ArxHeadTracking {

void InitGameState(const BuildProfile& profile);

// Whether this frame is ordinary first-person play, which is the only state
// head tracking is applied in.
//
// Menus, the loading screen and 2D cinematics need no test at all: DANAE::Render
// returns through `if (ARX_Menu_Render(...)) goto norenderend;` long before it
// ever touches the camera, so the hook simply is not reached.
bool IsGameplayFrame();

}  // namespace ArxHeadTracking
