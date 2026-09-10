// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "camera_hook.h"

#include "aim_projection.h"
#include "arx_game.h"
#include "camera_diagnostics.h"
#include "cursor_hook.h"
#include "engine_boundary.h"
#include "engine_memory.h"
#include "game_state.h"
#include "hook_install.h"
#include "lean_trace.h"
#include "logging.h"
#include "roll_hook.h"

#include "cameraunlock/camera/lean_clamp.h"
#include "cameraunlock/math/vec3.h"

#include <windows.h>
#include <MinHook.h>

namespace ArxHeadTracking {

namespace {

using SetActiveCameraFn = void(__cdecl*)(void*);
// DANAE::Render is a virtual with no arguments, so `this` arrives in ECX. A
// __fastcall detour picks it up there and the unused EDX slot rides along.
using RenderFn = long(__fastcall*)(void*, void*);

const BuildProfile* g_profile = nullptr;
TrackingRuntime* g_tracking = nullptr;
Config g_cfg{};

SetActiveCameraFn g_origSetActiveCamera = nullptr;
RenderFn g_origRender = nullptr;

// The un-zoomed focal every zoom factor is measured against: the game's own 310,
// or the one written into CURRENT_BASE_FOCAL for a chosen field of view. Held as
// the rounded long the engine was actually given, so the factor is exactly
// 1.0000 in ordinary play rather than nearly.
float g_baseFocal = kReferenceFocal;

cameraunlock::camera::LeanClamp g_leanClamp;

// How far the clean eye may move between two rendered frames and still be the
// same camera. A run is under 20 units a frame at 60Hz; a level change, a
// teleport or a scripted cut moves it by hundreds at once.
constexpr float kCameraCutDistance = 250.0f;

// Per-frame state. Only ever touched on the game thread, inside DANAE::Render.
struct FrameState {
    FrameSample sample{};
    bool applied = false;
    // Whether the lean clamp was exercised this frame. Separate from `applied`
    // because a rotation-only frame applies a pose and no lean, and the clamp's
    // allowance has to be dropped either way.
    bool leanApplied = false;
    float cleanAngle[3] = {0.0f, 0.0f, 0.0f};
    float cleanPos[3] = {0.0f, 0.0f, 0.0f};
};
FrameState g_frame;

// The previous frame's clean eye, for the camera-cut test below.
float g_lastCleanPos[3] = {0.0f, 0.0f, 0.0f};
bool g_haveLastCleanPos = false;

// True when the eye jumped further than any movement could carry it, which is
// what a level load, a teleport or a scripted cut looks like from here. The
// lean allowance is a scalar held against one room's wall, so carrying it
// across a cut rations the first lean in the new room through the release ease
// for no reason.
bool CameraCut(const float cleanPos[3]) {
    if (!g_haveLastCleanPos) {
        g_haveLastCleanPos = true;
        return true;
    }
    const float dx = cleanPos[0] - g_lastCleanPos[0];
    const float dy = cleanPos[1] - g_lastCleanPos[1];
    const float dz = cleanPos[2] - g_lastCleanPos[2];
    return dx * dx + dy * dy + dz * dz >
           kCameraCutDistance * kCameraCutDistance;
}

EerieCamera* Subj() { return reinterpret_cast<EerieCamera*>(g_profile->addrSubj); }

// This frame's field of view, read live off the camera. subj.focal is the
// engine's own working value: it carries the base field of view, a drawn bow's
// zoom and Magic Sight's, and the game recomputes it every frame.
ZoomBasis FrameZoom() { return MakeZoomBasis(Subj()->focal, g_baseFocal); }

void ApplyTrackedRotation(EerieCamera& subj, float zoomFactor) {
    const EngineRotation r = ToEngineRotation(g_frame.sample, zoomFactor);
    subj.angle_a = ComposeEnginePitch(g_frame.cleanAngle[0], r.dPitch);
    subj.angle_b = g_frame.cleanAngle[1] + r.dYaw;
    subj.angle_g = g_frame.cleanAngle[2] + r.dRoll;
}

void ApplyTrackedPosition(EerieCamera& subj, float zoomFactor) {
    float offset[3];
    ToEngineOffset(g_frame.sample, g_frame.cleanAngle[1], zoomFactor, offset);

    if (g_cfg.collision_enabled) {
        const cameraunlock::math::Vec3 eye(g_frame.cleanPos[0], g_frame.cleanPos[1],
                                           g_frame.cleanPos[2]);
        const cameraunlock::math::Vec3 want(offset[0], offset[1], offset[2]);
        // The sweep starts from the CLEAN eye and the clamp runs before the
        // offset reaches the camera. Clamping afterwards would mean reading back
        // a position that is already inside the wall.
        const cameraunlock::math::Vec3 allowed =
            g_leanClamp.Apply(eye, want, g_frame.sample.delta_time, &LeanQuery, nullptr);
        offset[0] = allowed.x;
        offset[1] = allowed.y;
        offset[2] = allowed.z;
        g_frame.leanApplied = true;
        ReportLeanState(g_leanClamp);
    }

    subj.pos_x = g_frame.cleanPos[0] + offset[0];
    subj.pos_y = g_frame.cleanPos[1] + offset[1];
    subj.pos_z = g_frame.cleanPos[2] + offset[2];
}

void ApplyTrackedPose() {
    EerieCamera* subj = Subj();

    g_frame.cleanAngle[0] = subj->angle_a;
    g_frame.cleanAngle[1] = subj->angle_b;
    g_frame.cleanAngle[2] = subj->angle_g;
    g_frame.cleanPos[0] = subj->pos_x;
    g_frame.cleanPos[1] = subj->pos_y;
    g_frame.cleanPos[2] = subj->pos_z;
    g_frame.applied = true;

    if (CameraCut(g_frame.cleanPos)) g_leanClamp.Reset();
    g_lastCleanPos[0] = g_frame.cleanPos[0];
    g_lastCleanPos[1] = g_frame.cleanPos[1];
    g_lastCleanPos[2] = g_frame.cleanPos[2];

    const ZoomBasis zoom = FrameZoom();
    if (g_frame.sample.has_rotation) ApplyTrackedRotation(*subj, zoom.factor);
    if (g_frame.sample.has_position) ApplyTrackedPosition(*subj, zoom.factor);
    BeginRenderRoll(subj->angle_g);

    const float projScale = ProjectionScale(zoom.focal, ReadLong(g_profile->addrDanaeSizY));
    const AimPoint& aim =
        ComputeAimPoint(*subj, g_frame.cleanPos, g_frame.cleanAngle, projScale);
    // Now, rather than at the frame boundary, so the mark is drawn on the frame
    // it was computed for.
    PlaceCursorOnAimPoint();
    ReportPoseDiagnostics(*subj, g_frame.cleanAngle, g_frame.cleanPos, aim, projScale);
}

void RestoreCleanPose() {
    if (!g_frame.applied) return;
    EerieCamera* subj = Subj();
    subj->angle_a = g_frame.cleanAngle[0];
    subj->angle_b = g_frame.cleanAngle[1];
    subj->angle_g = g_frame.cleanAngle[2];
    subj->Zcos = std::cos(subj->angle_g * kDegToRad);
    subj->Zsin = std::sin(subj->angle_g * kDegToRad);
    subj->pos_x = g_frame.cleanPos[0];
    subj->pos_y = g_frame.cleanPos[1];
    subj->pos_z = g_frame.cleanPos[2];
    g_frame.applied = false;
}

void __cdecl Detour_SetActiveCamera(void* cam) {
    if (cam == reinterpret_cast<void*>(g_profile->addrSubj) && !g_frame.applied &&
        (g_frame.sample.has_rotation || g_frame.sample.has_position) && IsGameplayFrame()) {
        ApplyTrackedPose();
    }
    g_origSetActiveCamera(cam);
}

long __fastcall Detour_Render(void* self, void* edx) {
    // Sampled here, before anything in the frame has run, so the pipeline
    // advances exactly once per rendered frame.
    g_frame.sample = g_tracking->SampleFrame();
    g_frame.applied = false;
    g_frame.leanApplied = false;

    const long hr = g_origRender(self, edx);

    // Captured BEFORE the restore. RestoreCleanPose() ends by clearing the flag,
    // so testing it afterwards is testing a constant - which is how the guard
    // further down came to fire on every frame and undo its own purpose.
    const bool appliedThisFrame = g_frame.applied;

    // The frame boundary. Every projection is done, and everything the game does
    // before the next one - player movement, spell launches, the interpolation
    // the death camera starts from - reads the camera it built itself.
    RestoreCleanPose();
    EndRenderRoll();
    RestoreCursor();

    // Invalidated HERE, not at frame entry, and only when this frame produced
    // nothing. Arx picks what the cursor is over near the top of DANAE::Render,
    // hundreds of calls before it sets the camera the mod hooks, so the only aim
    // point the pick can possibly use is the PREVIOUS frame's - which is exactly
    // what it should use, since the pick tests against the bounding boxes that
    // same frame projected. Clearing on the way in threw that value away and
    // left the crosshair drawn on the aim point while the pick and the click
    // stayed at screen centre.
    if (!appliedThisFrame) ClearAimPoint();

    // After the restore: the zoom basis gates on IsGameplayFrame(), which reads
    // the camera angles back against the player's, so a tracked pose still
    // standing in subj would fail that test on every frame the mod is working
    // and the line would only ever appear when tracking was doing nothing.
    ReportZoomBasis(g_baseFocal);
    ReportZoomUnreadable(FrameZoom());
    ReportFrameEndProjection();

    if (!g_frame.leanApplied) {
        // A frame that applied no lean at all - the gate closed, the tracker
        // stopped, rotation-only mode, a level loaded - must not carry the
        // previous room's wall into the next one.
        g_leanClamp.Reset();
    }
    return hr;
}

// The player's field of view, applied the way the game would have applied its
// own: by writing the base focal Arx builds every frame's projection from.
//
// BASE_FOCAL is recomputed each frame as CURRENT_BASE_FOCAL + FOKMOD +
// BOW_FOCAL/4, and subj.focal is eased onto it, so this one write reaches every
// consumer at once and the engine stays the only thing deriving from it: the
// projection matrix, the sprite and halo scale, the frustum the level is culled
// against, and the deliberately narrower focal the player's own arms are drawn
// with. It also means a drawn bow keeps narrowing the view by the same amount
// of focal it always did.
//
// Two hooks around PrepareCamera and PrepareActiveCamera would put the same
// field of view in the projection matrix, and are what this replaced. There is
// nothing to recommend them: they run every frame, they leave the camera holding
// one field of view and the engine's other derivations holding another, and
// PrepareActiveCamera had to be found the hard way after the first pair silently
// did nothing to the picture.
//
// CURRENT_BASE_FOCAL is statically initialised in the image and never written by
// the game, so one write holds for the session.
void ApplyFieldOfView() {
    if (g_cfg.field_of_view <= 0.0f) return;

    const long focal = BaseFocalForFov(g_cfg.field_of_view);
    // The reference before the engine's own value. Neither order is observable:
    // this runs on the init thread while the game is still on its menus, where
    // the gameplay gate is shut and no pose is applied, and the engine then eases
    // subj.focal onto the new base over many frames regardless.
    g_baseFocal = static_cast<float>(focal);
    WriteLong(g_profile->addrCurrentBaseFocal, static_cast<int32_t>(focal));

    Log::Line("Field of view: %.2f degrees vertical instead of the game's %.2f, written as "
              "CURRENT_BASE_FOCAL=%ld. Drawing a bow still narrows it by the same ratio.",
              FocalToFovDegrees(g_baseFocal), FocalToFovDegrees(kReferenceFocal), focal);
}

}  // namespace

bool InstallCameraHook(const BuildProfile& profile, TrackingRuntime& tracking,
                       const Config& cfg) {
    g_profile = &profile;
    g_tracking = &tracking;
    g_cfg = cfg;

    InitAimProjection(profile, cfg.move_crosshair);
    InitCameraDiagnostics(profile, cfg.diagnostics);

    cameraunlock::camera::LeanClampSettings lean;
    lean.skin = cfg.collision_radius;
    lean.release_smoothing = cfg.collision_release_smoothing;
    g_leanClamp.SetSettings(lean);

    if (MH_Initialize() != MH_OK) {
        Log::Line("ERROR: MinHook failed to initialise");
        return false;
    }

    // A failed install has to leave the process exactly as it was found, the
    // same as an unrecognised build does. Without the unwind the Render detour
    // stays live for the rest of the session on a mod that has already reported
    // itself dead and stopped the runtime that detour samples from.
    // MH_Uninitialize disables and removes every hook, which is the whole of
    // what has been installed by this point.
    if (!InstallDetour(reinterpret_cast<void*>(profile.addrRender),
                       reinterpret_cast<void*>(&Detour_Render),
                       reinterpret_cast<void**>(&g_origRender), "DANAE::Render") ||
        !InstallDetour(reinterpret_cast<void*>(profile.addrSetActiveCamera),
                       reinterpret_cast<void*>(&Detour_SetActiveCamera),
                       reinterpret_cast<void**>(&g_origSetActiveCamera), "SetActiveCamera") ||
        !InstallRollHooks(profile, cfg.diagnostics)) {
        MH_Uninitialize();
        return false;
    }

    Log::Line("Camera hooks installed (Render=0x%08X, SetActiveCamera=0x%08X, collision=%s)",
              static_cast<unsigned>(profile.addrRender),
              static_cast<unsigned>(profile.addrSetActiveCamera),
              cfg.collision_enabled ? "on" : "off");
    // After the hooks are known to be in. This writes one of the game's own
    // globals and nothing restores it, so doing it earlier left a failed
    // install having changed the player's field of view for a session with no
    // mod running - and "a failed install has to leave the process exactly as
    // it was found" is the rule directly above.
    ApplyFieldOfView();
    return true;
}

}  // namespace ArxHeadTracking
