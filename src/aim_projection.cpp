// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "aim_projection.h"

#include "engine_memory.h"
#include "lean_trace.h"
#include "logging.h"
#include "roll_math.h"

#include <cmath>

namespace ArxHeadTracking {

namespace {

// How far the aim cast reaches when the camera has no far distance of its own
// yet, in Arx units. Every camera the game builds sets cdepth, so this covers
// only the frames before one has.
constexpr float kFallbackAimReach = 2100.0f;

const BuildProfile* g_profile = nullptr;
bool g_enabled = false;
AimPoint g_aim;

// A private copy of the camera the frame is about to be drawn through, carrying
// the six trig terms PrepareCamera is about to compute. PrepareCamera runs after
// the camera hook, so the real struct's terms are still last frame's; the copy
// exists only so the aim point can be projected through the basis this frame
// uses.
EerieCamera MakeProjectionCamera(const EerieCamera& subj) {
    EerieCamera cam = subj;
    cam.Xcos = std::cos(subj.angle_a * kDegToRad);
    cam.Xsin = std::sin(subj.angle_a * kDegToRad);
    cam.Ycos = std::cos(subj.angle_b * kDegToRad);
    cam.Ysin = std::sin(subj.angle_b * kDegToRad);
    // The game overwrites these with DANAECENTERX/Y immediately after
    // PrepareCamera, so take them from the same source rather than from the
    // struct's stale copy.
    cam.posleft = static_cast<float>(ReadLong(g_profile->addrDanaeCenterX));
    cam.postop = static_cast<float>(ReadLong(g_profile->addrDanaeCenterY));
    return cam;
}

}  // namespace

void InitAimProjection(const BuildProfile& profile, bool moveCrosshair) {
    g_profile = &profile;
    g_enabled = moveCrosshair;
}

void ClearAimPoint() {
    g_aim.valid = false;
}

const AimPoint& ComputeAimPoint(const EerieCamera& subj, const float cleanPos[3],
                                const float cleanAngle[3], float projScale) {
    g_aim.valid = false;
    if (!g_enabled) return g_aim;
    if (!(projScale > 0.0f)) {
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            Log::Line("WARN: the camera's field of view could not be turned into a projection "
                      "scale, so the cursor stays where the game puts it.");
        }
        return g_aim;
    }

    const EerieCamera trackedCam = MakeProjectionCamera(subj);
    const Eerie3D forward = ForwardFromAngles(cleanAngle[0], cleanAngle[1]);
    const float dir[3] = {forward.x, forward.y, forward.z};

    float hit[3] = {0.0f, 0.0f, 0.0f};
    int rayResult = 0;
    const float reach = trackedCam.cdepth > 1.0f ? trackedCam.cdepth : kFallbackAimReach;
    if (!TraceAimPoint(cleanPos, dir, reach, hit, rayResult)) {
        // The cast is the only thing that knows where the aim lands. Without it
        // there is no honest screen position, so the cursor is left where the
        // game put it rather than moved to a guess.
        static bool s_warned = false;
        if (!s_warned) {
            s_warned = true;
            Log::Line("WARN: the aim cast is unavailable, so the cursor stays where the game "
                      "puts it and will not follow the aim point.");
        }
        return g_aim;
    }

    g_aim.world[0] = hit[0];
    g_aim.world[1] = hit[1];
    g_aim.world[2] = hit[2];
    // Only a POSITIVE result is level geometry. Zero is the far end of a clear
    // segment and negative is the cast leaving the background grid, and calling
    // either of those a hit mislabels the diagnostic line a cursor complaint is
    // read against.
    g_aim.hitGeometry = rayResult > 0;

    ScreenPoint sp{};
    const Eerie3D aim{hit[0], hit[1], hit[2]};
    if (!ProjectWorldPoint(trackedCam, projScale, aim, sp)) return g_aim;
    ViewRoll(subj.angle_g).ApplyScreen(sp.x, sp.y, trackedCam.posleft, trackedCam.postop);

    g_aim.screenX = sp.x;
    g_aim.screenY = sp.y;
    g_aim.valid = true;
    return g_aim;
}

bool GetAimScreenPosition(float& x, float& y) {
    if (!g_aim.valid) return false;
    x = g_aim.screenX;
    y = g_aim.screenY;
    return true;
}

}  // namespace ArxHeadTracking
