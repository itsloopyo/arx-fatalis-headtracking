// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"
#include "config.h"
#include "tracking_runtime.h"

namespace ArxHeadTracking {

// Installs the two camera detours: DANAE::Render (the frame boundary, where the
// tracker is sampled and the clean camera is put back) and SetActiveCamera (the
// one moment per frame when the game has finished building the player's camera
// and has not yet projected anything through it).
bool InstallCameraHook(const BuildProfile& profile, TrackingRuntime& tracking,
                       const Config& cfg);

}  // namespace ArxHeadTracking
