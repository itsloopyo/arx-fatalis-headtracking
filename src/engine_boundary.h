// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "arx_game.h"
#include "tracking_runtime.h"

#include "cameraunlock/camera/zoom_compensation.h"

#include <cmath>

// Where the tracker's conventions become Arx's. Every sign flip, unit change and
// zoom scale the pose goes through lives here and nowhere else, so the tests can
// hold them still.
namespace ArxHeadTracking {

// The head rotation, converted from the tracker's convention to Arx's.
//
// Arx's angle.b grows turning left and angle.a grows looking down.
// Both tracker angles invert at this boundary.
struct EngineRotation {
    float dPitch;
    float dYaw;
    float dRoll;
};

// ScaleAngleForZoom takes the tangent of the angle and the arctangent of the
// result, which is single-valued only on (-90, 90). Outside it the angle wraps:
// 120 degrees comes back as -60 and the view snaps round to face the other way,
// and it does so at a zoom factor of exactly 1.0, so ordinary play is where it
// would land.
//
// Nothing upstream bounds the angle. The wire carries whatever the tracker
// sends, and the INI sensitivity it is multiplied by is deliberately unbounded
// short of cameraunlock::config::kMaxSensitivity, so a 40 degree turn at a
// sensitivity of 3 already leaves the domain. Bound it here, where the tracker's
// convention becomes Arx's and every other conversion of the same kind lives.
// No pose a neck produces reaches this, so nothing a player can do changes.
constexpr float kMaxTrackedAngleDegrees = 89.0f;

inline float ClampTrackedAngle(float degrees) {
    if (degrees > kMaxTrackedAngleDegrees) return kMaxTrackedAngleDegrees;
    if (degrees < -kMaxTrackedAngleDegrees) return -kMaxTrackedAngleDegrees;
    return degrees;
}

// The band the engine itself keeps the player's pitch inside.
//
// ARX_Interface.cpp pushes player.desiredangle.a back out of [74.9, 301] on
// every mouse frame, so the game never renders a pitch past 74.9 degrees down
// or 59 up. The head pitch is ADDED to that, and nothing bounded the sum. Past
// 90 degrees cos(angle.a) changes sign and specialEE_RTP2's sz term goes
// negative for everything in front of the eye: the world ahead is culled, the
// world behind is drawn, and the view snaps round. A 75 degree look at the
// floor plus 16 degrees of head tilt reaches it, which is an ordinary pose
// rather than a stunt.
//
// Yaw needs no equivalent. The engine leaves it unbounded and the trig carries
// any magnitude.
constexpr float kEnginePitchDown = 74.9f;
constexpr float kEnginePitchUp = -59.0f;

// The engine stores angle.a wrapped into [0, 360), so a look upward arrives as
// 301..360 rather than as a negative number and has to be unwrapped before
// anything is added to it.
inline float UnwrapEngineAngle(float degrees) {
    return degrees > 180.0f ? degrees - 360.0f : degrees;
}

inline float WrapEngineAngle(float degrees) {
    float wrapped = std::fmod(degrees, 360.0f);
    if (wrapped < 0.0f) wrapped += 360.0f;
    return wrapped;
}

// The clean pitch plus the head's, bounded to what the engine can draw and
// handed back in the engine's own wrapped representation so subj holds what the
// game itself would have put there.
inline float ComposeEnginePitch(float cleanPitch, float dPitch) {
    const float clean = UnwrapEngineAngle(cleanPitch);
    // The band always admits whatever the engine itself put there. The mouse
    // path is not the only writer of player.angle.a - the PLAYERLOOKAT script
    // command sets it outright, without the band and without closing the
    // gameplay gate - and clamping a clean pitch that is already outside would
    // move the view on a frame the head is asking for nothing at all.
    const float lo = clean < kEnginePitchUp ? clean : kEnginePitchUp;
    const float hi = clean > kEnginePitchDown ? clean : kEnginePitchDown;

    float pitch = clean + dPitch;
    if (pitch > hi) pitch = hi;
    if (pitch < lo) pitch = lo;
    return WrapEngineAngle(pitch);
}

inline EngineRotation ToEngineRotation(const FrameSample& s, float zoom) {
    using cameraunlock::camera::ScaleAngleForZoom;
    EngineRotation out;
    // Both translate the image across the frame, so both scale with the zoom.
    out.dYaw = ScaleAngleForZoom(ClampTrackedAngle(-s.yaw), zoom);
    out.dPitch = ScaleAngleForZoom(ClampTrackedAngle(-s.pitch), zoom);
    // Mirrored like the yaw, and not scaled: roll turns the picture about the
    // view axis instead of moving it across the frame, so a held tilt covers
    // the same angle whatever the zoom.
    out.dRoll = -s.roll;
    return out;
}

// The lean, converted to Arx units on the player's own horizon basis.
//
// Core hands out metres with x right, y up and NEGATIVE z the forward lean. Arx
// is centimetres with x right, y DOWN and z forward, and the tracker's x is
// mirrored the same way its yaw is, so all three components invert on the way
// across. The basis comes from the CLEAN yaw only: the tracker measures where the
// head sits in the room, so the offset belongs on the body's facing and must not
// tip with where the player is looking.
inline void ToEngineOffset(const FrameSample& s, float cleanYaw, float zoom, float out[3]) {
    const HorizonBasis basis = MakeHorizonBasis(cleanYaw);
    const float right = -s.pos_x * kUnitsPerMetre * zoom;
    const float down = -s.pos_y * kUnitsPerMetre * zoom;
    const float forward = -s.pos_z * kUnitsPerMetre * zoom;

    out[0] = basis.right.x * right + basis.down.x * down + basis.forward.x * forward;
    out[1] = basis.right.y * right + basis.down.y * down + basis.forward.y * forward;
    out[2] = basis.right.z * right + basis.down.z * down + basis.forward.z * forward;
}

// tan(fov/2) for one of Arx's "focal" values, which is what the zoom factor is a
// ratio of. Both terms of that ratio come through here, so both are VERTICAL -
// the axis EERIE_CreateMatriceProj takes its argument in - and the pairing that
// would leave normal play quietly running at a fixed fraction of the pose
// cannot arise.
inline float TanHalfFovFromFocal(float focal) {
    return std::tan(FocalToFovDegrees(focal) * 0.5f * kDegToRad);
}

// tan(fov/2) for a field of view already in degrees.
inline float TanHalfFov(float fovDegrees) {
    return std::tan(fovDegrees * 0.5f * kDegToRad);
}

// The value to give the game's own CURRENT_BASE_FOCAL for a chosen vertical
// field of view. It is a long in the engine, so the rounding happens here and
// the rounded value is what everything downstream measures against.
inline long BaseFocalForFov(float fovDegrees) {
    return std::lround(FocalFromFovDegrees(fovDegrees));
}

// Everything about this frame's field of view, worked out once so the pose, the
// projection the cursor is placed with and the log line cannot disagree.
//
// `focal` is read live off the camera every frame. `baseFocal` is the un-zoomed
// one it is measured against - the game's own 310, or whatever the player set,
// which the game is by then treating as its own.
struct ZoomBasis {
    float focal;
    float fov;       // vertical degrees, what the frame is drawn at
    float baseFocal;
    float baseFov;   // vertical degrees, the un-zoomed reference
    float factor;
    // False when the focal could not be read and the factor is the pass-through
    // 1.0 rather than a measurement. An unreadable field of view means no
    // compensation and one log line, never a guessed one, and the caller cannot
    // tell the two apart from the factor alone - an honest 1.0 is what ordinary
    // play looks like.
    bool usable;
};

// The factor is the rendered field of view against the un-zoomed one, so it is
// exactly 1.0 in ordinary play whether or not a field of view has been chosen:
// choosing one moves the reference with it, because that IS the player's
// un-zoomed view. What it still cancels is the game's own zoom - a drawn bow,
// which adds up to 177.5 to the focal, and Magic Sight.
inline ZoomBasis MakeZoomBasis(float focal, float baseFocal) {
    ZoomBasis z;
    z.focal = focal;
    z.fov = FocalToFovDegrees(focal);
    z.baseFocal = baseFocal;
    z.baseFov = FocalToFovDegrees(baseFocal);

    // `focal` is read live off the camera, so a frame the game has not finished
    // setting one up hands back a zero. The focal map answers zero with 168.5
    // degrees, whose half-tangent is thirteen times the base's, and that factor
    // would multiply the head pose by thirteen and throw the view across the
    // room. An unreadable field of view means no compensation, not a guessed
    // one, so the factor stays at 1.0 and the pose passes through as it is.
    const bool readable = std::isfinite(focal) && focal > 0.0f &&
                          std::isfinite(baseFocal) && baseFocal > 0.0f;
    const float tanNow = TanHalfFov(z.fov);
    const float tanBase = TanHalfFov(z.baseFov);
    z.usable = readable && tanNow > 0.0f && tanBase > 0.0f;
    z.factor = z.usable ? cameraunlock::camera::FovZoomFactor(tanNow, tanBase) : 1.0f;
    return z;
}

// The pixel scale the engine's projection divides by depth.
// EERIE_CreateMatriceProj ends up with ProjectionMatrix._11 == ._22 ==
// (height/2) * cot(fov/2) - the horizontal term picks up height/width and then
// width/2, which cancel - so one number covers both axes.
inline float ProjectionScale(float focal, int screenHeight) {
    const float t = TanHalfFovFromFocal(focal);
    if (!(t > 0.0f)) return 0.0f;
    return (static_cast<float>(screenHeight) * 0.5f) / t;
}

}  // namespace ArxHeadTracking
