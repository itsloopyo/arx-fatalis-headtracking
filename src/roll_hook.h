// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"

namespace ArxHeadTracking {

bool InstallRollHooks(const BuildProfile& profile, bool diagnostics);
void BeginRenderRoll(float degrees);
void EndRenderRoll();

}  // namespace ArxHeadTracking
