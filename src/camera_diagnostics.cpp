// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "camera_diagnostics.h"

#include "engine_boundary.h"
#include "engine_memory.h"
#include "game_state.h"
#include "logging.h"

#include <windows.h>

#include <cmath>
#include <cstddef>

namespace ArxHeadTracking {

namespace {

// How often the per-second lines may repeat, in milliseconds. Slow enough that
// the log stays readable over a session, fast enough to hold a pose and watch it.
constexpr DWORD kDiagnosticIntervalMs = 1000;

// Byte offset of ProjectionMatrix._22 within EERIEMATRIX, the term ProjectionScale
// reproduces.
constexpr uintptr_t kProjectionMatrix22Offset = 20;

// How far apart use_focal and focal * Xratio may sit and still count as the
// camera having been prepared with the focal standing in it now.
constexpr float kFocalAgreementTolerance = 0.01f;

// How far apart our projection scale and the engine's _22 may sit, in pixels,
// and still count as the same projection.
constexpr float kScaleAgreementTolerance = 0.5f;

// Frames of agreement before the basis line is written, and frames of
// disagreement before it is reported as a fault. The patience is deliberately
// long: a level load holds the projection matrix for twenty seconds or more
// while the gameplay gate already reads as live, and a warning fired inside that
// window would be exactly the false alarm this is guarding against. A units
// fault never resolves, so waiting minutes for it costs nothing.
constexpr int kFramesBeforeBasisLine = 30;
constexpr int kFramesBeforeScaleWarning = 18000;

// Lean clamp transitions worth writing down. Every line is flushed to disk, and
// contact flips as fast as a player leans in and out of a doorway, so past this
// many the log stops being a record of the session and becomes a record of one
// wall. The first few carry everything the state has to say.
constexpr int kMaxLeanStateReports = 20;

const BuildProfile* g_profile = nullptr;
bool g_perFrameLines = false;

const EerieCamera* Subj() {
    return reinterpret_cast<const EerieCamera*>(g_profile->addrSubj);
}

// True at most once per kDiagnosticIntervalMs for the caller identified by
// `lastTick`, of which each per-second line owns its own copy.
bool IntervalElapsed(DWORD& lastTick) {
    const DWORD now = GetTickCount();
    if (lastTick != 0 && now - lastTick < kDiagnosticIntervalMs) return false;
    lastTick = now;
    return true;
}

}  // namespace

void InitCameraDiagnostics(const BuildProfile& profile, bool perFrameLines) {
    g_profile = &profile;
    g_perFrameLines = perFrameLines;
}

// A factor wrong by a constant reads exactly like a factor that is right, so the
// gate is that this line says 1.0000 in ordinary play. It is taken off the
// CAMERA rather than off the pose, so the basis is visible with no tracker
// connected and no save loaded, and off the end of a rendered frame, where the
// projection matrix is the one the level was drawn through.
//
// Both fields of view are VERTICAL degrees. There is no aspect term in the
// ratio: EERIE_CreateMatriceProj takes a vertical FOV, both numbers come out of
// the same focal map, and the horizontal is whatever the window's width makes of
// it.
void ReportZoomBasis(float baseFocal) {
    static bool s_done = false;
    if (s_done || !IsGameplayFrame()) return;

    // PrepareCamera writes use_focal = focal * Xratio every time it runs, so
    // these agreeing is the frame saying the camera was prepared with the focal
    // standing in it now.
    const EerieCamera* subj = Subj();
    const float focal = subj->focal;
    const float xratio = ReadFloat(g_profile->addrXratio);
    if (!(xratio > 0.0f) ||
        std::fabs(subj->use_focal - focal * xratio) > kFocalAgreementTolerance) {
        return;
    }

    const int32_t sizY = ReadLong(g_profile->addrDanaeSizY);
    const float ours = ProjectionScale(focal, sizY);
    const float theirs = ReadFloat(g_profile->addrProjectionMatrix + kProjectionMatrix22Offset);

    // Counted only over frames the engine has rebuilt its projection on. A
    // loading screen leaves the matrix holding whatever the last thing rendered
    // put there while the gate above still reads as gameplay, and a line logged
    // then compares our scale against a matrix nothing has rebuilt yet - which
    // is indistinguishable from the units fault this line exists to catch. So
    // the wait is for agreement, and only a wait that never ends is the finding.
    static int s_agree = 0;
    static int s_waiting = 0;
    if (!(ours > 0.0f) || std::fabs(ours - theirs) > kScaleAgreementTolerance) {
        if (++s_waiting < kFramesBeforeScaleWarning) return;
        s_done = true;
        Log::Line("WARN: our projection scale %.2f does not match the engine's "
                  "ProjectionMatrix._22 %.2f at focal %.1f (DANAESIZY=%d). The cursor will be "
                  "placed through a different field of view than the frame is drawn with.",
                  ours, theirs, focal, sizY);
        return;
    }
    if (++s_agree <= kFramesBeforeBasisLine) return;
    s_done = true;

    const ZoomBasis zoom = MakeZoomBasis(focal, baseFocal);
    Log::Line("Zoom basis: rendering %.2f deg vertical (focal %.1f, tan half %.4f) against a "
              "base of %.2f deg (focal %.1f, tan half %.4f) | factor=%.4f",
              zoom.fov, zoom.focal, TanHalfFov(zoom.fov), zoom.baseFov, zoom.baseFocal,
              TanHalfFov(zoom.baseFov), zoom.factor);
    Log::Line("Zoom cross-check: our scale=%.2f, the engine's ProjectionMatrix._22=%.2f "
              "(DANAESIZY=%d). These agree.",
              ours, theirs, sizY);
}

void ReportFrameEndProjection() {
    if (!g_perFrameLines) return;
    static DWORD s_lastTick = 0;
    if (!IntervalElapsed(s_lastTick)) return;

    const EerieCamera* subj = Subj();
    Log::Line("diag frame end: proj _11=%.2f _22=%.2f | subj.focal=%.1f use_focal=%.1f",
              ReadFloat(g_profile->addrProjectionMatrix),
              ReadFloat(g_profile->addrProjectionMatrix + kProjectionMatrix22Offset),
              subj->focal, subj->use_focal);
}

void ReportZoomUnreadable(const ZoomBasis& zoom) {
    static bool s_reported = false;
    if (s_reported || zoom.usable) return;
    s_reported = true;
    Log::Line("WARN: the camera's focal read back as %.1f, which is not a field of view this "
              "mod can measure against. The head pose is being applied unscaled, so it will "
              "feel stronger than it should while the game is zoomed.", zoom.focal);
}

// Transitions alone cannot tell "the trace runs and the room is open" from "the
// trace is not running", and those need different fixes, so the query failure is
// carried alongside the contact state.
void ReportLeanState(const cameraunlock::camera::LeanClamp& clamp) {
    static int s_last = -1;
    static int s_reports = 0;
    const int state = (clamp.LastQueryFailed() ? 2 : 0) | (clamp.InContact() ? 1 : 0);
    if (state == s_last || s_reports == kMaxLeanStateReports) return;
    s_last = state;

    const char* what = "clear, the lean is unrestricted.";
    if (state & 2) {
        what = "the world query is not answering, so the lean is passing through unclamped.";
    } else if (state & 1) {
        what = "holding the eye off a surface.";
    }

    ++s_reports;
    Log::Line("Lean clamp: %s%s", what,
              s_reports == kMaxLeanStateReports
                  ? " (further lean clamp changes not logged)" : "");
}

// A reticle that has drifted shows up here as an aim point that moved when only
// the head did.
void ReportPoseDiagnostics(const EerieCamera& subj, const float cleanAngle[3],
                           const float cleanPos[3], const AimPoint& aim, float projScale) {
    if (!g_perFrameLines) return;
    static DWORD s_lastTick = 0;
    if (!IntervalElapsed(s_lastTick)) return;

    Log::Line("diag clean angle=(%.2f,%.2f,%.2f) pos=(%.1f,%.1f,%.1f) | tracked "
              "angle=(%.2f,%.2f,%.2f) pos=(%.1f,%.1f,%.1f) | offset=(%.1f,%.1f,%.1f)",
              cleanAngle[0], cleanAngle[1], cleanAngle[2],
              cleanPos[0], cleanPos[1], cleanPos[2],
              subj.angle_a, subj.angle_b, subj.angle_g, subj.pos_x, subj.pos_y, subj.pos_z,
              subj.pos_x - cleanPos[0], subj.pos_y - cleanPos[1], subj.pos_z - cleanPos[2]);

    const float dx = aim.world[0] - cleanPos[0];
    const float dy = aim.world[1] - cleanPos[1];
    const float dz = aim.world[2] - cleanPos[2];
    Log::Line("diag aim world=(%.1f,%.1f,%.1f) %s range=%.1f | screen=(%.1f,%.1f) valid=%d "
              "centre=(%d,%d) scale=%.2f",
              aim.world[0], aim.world[1], aim.world[2], aim.hitGeometry ? "hit" : "open",
              std::sqrt(dx * dx + dy * dy + dz * dz),
              aim.screenX, aim.screenY, aim.valid ? 1 : 0,
              ReadLong(g_profile->addrDanaeCenterX), ReadLong(g_profile->addrDanaeCenterY),
              projScale);

    Log::Line("diag mouselook=%d interface=0x%04X cursor=(%d,%d) externalview=%d "
              "blockcontrols=%d cinemascope=%d",
              ReadLong(g_profile->addrMouseLookOn),
              static_cast<uint16_t>(ReadShort(g_profile->addrPlayerInterface)),
              ReadShort(g_profile->addrDanaeMouse + offsetof(EerieS2D, x)),
              ReadShort(g_profile->addrDanaeMouse + offsetof(EerieS2D, y)),
              ReadLong(g_profile->addrExternalView),
              ReadLong(g_profile->addrBlockControls), ReadLong(g_profile->addrCinemascope));
}

}  // namespace ArxHeadTracking
