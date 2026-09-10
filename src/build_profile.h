// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include <cstdint>

namespace ArxHeadTracking {

// arx.exe is linked without DYNAMIC_BASE, so every address in a profile is
// absolute and the image is expected at its link-time base.
constexpr uintptr_t kExpectedImageBase = 0x00400000u;

constexpr const char* kGameExeName = "arx.exe";

// One shipped build of arx.exe, identified by its PE header, and every address
// this mod needs in it.
//
// arx.exe is linked without DYNAMIC_BASE, so the image is always mapped at
// 0x00400000 and these are absolute addresses rather than RVAs. They are still
// stored per build: a repack that moved the image would change SizeOfImage and
// fail the fingerprint before anything was hooked.
struct BuildProfile {
    const char* name;

    uint32_t timeDateStamp;
    uint32_t sizeOfImage;
    uint32_t checkSum;

    // Functions.
    uintptr_t addrSetActiveCamera;   // void SetActiveCamera(EERIE_CAMERA*)
    uintptr_t addrRender;            // HRESULT DANAE::Render(), __thiscall
    uintptr_t addrFlyingOverObject;  // INTERACTIVE_OBJ* FlyingOverObject(EERIE_S2D*, long)
    uintptr_t addrLaunchRay3;        // int EERIELaunchRay3(o, d, hit, poly, flag)
    uintptr_t addrDrawBitmap;        // void EERIEDrawBitmap(dev, x, y, sx, sy, z, tex, colour)

    uintptr_t addrProjectVertex;
    uintptr_t addrProjectParticle;
    uintptr_t addrPortalProjected[3];
    uintptr_t addrObjectProjected;
    uintptr_t addrBuildViewMatrix;
    uintptr_t addrScreenViewReturn;

    // Globals.
    // The base field of view, as a focal. Arx recomputes BASE_FOCAL from it
    // every frame - CURRENT_BASE_FOCAL + FOKMOD + BOW_FOCAL/4 - and eases
    // subj.focal onto the result, so writing it once is how the game itself
    // would have exposed a field of view setting.
    uintptr_t addrCurrentBaseFocal;  // long CURRENT_BASE_FOCAL
    uintptr_t addrSubj;              // EERIE_CAMERA subj
    uintptr_t addrActiveCam;
    uintptr_t addrDanaeMouse;        // EERIE_S2D DANAEMouse
    uintptr_t addrProjectionMatrix;  // EERIEMATRIX ProjectionMatrix
    uintptr_t addrXratio;            // float Xratio, DANAESIZX / 640
    uintptr_t addrPlayerPos;         // player.pos (player.angle is +12)
    uintptr_t addrPlayerInterface;   // short player.Interface
    uintptr_t addrExternalView;      // long EXTERNALVIEW
    uintptr_t addrBlockControls;     // long BLOCK_PLAYER_CONTROLS
    uintptr_t addrCinemascope;       // long CINEMASCOPE
    uintptr_t addrMouseLookOn;       // long TRUE_PLAYER_MOUSELOOK_ON
    uintptr_t addrDanaeSizX;         // long DANAESIZX
    uintptr_t addrDanaeSizY;         // long DANAESIZY
    uintptr_t addrDanaeCenterX;      // long DANAECENTERX
    uintptr_t addrDanaeCenterY;      // long DANAECENTERY
    uintptr_t addrCrosshairTexture;  // TextureContainer* pTCCrossHair
};

// The profile matching the running arx.exe, or nullptr when none does. Logs the
// mismatch direction itself, so a caller only has to stay dormant.
const BuildProfile* MatchRunningProfile();

}  // namespace ArxHeadTracking
