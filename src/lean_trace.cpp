// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "lean_trace.h"

#include "arx_game.h"
#include "engine_memory.h"
#include "sphere_sweep.h"

#include <windows.h>

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace ArxHeadTracking {

namespace {

using cameraunlock::math::Vec3;

// int EERIELaunchRay3(EERIE_3D* orgn, EERIE_3D* dest, EERIE_3D* hit, EERIEPOLY* ep, long flag)
//
// Walks the background grid from orgn toward dest in 1.5-unit steps. Returns
// non-zero when level geometry stopped it, and writes the point it reached into
// `hit` either way - so the caller gets a surface point on a block and the far
// end of the segment on a clear path. This is the same cast the game runs for
// its own light occlusion and its arrows.
using LaunchRay3Fn = int(__cdecl*)(const Eerie3D*, const Eerie3D*, Eerie3D*, void*, long);

const BuildProfile* g_profile = nullptr;
LaunchRay3Fn g_launchRay3 = nullptr;

// The sweep's radius: how far the eye is held off anything, in Arx units.
float g_radius = 0.0f;

// Objects whose origin is further than this from the eye are skipped before
// their faces are read. The same bound CheckAnythingInCylinder puts on the
// objects it tests against the player.
constexpr float kObjectReach = 1000.0f;

LeanTraceStats g_stats;

struct Sweep {
    Vec3 origin;
    Vec3 dir;
    float length;
    float radius;
    Vec3 boxMin;
    Vec3 boxMax;
    // The nearest contact so far, which also shortens the sweep for every
    // triangle after it.
    float hit;
    bool blocked;
};

Vec3 ToVec(const Eerie3D& v) { return Vec3(v.x, v.y, v.z); }
Vec3 ToVec(const D3dTlVertex& v) { return Vec3(v.sx, v.sy, v.sz); }

bool BoxesOverlap(const Sweep& s, const Vec3& lo, const Vec3& hi) {
    return lo.x <= s.boxMax.x && hi.x >= s.boxMin.x && lo.y <= s.boxMax.y &&
           hi.y >= s.boxMin.y && lo.z <= s.boxMax.z && hi.z >= s.boxMin.z;
}

bool SweepTriangle(Sweep& s, const Vec3& a, const Vec3& b, const Vec3& c) {
    ++g_stats.triangles;
    float t = 0.0f;
    if (!SweepSphereTriangle(s.origin, s.dir, s.hit, s.radius, a, b, c, t)) return false;
    s.hit = t;
    s.blocked = true;
    return true;
}

// The level polygons in the grid cells the swept sphere passes over. The same
// polygons, with the same type mask, that the game's own sphere and cylinder
// collision uses, so the eye stops at what stops the player.
void SweepLevel(Sweep& s, const EerieBackground& bkg) {
    const auto cell = [](float coord, float mul, int size) {
        const int i = static_cast<int>(std::floor(coord * mul));
        return std::clamp(i, 0, std::min(size, kMaxBackgroundCells) - 1);
    };
    const int x0 = cell(s.boxMin.x, bkg.xmul, bkg.xsize);
    const int x1 = cell(s.boxMax.x, bkg.xmul, bkg.xsize);
    const int z0 = cell(s.boxMin.z, bkg.zmul, bkg.zsize);
    const int z1 = cell(s.boxMax.z, bkg.zmul, bkg.zsize);

    for (int x = x0; x <= x1; ++x) {
        for (int z = z0; z <= z1; ++z) {
            const FastBkgData& data = bkg.fastdata[x][z];
            for (int k = 0; k < data.nbpoly; ++k) {
                const EeriePoly& poly = data.polydata[k];
                ++g_stats.polygons;
                if (poly.type & kPolyNoCollision) continue;
                if (!BoxesOverlap(s, ToVec(poly.min), ToVec(poly.max))) continue;
                const Vec3 v0 = ToVec(poly.v[0]);
                const Vec3 v1 = ToVec(poly.v[1]);
                const Vec3 v2 = ToVec(poly.v[2]);
                SweepTriangle(s, v0, v1, v2);
                // The renderer draws a quad as the strip (0,1,2), (3,2,1).
                if (poly.type & kPolyQuad) SweepTriangle(s, v1, ToVec(poly.v[3]), v2);
            }
        }
    }
}

// The meshes of the objects near the player - doors, chests, furniture, NPCs,
// items - in their current animation frame. The level grid does not hold them.
//
// The player's own object is skipped, and so is an object the game lets the
// player walk through, which is its own rule for IO_NO_COLLISIONS on anything
// but an NPC.
bool SweepObjects(Sweep& s) {
    const int32_t count = ReadLong(g_profile->addrTreatZoneCount);
    const auto* zone = static_cast<const TreatzoneIo*>(ReadPointer(g_profile->addrTreatZone));
    if (zone == nullptr) return false;

    const float reach = kObjectReach + s.length;
    bool hit = false;
    for (int32_t i = 0; i < count; ++i) {
        const TreatzoneIo& entry = zone[i];
        if (entry.show != kShowInScene || entry.io == nullptr || entry.num == 0) continue;
        const InteractiveObj& io = *entry.io;
        if (io.obj == nullptr) continue;
        if (!(io.ioflags & kIoNpc) && (io.ioflags & kIoNoCollisions)) continue;
        if ((ToVec(io.pos) - s.origin).SqrMagnitude() > reach * reach) continue;

        const Eerie3DObj& mesh = *io.obj;
        ++g_stats.objects;
        // The vertices are read in order, which is far cheaper than the face
        // loop's indexed reads, and most objects in the zone are nowhere near
        // the lean.
        Vec3 lo(FLT_MAX, FLT_MAX, FLT_MAX);
        Vec3 hi(-FLT_MAX, -FLT_MAX, -FLT_MAX);
        for (int32_t v = 0; v < mesh.nbvertex; ++v) {
            const Eerie3D& p = mesh.vertexlist3[v].v;
            lo = Vec3(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
            hi = Vec3(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
        }
        if (!BoxesOverlap(s, lo, hi)) continue;
        g_stats.faces += static_cast<unsigned>(mesh.nbfaces);
        for (int32_t f = 0; f < mesh.nbfaces; ++f) {
            const EerieFace& face = mesh.facelist[f];
            if (face.facetype & (kPolyHide | kPolyNoCollision)) continue;
            const Vec3 a = ToVec(mesh.vertexlist3[face.vid[0]].v);
            const Vec3 b = ToVec(mesh.vertexlist3[face.vid[1]].v);
            const Vec3 c = ToVec(mesh.vertexlist3[face.vid[2]].v);
            const Vec3 faceLo(std::min({a.x, b.x, c.x}), std::min({a.y, b.y, c.y}),
                              std::min({a.z, b.z, c.z}));
            const Vec3 faceHi(std::max({a.x, b.x, c.x}), std::max({a.y, b.y, c.y}),
                              std::max({a.z, b.z, c.z}));
            if (!BoxesOverlap(s, faceLo, faceHi)) continue;
            hit |= SweepTriangle(s, a, b, c);
        }
    }
    return hit;
}

}  // namespace

void InitLeanTrace(const BuildProfile& profile, float radius) {
    g_profile = &profile;
    g_launchRay3 = reinterpret_cast<LaunchRay3Fn>(profile.addrLaunchRay3);
    g_radius = radius;
}

const LeanTraceStats& LastLeanTrace() {
    return g_stats;
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

cameraunlock::camera::LeanObstruction LeanQuery(void* /*context*/, const Vec3& start,
                                                const Vec3& direction, float maxDistance) {
    cameraunlock::camera::LeanObstruction out;
    g_stats = LeanTraceStats{};
    if (!g_profile) return out;
    const auto* bkg = static_cast<const EerieBackground*>(ReadPointer(g_profile->addrActiveBkg));
    if (bkg == nullptr) return out;

    LARGE_INTEGER begin;
    QueryPerformanceCounter(&begin);

    Sweep s;
    s.origin = start;
    s.dir = direction;
    s.length = maxDistance;
    s.radius = g_radius;
    s.hit = maxDistance;
    s.blocked = false;
    const Vec3 end = start + direction * maxDistance;
    s.boxMin = Vec3(std::min(start.x, end.x) - g_radius, std::min(start.y, end.y) - g_radius,
                    std::min(start.z, end.z) - g_radius);
    s.boxMax = Vec3(std::max(start.x, end.x) + g_radius, std::max(start.y, end.y) + g_radius,
                    std::max(start.z, end.z) + g_radius);

    SweepLevel(s, *bkg);
    LARGE_INTEGER afterLevel;
    QueryPerformanceCounter(&afterLevel);
    // The level contact has already shortened the sweep, so an object only
    // reports a hit when it is nearer.
    g_stats.object = SweepObjects(s);

    LARGE_INTEGER finish, frequency;
    QueryPerformanceCounter(&finish);
    QueryPerformanceFrequency(&frequency);
    g_stats.microseconds =
        static_cast<float>(finish.QuadPart - begin.QuadPart) * 1.0e6f /
        static_cast<float>(frequency.QuadPart);
    g_stats.levelMicroseconds =
        static_cast<float>(afterLevel.QuadPart - begin.QuadPart) * 1.0e6f /
        static_cast<float>(frequency.QuadPart);
    g_stats.queried = true;
    g_stats.blocked = s.blocked;
    g_stats.distance = s.hit;

    out.queried = true;
    out.blocked = s.blocked;
    out.distance = s.hit;
    return out;
}

}  // namespace ArxHeadTracking
