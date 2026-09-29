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

// The level and object geometry the lean clamp sweeps against. The layouts are
// the 1.21 source's, and every offset below is the one CheckAnythingInSphere
// (0x00429BD0 in both builds) reads: the background grid, the polygon type mask
// and vertices, the zone list of nearby objects, their flags, position and
// mesh, and the mesh's faces and world-space vertices.
#pragma pack(push, 1)
struct D3dTlVertex {
    float sx, sy, sz, rhw;
    uint32_t color, specular;
    float tu, tv;
};

// EERIEPOLY. For level polygons sx/sy/sz of v[] are world coordinates.
struct EeriePoly {
    int32_t type;
    Eerie3D min;
    Eerie3D max;
    Eerie3D norm;
    Eerie3D norm2;
    D3dTlVertex v[4];
    uint8_t rest[0x18C - 0xB4];
};

// FAST_BKG_DATA, one 100x100 unit cell of the level grid.
struct FastBkgData {
    int8_t treat;
    int8_t nothing;
    int16_t nbpoly;
    int16_t nbianchors;
    int16_t nbpolyin;
    int32_t flags;
    float frustrumMinY;
    float frustrumMaxY;
    EeriePoly* polydata;
    void* polyin;
    void* ianchors;
};

constexpr int kMaxBackgroundCells = 160;

// EERIE_BACKGROUND, up to the fields the grid walk needs.
struct EerieBackground {
    FastBkgData fastdata[kMaxBackgroundCells][kMaxBackgroundCells];
    int32_t exist;
    int16_t xsize;
    int16_t zsize;
    int16_t xdiv;
    int16_t zdiv;
    float xmul;
    float zmul;
};

// EERIE_VERTEX. `v` is the world position of the current animation frame.
struct EerieVertex {
    D3dTlVertex vert;
    Eerie3D v;
    Eerie3D norm;
    Eerie3D vworld;
};

// EERIE_FACE, up to its vertex indices.
struct EerieFace {
    int32_t facetype;
    int16_t texid;
    uint16_t vid[3];
    uint8_t rest[0x74 - 12];
};

// EERIE_3DOBJ, the fields read.
struct Eerie3DObj {
    uint8_t pad0[0x22C];
    int32_t nbvertex;
    int32_t trueNbvertex;
    int32_t nbfaces;
    uint8_t pad1[0x258 - 0x238];
    EerieVertex* vertexlist3;
    EerieFace* facelist;
};

// INTERACTIVE_OBJ, the leading fields read.
struct InteractiveObj {
    uint32_t ioflags;
    Eerie3D lastpos;
    Eerie3D pos;
    uint8_t pad[0xA0 - 0x1C];
    Eerie3DObj* obj;
};

// TREATZONE_IO, one entry of the list of objects near the player.
struct TreatzoneIo {
    int32_t num;
    InteractiveObj* io;
    int32_t ioflags;
    int32_t show;
};
#pragma pack(pop)

static_assert(sizeof(D3dTlVertex) == 0x20, "D3DTLVERTEX");
static_assert(offsetof(EeriePoly, v) == 0x34, "EERIEPOLY.v");
static_assert(sizeof(EeriePoly) == 0x18C, "EERIEPOLY");
static_assert(offsetof(FastBkgData, nbpoly) == 2, "FAST_BKG_DATA.nbpoly");
static_assert(offsetof(FastBkgData, polydata) == 0x14, "FAST_BKG_DATA.polydata");
static_assert(sizeof(FastBkgData) == 0x20, "FAST_BKG_DATA");
static_assert(offsetof(EerieBackground, xsize) == 0xC8004, "EERIE_BACKGROUND.Xsize");
static_assert(offsetof(EerieBackground, zsize) == 0xC8006, "EERIE_BACKGROUND.Zsize");
static_assert(offsetof(EerieBackground, xmul) == 0xC800C, "EERIE_BACKGROUND.Xmul");
static_assert(offsetof(EerieBackground, zmul) == 0xC8010, "EERIE_BACKGROUND.Zmul");
static_assert(offsetof(EerieVertex, v) == 0x20, "EERIE_VERTEX.v");
static_assert(sizeof(EerieVertex) == 0x44, "EERIE_VERTEX");
static_assert(offsetof(EerieFace, vid) == 6, "EERIE_FACE.vid");
static_assert(sizeof(EerieFace) == 0x74, "EERIE_FACE");
static_assert(offsetof(Eerie3DObj, nbvertex) == 0x22C, "EERIE_3DOBJ.nbvertex");
static_assert(offsetof(Eerie3DObj, nbfaces) == 0x234, "EERIE_3DOBJ.nbfaces");
static_assert(offsetof(Eerie3DObj, vertexlist3) == 0x258, "EERIE_3DOBJ.vertexlist3");
static_assert(offsetof(Eerie3DObj, facelist) == 0x25C, "EERIE_3DOBJ.facelist");
static_assert(offsetof(InteractiveObj, pos) == 0x10, "INTERACTIVE_OBJ.pos");
static_assert(offsetof(InteractiveObj, obj) == 0xA0, "INTERACTIVE_OBJ.obj");
static_assert(sizeof(TreatzoneIo) == 0x10, "TREATZONE_IO");

constexpr int32_t kPolyQuad = 0x40;     // POLY_QUAD: v[3] is used
constexpr int32_t kPolyHide = 0x200;    // POLY_HIDE
// POLY_TRANS | POLY_WATER | POLY_NOCOL, the polygons the game's own collision
// lets the player walk through.
constexpr int32_t kPolyNoCollision = 0x4 | 0x8 | 0x4000;
constexpr uint32_t kIoNpc = 0x8;             // IO_NPC
constexpr uint32_t kIoNoCollisions = 0x200;  // IO_NO_COLLISIONS
constexpr int32_t kShowInScene = 1;          // SHOW_FLAG_IN_SCENE

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
// the shipped render path calls. Neither carries roll: EERIE_TRANSFORM has a
// yaw pair and a pitch pair and no roll term, so roll_hook.cpp rotates the
// projected result about the screen centre, and a caller that wants the rolled
// position applies ViewRoll::ApplyScreen to what this returns.
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
