// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "roll_hook.h"

#include "engine_memory.h"
#include "hook_install.h"
#include "logging.h"
#include "roll_math.h"

#include <windows.h>
#include <intrin.h>

namespace ArxHeadTracking {
namespace {

using ProjectFn = void(__cdecl*)(float*, float*);
using BuildViewFn = long(__cdecl*)(float*, const float*, const float*, const float*);

const BuildProfile* g_profile = nullptr;
ProjectFn g_origProjectVertex = nullptr;
ProjectFn g_origProjectParticle = nullptr;
BuildViewFn g_origBuildView = nullptr;
void* g_origPortalBounds = nullptr;
void* g_origPortalRejected = nullptr;
void* g_origPortalFrustum = nullptr;
void* g_origObjectProjected = nullptr;
ViewRoll g_roll;
float g_degrees = 0.0f;
bool g_active = false;
bool g_diagnostics = false;
unsigned g_vertices = 0;
unsigned g_objects = 0;
unsigned g_portals = 0;
unsigned g_frustums = 0;

bool RollActive() {
    return g_active && ReadPointer(g_profile->addrActiveCam) ==
                           reinterpret_cast<void*>(g_profile->addrSubj);
}

void RollScreen(float* vertex) {
    const auto* cam = reinterpret_cast<const EerieCamera*>(g_profile->addrSubj);
    g_roll.ApplyScreen(vertex[0], vertex[1], cam->transform.xmod, cam->transform.ymod);
}

void __cdecl Detour_ProjectVertex(float* input, float* output) {
    g_origProjectVertex(input, output);
    if (!RollActive()) return;
    RollScreen(output);
    ++g_vertices;
}

void __cdecl Detour_ProjectParticle(float* input, float* output) {
    g_origProjectParticle(input, output);
    if (!RollActive()) return;
    RollScreen(output);
    ++g_vertices;
}

void __cdecl RollPortal(void* polygon) {
    if (!RollActive()) return;
    auto* bytes = static_cast<unsigned char*>(polygon);
    const unsigned count = (*bytes & 0x40) ? 4 : 3;
    for (unsigned i = 0; i < count; ++i) {
        RollScreen(reinterpret_cast<float*>(bytes + 0xb4 + i * 0x20));
    }
    ++g_portals;
}

void __cdecl RollObject(float* vertex) {
    if (!RollActive()) return;
    RollScreen(vertex);
    // Near-plane clipping interpolates these view coordinates and projects them again.
    g_roll.Apply(vertex[14], vertex[15]);
    ++g_objects;
}

long __cdecl Detour_BuildView(float* matrix, const float* eye,
                             const float* target, const float* up) {
    const long result = g_origBuildView(matrix, eye, target, up);
    if (result >= 0 && RollActive() &&
        reinterpret_cast<uintptr_t>(_ReturnAddress()) == g_profile->addrScreenViewReturn) {
        g_roll.ApplyViewMatrix(matrix);
        ++g_frustums;
    }
    return result;
}

// These sites are inside SSE projection loops. Preserve the full FP state as
// well as flags and registers before calling C++, then replay MinHook's stolen
// instructions. EBX retains the unaligned stack for the restore.
#define ROLL_MID_HOOK(name, argument, helper, original) \
    __declspec(naked) void name() {                    \
        __asm pushfd                                  \
        __asm pushad                                  \
        __asm lea eax, argument                       \
        __asm mov ebx, esp                            \
        __asm sub esp, 528                            \
        __asm and esp, -16                            \
        __asm fxsave [esp]                            \
        __asm sub esp, 16                             \
        __asm mov [esp], eax                          \
        __asm call helper                             \
        __asm add esp, 16                             \
        __asm fxrstor [esp]                           \
        __asm mov esp, ebx                            \
        __asm popad                                   \
        __asm popfd                                   \
        __asm jmp dword ptr [original]                 \
    }

ROLL_MID_HOOK(Detour_PortalBounds, [esi], RollPortal, g_origPortalBounds)
ROLL_MID_HOOK(Detour_PortalRejected, [esi], RollPortal, g_origPortalRejected)
ROLL_MID_HOOK(Detour_PortalFrustum, [esi], RollPortal, g_origPortalFrustum)
ROLL_MID_HOOK(Detour_ObjectProjected, [esi + ecx], RollObject, g_origObjectProjected)

#undef ROLL_MID_HOOK

}  // namespace

bool InstallRollHooks(const BuildProfile& profile, bool diagnostics) {
    g_profile = &profile;
    g_diagnostics = diagnostics;
    if (!InstallDetour(reinterpret_cast<void*>(profile.addrProjectVertex),
                       reinterpret_cast<void*>(&Detour_ProjectVertex),
                       reinterpret_cast<void**>(&g_origProjectVertex), "world vertex roll") ||
        !InstallDetour(reinterpret_cast<void*>(profile.addrProjectParticle),
                       reinterpret_cast<void*>(&Detour_ProjectParticle),
                       reinterpret_cast<void**>(&g_origProjectParticle), "particle roll") ||
        !InstallDetour(reinterpret_cast<void*>(profile.addrPortalProjected[0]),
                       reinterpret_cast<void*>(&Detour_PortalBounds),
                       &g_origPortalBounds, "portal bounds roll") ||
        !InstallDetour(reinterpret_cast<void*>(profile.addrPortalProjected[1]),
                       reinterpret_cast<void*>(&Detour_PortalRejected),
                       &g_origPortalRejected, "portal debug roll") ||
        !InstallDetour(reinterpret_cast<void*>(profile.addrPortalProjected[2]),
                       reinterpret_cast<void*>(&Detour_PortalFrustum),
                       &g_origPortalFrustum, "portal frustum roll") ||
        !InstallDetour(reinterpret_cast<void*>(profile.addrObjectProjected),
                       reinterpret_cast<void*>(&Detour_ObjectProjected),
                       &g_origObjectProjected, "object roll") ||
        !InstallDetour(reinterpret_cast<void*>(profile.addrBuildViewMatrix),
                       reinterpret_cast<void*>(&Detour_BuildView),
                       reinterpret_cast<void**>(&g_origBuildView), "screen frustum roll")) {
        return false;
    }
    Log::Line("Roll hooks installed (world projection, objects, portals, screen frustum)");
    return true;
}

void BeginRenderRoll(float degrees) {
    g_roll = ViewRoll(degrees);
    g_degrees = degrees;
    g_active = degrees != 0.0f;
    g_vertices = g_objects = g_portals = g_frustums = 0;
}

void EndRenderRoll() {
    static ULONGLONG lastReport = 0;
    const ULONGLONG now = GetTickCount64();
    if (g_diagnostics && g_active && now - lastReport >= 1000) {
        lastReport = now;
        Log::Line("diag roll=%.2f world_vertices=%u object_vertices=%u portals=%u frustums=%u",
                  g_degrees, g_vertices, g_objects, g_portals, g_frustums);
    }
    g_active = false;
}

}  // namespace ArxHeadTracking
