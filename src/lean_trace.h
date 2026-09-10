// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"

#include "cameraunlock/camera/lean_clamp.h"

namespace ArxHeadTracking {

// @p standoff is the distance core's clamp will subtract from whatever this
// reports, in Arx units. The cast has to overreach by enough to see the surface
// the lean is about to come to rest against, and how much that is depends on the
// standoff, not on the lean.
void InitLeanTrace(const BuildProfile& profile, float standoff);

// The engine half of the lean clamp: asks Arx's own level collision whether
// anything sits between the clean eye and where the head wants to go. Core owns
// what to do with the answer.
//
// Distances are in Arx units (centimetres), matching everything else that
// reaches the camera struct.
cameraunlock::camera::LeanObstruction LeanQuery(void* context,
                                                const cameraunlock::math::Vec3& start,
                                                const cameraunlock::math::Vec3& direction,
                                                float maxDistance);

// Casts the game's own level ray along `direction` from `start`, for the aim
// point the cursor is drawn on and for the lean clamp's query. False only when
// the ray function is unavailable.
//
// @p rayResult is EERIELaunchRay3's own answer, which has three cases, not two:
// positive means level geometry stopped it, zero means it reached the far end of
// the segment through open space, and NEGATIVE means the cast ran off the edge of
// the background grid or out of steps - which is not an answer about geometry at
// all. @p outPoint is written in every case.
bool TraceAimPoint(const float start[3], const float direction[3], float maxDistance,
                   float outPoint[3], int& rayResult);

}  // namespace ArxHeadTracking
