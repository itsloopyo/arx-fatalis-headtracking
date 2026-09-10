// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "build_profile.h"

#include "logging.h"

#include "cameraunlock/memory/pe_fingerprint.h"

#include <windows.h>

namespace ArxHeadTracking {

namespace {

// The Microsoft Store / Game Pass build, from the GDK package
// BethesdaSoftworks.ArxFatalis. Same 1.21 code as the Steam build with 0x20
// bytes inserted into .text below 0x0047C100: everything above that sits 0x20
// higher, everything below is where it was, and no global moved at all (.data
// keeps its virtual address and size, and every address below still appears as
// an operand the same number of times). Each address here was confirmed by
// matching the Steam build's bytes at it rather than by applying the shift.
const BuildProfile kGdkProfile_20210611 = {
    "gdk-win32-20210611",
    0x60C28A56u,  // TimeDateStamp
    0x00702000u,  // SizeOfImage
    0x00000000u,  // CheckSum

    0x0054EC10u,  // SetActiveCamera
    0x0051C960u,  // DANAE::Render
    0x0044A3E0u,  // FlyingOverObject   (below the insertion, unmoved)
    0x00546930u,  // EERIELaunchRay3
    0x0053FF30u,  // EERIEDrawBitmap

    0x0054F4F0u,  // specialEE_RTP
    0x00548CE0u,  // extEE_RTP
    {0x004B2DFFu, 0x004B37DDu, 0x004B3B09u},  // portal vertices projected
    0x00550DAEu,  // object vertex projected, before screen bounds
    0x0056D750u,  // D3DUtil_SetViewMatrix
    0x004BD809u,  // CreateScreenFrustrum's view-matrix call return

    0x006357B0u,  // CURRENT_BASE_FOCAL
    0x008D84F0u,  // subj
    0x00A08BBCu,  // ACTIVECAM
    0x008D84ECu,  // DANAEMouse
    0x00A08BD0u,  // ProjectionMatrix
    0x00635A68u,  // Xratio
    0x00853C48u,  // player.pos
    0x00853CFAu,  // player.Interface
    0x009A47CCu,  // EXTERNALVIEW
    0x00853F90u,  // BLOCK_PLAYER_CONTROLS
    0x007F2DF8u,  // CINEMASCOPE
    0x007F2BA0u,  // TRUE_PLAYER_MOUSELOOK_ON
    0x00635714u,  // DANAESIZX
    0x00635718u,  // DANAESIZY
    0x009A0AF0u,  // DANAECENTERX
    0x009A0AF4u,  // DANAECENTERY
    0x009A0BD4u,  // pTCCrossHair
};

// Steam's Arx Fatalis 1.21. The image has no DYNAMIC_BASE, so these are the
// addresses the process actually runs at.
const BuildProfile kSteamProfile_20200515 = {
    "steam-win32-20200515",
    0x5EBEEFD7u,  // TimeDateStamp
    0x00702000u,  // SizeOfImage
    0x00000000u,  // CheckSum

    0x0054EBF0u,  // SetActiveCamera
    0x0051C940u,  // DANAE::Render
    0x0044A3E0u,  // FlyingOverObject
    0x00546910u,  // EERIELaunchRay3
    0x0053FF10u,  // EERIEDrawBitmap

    0x0054F4D0u,  // specialEE_RTP
    0x00548CC0u,  // extEE_RTP
    {0x004B2DDFu, 0x004B37BDu, 0x004B3AE9u},  // portal vertices projected
    0x00550D8Eu,  // object vertex projected, before screen bounds
    0x0056D730u,  // D3DUtil_SetViewMatrix
    0x004BD7E9u,  // CreateScreenFrustrum's view-matrix call return

    0x006357B0u,  // CURRENT_BASE_FOCAL
    0x008D84F0u,  // subj
    0x00A08BBCu,  // ACTIVECAM
    0x008D84ECu,  // DANAEMouse
    0x00A08BD0u,  // ProjectionMatrix
    0x00635A68u,  // Xratio
    0x00853C48u,  // player.pos
    0x00853CFAu,  // player.Interface
    0x009A47CCu,  // EXTERNALVIEW
    0x00853F90u,  // BLOCK_PLAYER_CONTROLS
    0x007F2DF8u,  // CINEMASCOPE
    0x007F2BA0u,  // TRUE_PLAYER_MOUSELOOK_ON
    0x00635714u,  // DANAESIZX
    0x00635718u,  // DANAESIZY
    0x009A0AF0u,  // DANAECENTERX
    0x009A0AF4u,  // DANAECENTERY
    0x009A0BD4u,  // pTCCrossHair
};

// Newest first: the head of this array is the diagnostic primary, the build the
// "your game is newer/older than this mod knows about" line compares against.
// Append new builds above the old ones and never edit an entry in place - a
// player still on the previous build keeps matching theirs.
const BuildProfile* const kKnownProfiles[] = {
    &kGdkProfile_20210611,
    &kSteamProfile_20200515,
};

}  // namespace

const BuildProfile* MatchRunningProfile() {
    HMODULE exe = GetModuleHandleA(kGameExeName);
    if (!exe) {
        Log::Line("Staying dormant: %s is not loaded in this process.", kGameExeName);
        return nullptr;
    }

    cameraunlock::memory::PeFingerprint running{};
    if (!cameraunlock::memory::ReadPeFingerprint(exe, running)) {
        Log::Line("Staying dormant: could not read the PE header of %s.", kGameExeName);
        return nullptr;
    }

    for (const BuildProfile* profile : kKnownProfiles) {
        const cameraunlock::memory::PeFingerprint reference{
            profile->timeDateStamp, profile->sizeOfImage, profile->checkSum};
        if (running.Matches(reference)) {
            // The fingerprint says the file on disk is the right one; it says
            // nothing about where Windows put it. Exploit Protection's "force
            // randomization for images" relocates an image that never opted
            // into ASLR, without touching a byte of the file - so the
            // fingerprint still matches while every absolute address in the
            // profile points somewhere else, and hooking them crashes the game
            // seconds in under a log that says the mod loaded cleanly.
            const uintptr_t base = reinterpret_cast<uintptr_t>(exe);
            if (base != kExpectedImageBase) {
                Log::Line("Staying dormant: %s matched %s but is mapped at 0x%08X instead of "
                          "0x%08X, so this mod's addresses do not apply. The usual cause is "
                          "\"Force randomization for images\" being on for it in Windows "
                          "Exploit Protection.",
                          kGameExeName, profile->name, static_cast<unsigned>(base),
                          static_cast<unsigned>(kExpectedImageBase));
                return nullptr;
            }
            Log::Line("Build profile: %s", profile->name);
            return profile;
        }
    }

    const BuildProfile* primary = kKnownProfiles[0];
    const cameraunlock::memory::PeFingerprint reference{
        primary->timeDateStamp, primary->sizeOfImage, primary->checkSum};
    switch (cameraunlock::memory::ClassifyMismatch(running, reference)) {
        case cameraunlock::memory::FingerprintMismatch::Newer:
            Log::Line("Staying dormant: this arx.exe is newer than any build this mod knows "
                      "about. Check the releases page for an updated mod.");
            break;
        case cameraunlock::memory::FingerprintMismatch::Older:
            Log::Line("Staying dormant: this arx.exe is older than the builds this mod knows "
                      "about. Let the store finish updating the game.");
            break;
        case cameraunlock::memory::FingerprintMismatch::Differs:
            Log::Line("Staying dormant: this arx.exe has been modified or repacked. The mod "
                      "does not engage on a changed binary.");
            break;
    }
    Log::Line("  running: TimeDateStamp=0x%08X SizeOfImage=0x%08X CheckSum=0x%08X",
              running.TimeDateStamp, running.SizeOfImage, running.CheckSum);
    return nullptr;
}

}  // namespace ArxHeadTracking
