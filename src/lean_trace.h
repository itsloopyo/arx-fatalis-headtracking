// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"

#include "cameraunlock/camera/lean_clamp.h"

namespace ArxHeadTracking {

// @p radius is how far the swept eye is held off level geometry and objects, in
// Arx units. The sweep carries the whole standoff, so core's clamp runs with a
// skin of zero.
void InitLeanTrace(const BuildProfile& profile, float radius);

// The engine half of the lean clamp: sweeps a sphere from the clean eye toward
// where the head wants to go, against the level polygons and the meshes of the
// objects near the player, and reports the distance its centre can travel
// before anything comes within the radius. Core owns what to do with the answer.
//
// Distances are in Arx units (centimetres), matching everything else that
// reaches the camera struct.
cameraunlock::camera::LeanObstruction LeanQuery(void* context,
                                                const cameraunlock::math::Vec3& start,
                                                const cameraunlock::math::Vec3& direction,
                                                float maxDistance);

// What the last LeanQuery did, for the diagnostic line.
struct LeanTraceStats {
    bool queried = false;
    bool blocked = false;
    // True when an object, not the level, was the nearest contact.
    bool object = false;
    float distance = 0.0f;
    unsigned polygons = 0;
    unsigned objects = 0;
    unsigned faces = 0;
    unsigned triangles = 0;
    float levelMicroseconds = 0.0f;
    float microseconds = 0.0f;
};
const LeanTraceStats& LastLeanTrace();

// Casts the game's own level ray along `direction` from `start`, for the aim
// point the cursor is drawn on. False only when the ray function is
// unavailable.
//
// @p rayResult is EERIELaunchRay3's own answer, which has three cases, not two:
// positive means level geometry stopped it, zero means it reached the far end of
// the segment through open space, and NEGATIVE means the cast ran off the edge of
// the background grid or out of steps - which is not an answer about geometry at
// all. @p outPoint is written in every case.
bool TraceAimPoint(const float start[3], const float direction[3], float maxDistance,
                   float outPoint[3], int& rayResult);

}  // namespace ArxHeadTracking
