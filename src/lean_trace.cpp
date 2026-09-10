// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "lean_trace.h"

#include "arx_game.h"

#include <cmath>

namespace ArxHeadTracking {

namespace {

// int EERIELaunchRay3(EERIE_3D* orgn, EERIE_3D* dest, EERIE_3D* hit, EERIEPOLY* ep, long flag)
//
// Walks the background grid from orgn toward dest in 1.5-unit steps. Returns
// non-zero when level geometry stopped it, and writes the point it reached into
// `hit` either way - so the caller gets a surface point on a block and the far
// end of the segment on a clear path. This is the same cast the game runs for
// its own light occlusion and its arrows.
using LaunchRay3Fn = int(__cdecl*)(const Eerie3D*, const Eerie3D*, Eerie3D*, void*, long);

LaunchRay3Fn g_launchRay3 = nullptr;

// The standoff core's clamp subtracts from whatever distance this reports.
float g_standoff = 0.0f;

// The shallowest approach the overreach below is sized for. A ray that met a
// surface more obliquely than this would need more travel than any fixed margin
// covers, so the divisor is floored rather than left to run away.
constexpr float kMinApproachCos = 0.35f;

float Length(const float v[3]) {
    return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
}

}  // namespace

void InitLeanTrace(const BuildProfile& profile, float standoff) {
    g_launchRay3 = reinterpret_cast<LaunchRay3Fn>(profile.addrLaunchRay3);
    g_standoff = standoff;
}

bool TraceAimPoint(const float start[3], const float direction[3], float maxDistance,
                   float outPoint[3], int& rayResult) {
    if (!g_launchRay3) return false;

    const Eerie3D origin{start[0], start[1], start[2]};
    const Eerie3D dest{start[0] + direction[0] * maxDistance,
                       start[1] + direction[1] * maxDistance,
                       start[2] + direction[2] * maxDistance};
    Eerie3D hit{dest.x, dest.y, dest.z};

    rayResult = g_launchRay3(&origin, &dest, &hit, nullptr, 1);
    outPoint[0] = hit.x;
    outPoint[1] = hit.y;
    outPoint[2] = hit.z;
    return true;
}

cameraunlock::camera::LeanObstruction LeanQuery(void* /*context*/,
                                                const cameraunlock::math::Vec3& start,
                                                const cameraunlock::math::Vec3& direction,
                                                float maxDistance) {
    cameraunlock::camera::LeanObstruction out;
    if (!g_launchRay3) return out;

    // A cast that stops where the lean stops cannot see the surface the lean is
    // about to come to rest against: the eye would travel the whole way, arrive
    // against the wall, and only pop back once the head pushed far enough for
    // the ray itself to cross it. EERIELaunchRay3 is a zero-extent line, so the
    // standoff lives in core's clamp and the cast has to overreach by the
    // distance core is going to subtract.
    //
    // That extra travel is a function of the STANDOFF, not of the lean: meeting
    // a surface at an angle needs skin/cos(approach) along the ray to buy skin
    // along the normal. Core has already added one skin to maxDistance, so what
    // is left to add is skin * (1/cos - 1). Scaling the whole distance instead
    // made the margin shrink with the lean, which left the smallest margin
    // exactly where it is needed most - a small lean into an oblique doorframe.
    const float margin = g_standoff * (1.0f / kMinApproachCos - 1.0f);

    const float startPos[3] = {start.x, start.y, start.z};
    const float dir[3] = {direction.x, direction.y, direction.z};
    float hit[3] = {0.0f, 0.0f, 0.0f};
    int rayResult = 0;
    if (!TraceAimPoint(startPos, dir, maxDistance + margin, hit, rayResult)) {
        return out;
    }
    // Negative is the cast running off the edge of the background grid or out of
    // steps. That says nothing about whether anything is in the way, and core
    // keeps `queried` separate from `blocked` precisely so it can be told apart
    // from an open room: reported as a failure, the lean passes through and the
    // log says the query is not answering.
    if (rayResult < 0) return out;

    out.queried = true;
    out.blocked = rayResult > 0;
    if (out.blocked) {
        const float delta[3] = {hit[0] - startPos[0], hit[1] - startPos[1], hit[2] - startPos[2]};
        out.distance = Length(delta);
    }
    return out;
}

}  // namespace ArxHeadTracking
