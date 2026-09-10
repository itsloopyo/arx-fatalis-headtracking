// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>

// The engine types and conventions this mod reaches into.
//
// Arx Fatalis renders on the CPU: every vertex is transformed and projected from
// the active EERIE_CAMERA's precomputed sines, cosines and position, so there is
// no view matrix anywhere to hook. Moving the view means moving that struct, and
// drawing anything at a world point means running the same projection the engine
// runs.
//
// The struct offsets, the rotation order and the sign of each angle were read
// off the Arx Fatalis source Arkane published in January 2011 (the
// ArxFatalis-1.21 drop, which is what this executable was built from) and
// confirmed against the shipped binary. THIRD-PARTY-NOTICES.md records that.
namespace ArxHeadTracking {

// Arx measures the world in centimetres: the player's eye sits 170 units above
// their feet (PLAYER_BASE_HEIGHT = -170, y growing downward).
constexpr float kUnitsPerMetre = 100.0f;

// ARX_INTERFACE_FLAG bits of player.Interface.
//
// INTER_COMBATMODE is "weapon drawn". The game draws no crosshair at all while
// it is set, though it still pins the cursor to screen centre in free-look.
constexpr int16_t kInterfaceCombatMode = 0x0040;

// The bits that mean a full-screen panel is up and the player is reading rather
// than playing: the book/map (INTER_MAP), the whole inventory page
// (INTER_INVENTORYALL), a note (INTER_NOTE) and the steal panel (INTER_STEAL).
// INTER_INVENTORY is deliberately absent - that is the small belt strip, which
// is on during ordinary play.
constexpr int16_t kInterfaceBlockingFlags = 0x0001 | 0x0004 | 0x0080 | 0x0100;

// Byte offset of player.angle (a, b, g) from player.pos, which is where the
// build profile points.
constexpr uintptr_t kPlayerAngleOffset = 12;

// EERIE_CAMERA, 360 bytes. Only the fields this mod touches are named; the rest
// is padding by offset so the layout stays exact and a stray write cannot land
// in the wrong member.
#pragma pack(push, 1)
struct EerieTransform {
    float posx, posy, posz;      // 0
    float ycos, ysin;            // 12
    float xsin, xcos;            // 20
    float use_focal;             // 28
    float xmod, ymod, zmod;      // 32
};

struct EerieCamera {
    EerieTransform transform;    // 0
    float pos_x, pos_y, pos_z;   // 44
    float Ycos, Ysin;            // 56
    float Xcos, Xsin;            // 64
    float Zcos, Zsin;            // 72
    float focal;                 // 80
    float use_focal;             // 84
    float Zmul;                  // 88
    float posleft, postop;       // 92
    float xmod, ymod;            // 100
    float matrix[16];            // 108
    float angle_a, angle_b, angle_g;  // 172: pitch, yaw, roll (degrees)
    float d_pos[3];              // 184
    float d_angle[3];            // 196
    float lasttarget[3];         // 208
    float lastpos[3];            // 220
    float translatetarget[3];    // 232
    int32_t lastinfovalid;       // 244
    float norm[3];               // 248
    float fadecolor[3];          // 260
    int32_t clip_left, clip_top, clip_right, clip_bottom;  // 272
    float clipz0, clipz1;        // 288
    int32_t centerx, centery;    // 296
    float smoothing;             // 304
    float AddX, AddY;            // 308
    int32_t Xsnap, Zsnap;        // 316
    float Zdiv;                  // 324
    int32_t clip3D;              // 328
    int32_t type;                // 332
    int32_t bkgcolor;            // 336
    int32_t nbdrawn;             // 340
    float cdepth;                // 344
    float size[3];               // 348
};
#pragma pack(pop)

static_assert(sizeof(EerieCamera) == 360, "EERIE_CAMERA must be 360 bytes");
static_assert(offsetof(EerieCamera, pos_x) == 44, "pos");
static_assert(offsetof(EerieCamera, angle_a) == 172, "angle");
static_assert(offsetof(EerieCamera, cdepth) == 344, "cdepth");

#pragma pack(push, 1)
// EERIE_S2D: the screen position the game picks and draws the cursor at.
struct EerieS2D {
    int16_t x;
    int16_t y;
};
#pragma pack(pop)

// The widest coordinate EERIE_S2D can hold. Converting a float outside a
// destination integer's range is undefined, so this is a hard bound rather than
// a tidy one.
constexpr float kMaxCursorCoordinate = 32767.0f;

// One axis of a projected screen position, bounded into the frame.
//
// `extent` is DANAESIZX or DANAESIZY, read out of the game. It is zero until the
// engine has sized its window, which is why the fallback bound exists: without
// one the position would be left unbounded on exactly the frames the projection
// can hand back a coordinate in the hundreds of thousands, and the conversion
// into EERIE_S2D below it is undefined there.
inline float ClampCursorAxis(float value, int extent) {
    float upper = kMaxCursorCoordinate;
    if (extent > 1 && static_cast<float>(extent - 1) < upper) {
        upper = static_cast<float>(extent - 1);
    }
    if (value < 0.0f) return 0.0f;
    if (value > upper) return upper;
    return value;
}

// Bounds a projected screen position into the frame. False when there is no
// position to write at all, which leaves the cursor where the game put it - a
// non-finite coordinate passes every comparison a clamp can make, so it has to
// be refused rather than clamped.
inline bool ClampToScreenPoint(float& x, float& y, int width, int height) {
    if (!std::isfinite(x) || !std::isfinite(y)) return false;
    x = ClampCursorAxis(x, width);
    y = ClampCursorAxis(y, height);
    return true;
}

// EERIE_3D, used by the engine's ray cast.
struct Eerie3D {
    float x, y, z;
};

// Degrees, the unit every EERIE_CAMERA angle is in.
constexpr float kDegToRad = 3.14159265358979323846f / 180.0f;

// Arx maps its "focal" to a VERTICAL field of view in degrees with this
// piecewise line, and PrepareCamera feeds the result straight to
// EERIE_CreateMatriceProj. Reproduced rather than read out of the binary
// because the mod needs the FOV of a focal the game is NOT currently rendering
// - the un-zoomed reference, and the one a chosen field of view asks for -
// which no engine global holds.
//
// The constants were read out of the function at 0x005487A0 and agree with
// EERIE_TransformOldFocalToNewFocal in the published source. The last two lines
// matter even though the game itself never asks for a focal above 487.5: a
// chosen field of view does, and a map that ran the 76.5 line on past 700 would
// have the mod projecting through one field of view while the engine rendered
// another.
inline float FocalToFovDegrees(float focal) {
    if (focal < 200.0f) return 168.5f - 0.34f * focal;
    if (focal < 300.0f) return 150.5f - 0.25f * focal;
    if (focal < 400.0f) return 124.0f - 0.155f * focal;
    if (focal < 500.0f) return 106.0f - 0.11f * focal;
    if (focal < 600.0f) return 88.5f - 0.075f * focal;
    if (focal < 700.0f) return 76.5f - 0.055f * focal;
    if (focal < 800.0f) return 69.5f - 0.045f * focal;
    return 33.5f;
}

// The focal that renders a given vertical field of view: the map above run
// backwards, each line inverted over the FOV range its own focal range covers,
// walked widest first.
//
// It is not quite one to one. The engine steps two degrees at focal 300 - the
// 200-to-300 line ends at 75.5 and the 300-to-400 line starts at 77.5 - so the
// band between them is reachable two ways, and this takes the 300-to-400
// branch, the one holding the game's own un-zoomed 310. Below 33.5 degrees
// there is no focal at all, the last line being flat; kMinFieldOfView keeps the
// config clear of that.
inline float FocalFromFovDegrees(float fovDegrees) {
    if (fovDegrees > 100.5f) return (168.5f - fovDegrees) / 0.34f;
    if (fovDegrees > 77.5f) return (150.5f - fovDegrees) / 0.25f;
    if (fovDegrees > 62.0f) return (124.0f - fovDegrees) / 0.155f;
    if (fovDegrees > 51.0f) return (106.0f - fovDegrees) / 0.11f;
    if (fovDegrees > 43.5f) return (88.5f - fovDegrees) / 0.075f;
    if (fovDegrees > 38.0f) return (76.5f - fovDegrees) / 0.055f;
    return (69.5f - fovDegrees) / 0.045f;
}

// The focal the game renders at when nothing is zoomed. BASE_FOCAL is
// CURRENT_BASE_FOCAL + FOKMOD + BOW_FOCAL/4, and neither of the first two is
// ever written after its initialiser, so 310 is the whole of the un-zoomed
// case and the zoom factor is exactly 1.0 in ordinary play.
constexpr float kReferenceFocal = 310.0f;

// The camera basis Arx projects through, as world direction vectors, taken from
// the renderer's rotation order (yaw about y, then pitch about x). y points DOWN
// in this world.
//
// Only the yaw is used: the tracker measures head position in the room, so the
// lean belongs on the player's body frame and must not tip with where they are
// looking.
struct HorizonBasis {
    Eerie3D right;
    Eerie3D down;
    Eerie3D forward;
};

inline HorizonBasis MakeHorizonBasis(float yawDegrees) {
    const float b = yawDegrees * kDegToRad;
    const float s = std::sin(b);
    const float c = std::cos(b);
    HorizonBasis out;
    out.right = { c, 0.0f, s };
    out.down = { 0.0f, 1.0f, 0.0f };
    out.forward = { -s, 0.0f, c };
    return out;
}

// The world-to-screen transform the game's own renderer performs, in one place.
//
// This mirrors `specialEE_RTP2` - the function every level polygon and every
// animated object goes through - and NOT the older `EE_RTP`, which nothing in
// the shipped render path calls. The difference is the whole reason roll is
// absent from this mod: the transform those two use reads its terms out of
// EERIE_TRANSFORM, which carries a yaw pair and a pitch pair and no roll term at
// all. Rolling the camera moves the handful of things still projected through
// EE_RTP and leaves the world upright, which is worse than not rolling.
//
// Returns false when the point is at or behind the eye plane, where there is no
// screen position to give.
struct ScreenPoint {
    float x;
    float y;
    float depth;
};

// `projScale` covers both axes: EERIE_CreateMatriceProj ends up with
// ProjectionMatrix._11 == ._22, so the projection is isotropic in pixels and one
// number is the whole of it. See ProjectionScale in engine_boundary.h.
inline bool ProjectWorldPoint(const EerieCamera& cam, float projScale, const Eerie3D& world,
                              ScreenPoint& out) {
    const float dx = world.x - cam.pos_x;
    const float dy = world.y - cam.pos_y;
    const float dz = world.z - cam.pos_z;

    const float temp = dz * cam.Ycos - dx * cam.Ysin;
    const float vx = dx * cam.Ycos + dz * cam.Ysin;

    const float vz = dy * cam.Xsin + temp * cam.Xcos;
    const float vy = dy * cam.Xcos - temp * cam.Xsin;

    if (!(vz > 1.0f)) return false;

    const float inv = 1.0f / vz;
    out.x = vx * projScale * inv + cam.posleft;
    out.y = vy * projScale * inv + cam.postop;
    out.depth = vz;
    return true;
}

// The world direction the camera faces, from its angles. Matches the vector the
// game builds for its own sound listener and LOD tests.
inline Eerie3D ForwardFromAngles(float pitchDegrees, float yawDegrees) {
    const float a = pitchDegrees * kDegToRad;
    const float b = yawDegrees * kDegToRad;
    const float ca = std::cos(a);
    return { -std::sin(b) * ca, std::sin(a), std::cos(b) * ca };
}

}  // namespace ArxHeadTracking
