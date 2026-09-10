// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "aim_projection.h"
#include "arx_game.h"
#include "build_profile.h"
#include "engine_boundary.h"

#include "cameraunlock/camera/lean_clamp.h"

// Everything the mod writes to the log about the camera it is injecting into,
// kept away from the injection itself so the per-frame path reads as the pose
// arithmetic it is.
namespace ArxHeadTracking {

// `perFrameLines` is the INI's Diagnostics switch. The one-shot lines below are
// written whether or not it is set; only the per-second ones depend on it.
void InitCameraDiagnostics(const BuildProfile& profile, bool perFrameLines);

// One line, once, naming every term of the zoom factor, plus the number that
// proves the mod projects through the same field of view the engine renders.
// `baseFocal` is the un-zoomed focal the factor is measured against.
void ReportZoomBasis(float baseFocal);

// Says once that the field of view could not be read, so the pose is passing
// through unscaled. An honest factor of 1.0 and a missing measurement look
// identical in the basis line, so the difference has to be stated.
void ReportZoomUnreadable(const ZoomBasis& zoom);

// The projection as the frame left it, which is the one the level was drawn
// through. A field of view that is not reaching the picture shows up here as a
// _22 that does not match the focal beside it, and nowhere else.
void ReportFrameEndProjection();

// Reports a change in whether the lean clamp is holding the eye off a surface,
// and whether its query is running at all.
void ReportLeanState(const cameraunlock::camera::LeanClamp& clamp);

// The per-second line the aim tests are decided on: the clean camera the shot
// leaves from, the tracked camera the frame is drawn through, the world point
// the aim cast reached, and where that point lands on screen.
void ReportPoseDiagnostics(const EerieCamera& subj, const float cleanAngle[3],
                           const float cleanPos[3], const AimPoint& aim, float projScale);

}  // namespace ArxHeadTracking
