// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "arx_game.h"
#include "build_profile.h"

namespace ArxHeadTracking {

// Where the point the player is actually aiming at landed this frame.
//
// The player still aims with the mouse, so the aim ray starts at the CLEAN eye
// and runs along the CLEAN facing. What that ray reaches is a world POINT, and
// the cursor belongs wherever that point projects into the frame the player is
// looking at. Projecting the aim DIRECTION instead would agree at exactly one
// distance and split either side of it the moment the head leans.
struct AimPoint {
    // Whether `screenX` / `screenY` hold a position to draw on. False when the
    // crosshair is not being moved, the cast could not run, or the point is
    // behind the rendered eye.
    bool valid = false;
    float screenX = 0.0f;
    float screenY = 0.0f;

    // The world point the cast reached, and whether level geometry stopped it.
    // Held for the diagnostic line, and left standing from the last successful
    // cast when this frame's did not run.
    float world[3] = {0.0f, 0.0f, 0.0f};
    bool hitGeometry = false;
};

void InitAimProjection(const BuildProfile& profile, bool moveCrosshair);

// Casts along the clean facing and projects where it lands into `subj`, the
// camera the frame is about to be drawn through. Returns this frame's aim
// point, which is also what GetAimScreenPosition reports until the next frame
// clears it.
const AimPoint& ComputeAimPoint(const EerieCamera& subj, const float cleanPos[3],
                                const float cleanAngle[3], float projScale);

// Invalidates the aim point at the frame boundary, leaving the world point
// standing for the diagnostic line.
void ClearAimPoint();

// Where the point the player is actually aiming at lands on screen this frame,
// in game pixels. False when there is no compensation to apply - tracking off,
// gate closed, or the aim point is behind the rendered eye.
bool GetAimScreenPosition(float& x, float& y);

}  // namespace ArxHeadTracking
