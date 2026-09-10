// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "cursor_hook.h"

#include "aim_projection.h"
#include "arx_game.h"
#include "engine_memory.h"
#include "hook_install.h"
#include "logging.h"

#include <cstdint>

namespace ArxHeadTracking {

namespace {

// INTERACTIVE_OBJ* FlyingOverObject(EERIE_S2D* pos, long flag)
using FlyingOverObjectFn = void*(__cdecl*)(EerieS2D*, long);
// void EERIEDrawBitmap(LPDIRECT3DDEVICE7, float x, float y, float sx, float sy,
//                      float z, TextureContainer*, D3DCOLOR)
using DrawBitmapFn = void(__cdecl*)(void*, float, float, float, float, float, void*, uint32_t);

// Largest sprite that can plausibly be a crosshair, in interface pixels. The
// shipped one is 16x16.
constexpr float kMaxCrosshairSprite = 64.0f;

const BuildProfile* g_profile = nullptr;
bool g_enabled = false;
FlyingOverObjectFn g_origFlyingOverObject = nullptr;
DrawBitmapFn g_origDrawBitmap = nullptr;

bool g_moved = false;
EerieS2D g_saved{0, 0};

EerieS2D* Mouse() { return reinterpret_cast<EerieS2D*>(g_profile->addrDanaeMouse); }

bool WeaponDrawn() {
    return (ReadShort(g_profile->addrPlayerInterface) & kInterfaceCombatMode) != 0;
}

bool FreeLookOn() { return ReadLong(g_profile->addrMouseLookOn) != 0; }

// The two engine conditions this file cares about are NOT the same condition,
// and collapsing them into one predicate gets one of the two wrong whichever
// way it is written.
//
// This is the pin: the state where the game holds DANAEMouse at screen centre,
// so the position it picks with is not the position the player is looking at.
// Two places pin it, under different conditions. ManageKeyMouse's pin sits
// inside a combat-mode exclusion, so a drawn weapon releases that one;
// ManageEditorControls' does not, and it runs later in the frame, which is what
// keeps the cursor pinned with a weapon out. Both are additionally gated on the
// game's own bMouseLookToggle and bAutoReadyWeapon options, which live in a
// config struct this mod does not have an address for - so free-look is as
// close as this can get, and it is wider than the truth by exactly those two
// options. See the note in cursor_hook.h.
bool CursorPinnedToCentre() {
    return FreeLookOn();
}

// The crosshair SPRITE is drawn under a narrower condition:
// ARX_INTERFACE_RenderCursor requires `TRUE_PLAYER_MOUSELOOK_ON &&
// bShowCrossHair && !(player.Interface & INTER_COMBATMODE)`, so with a weapon
// out there is no crosshair on screen at all.
bool CrosshairDrawn() {
    return FreeLookOn() && !WeaponDrawn();
}

// False when the aim point has no screen position that can be written into the
// game's 16-bit cursor, which leaves the cursor where the game put it.
bool ClampToScreen(float& x, float& y) {
    return ClampToScreenPoint(x, y, ReadLong(g_profile->addrDanaeSizX),
                              ReadLong(g_profile->addrDanaeSizY));
}

// The pick runs against the bounding boxes the LAST render projected, so the aim
// position cached from that same render is exactly the right one to test with. A
// frame later the camera hook writes the current frame's position over it, and
// that is the one the cursor is drawn at.
//
// Only DANAEMouse. Arx has a second pick site that passes a stack-local copy of
// MemoMouse - the position the pointer was at when free-look was entered - and
// that one is deliberately left alone: the engine never pinned it to centre, so
// moving it would change WHICH object the game picks with the head sitting
// still, which is the mod altering game logic rather than compensating for it.
void* __cdecl Detour_FlyingOverObject(EerieS2D* pos, long flag) {
    if (pos == Mouse()) {
        PlaceCursorOnAimPoint();
    }
    return g_origFlyingOverObject(pos, flag);
}

// True for the one bitmap Arx pins to the middle of the screen: the crosshair.
//
// Its draw site is the only place that computes screen centre minus half the
// sprite, so that arithmetic IS the signature. Matching on it rather than on the
// texture alone matters because the site falls back to the round target sprite
// when cruz.bmp is missing from the game's data, and that same texture is used
// as an ordinary cursor elsewhere - which is drawn from DANAEMouse and has
// already been moved by the time it gets here.
bool IsCentredCrosshair(float x, float y, float sx, float sy, void* texture) {
    // The game only draws it in free-look and never with a weapon out, so those
    // are the frames to look on. Everything below is cheap, but this keeps the
    // test off every other frame entirely.
    if (!CrosshairDrawn()) return false;

    void* const crosshair = ReadPointer(g_profile->addrCrosshairTexture);
    if (crosshair != nullptr && texture == crosshair) return true;

    // cruz.bmp is absent from the shipped data, so pTCCrossHair is null and the
    // draw site falls back to the round target sprite - which is also an ordinary
    // cursor elsewhere. What separates them is the arithmetic: only the crosshair
    // is placed at screen centre minus half the sprite. The size bound keeps a
    // full-screen overlay, which satisfies that same equation at (0,0), out.
    if (sx > kMaxCrosshairSprite || sy > kMaxCrosshairSprite) return false;
    const float cx = static_cast<float>(ReadLong(g_profile->addrDanaeSizX)) * 0.5f - sx * 0.5f;
    const float cy = static_cast<float>(ReadLong(g_profile->addrDanaeSizY)) * 0.5f - sy * 0.5f;
    return x == cx && y == cy;
}

// The crosshair is the one thing in Arx that does NOT follow DANAEMouse, so it
// needs its own move. Everything else the game draws through here is passed
// straight along.
void __cdecl Detour_DrawBitmap(void* device, float x, float y, float sx, float sy, float z,
                               void* texture, uint32_t colour) {
    if (g_enabled && texture != nullptr && IsCentredCrosshair(x, y, sx, sy, texture)) {
        static bool s_reported = false;
        if (!s_reported) {
            s_reported = true;
            Log::Line("Crosshair found: sprite %p at %.1fx%.1f. It now follows the aim point.",
                      texture, sx, sy);
        }
        float ax = 0.0f, ay = 0.0f;
        if (GetAimScreenPosition(ax, ay) && ClampToScreen(ax, ay)) {
            // The engine passes the sprite's top-left corner, so put its centre
            // on the aim point rather than its corner.
            x = ax - sx * 0.5f;
            y = ay - sy * 0.5f;
        }
    }
    g_origDrawBitmap(device, x, y, sx, sy, z, texture, colour);
}

}  // namespace

void PlaceCursorOnAimPoint() {
    if (!g_enabled || !CursorPinnedToCentre()) return;

    float x = 0.0f, y = 0.0f;
    if (!GetAimScreenPosition(x, y)) return;
    if (!ClampToScreen(x, y)) return;

    EerieS2D* mouse = Mouse();
    if (!g_moved) {
        g_saved = *mouse;
        g_moved = true;
    }
    mouse->x = static_cast<int16_t>(x);
    mouse->y = static_cast<int16_t>(y);
}

void RestoreCursor() {
    if (!g_moved) return;
    *Mouse() = g_saved;
    g_moved = false;
}

bool InstallCursorHook(const BuildProfile& profile, const Config& cfg) {
    g_profile = &profile;
    g_enabled = cfg.move_crosshair;
    if (!g_enabled) {
        Log::Line("MoveCrosshair is off: the cursor and crosshair stay where the game puts "
                  "them.");
        return true;
    }

    bool ok = true;
    if (!InstallDetour(reinterpret_cast<void*>(profile.addrFlyingOverObject),
                       reinterpret_cast<void*>(&Detour_FlyingOverObject),
                       reinterpret_cast<void**>(&g_origFlyingOverObject), "FlyingOverObject")) {
        Log::Line("  the cursor will be drawn on the aim point but will still pick up "
                  "whatever the head is facing.");
        ok = false;
    }
    if (!InstallDetour(reinterpret_cast<void*>(profile.addrDrawBitmap),
                       reinterpret_cast<void*>(&Detour_DrawBitmap),
                       reinterpret_cast<void**>(&g_origDrawBitmap), "EERIEDrawBitmap")) {
        Log::Line("  the crosshair will stay pinned to the centre of the screen.");
        ok = false;
    }

    Log::Line("Cursor hooks installed (FlyingOverObject=0x%08X, EERIEDrawBitmap=0x%08X, "
              "crosshair sprite=%p)",
              static_cast<unsigned>(profile.addrFlyingOverObject),
              static_cast<unsigned>(profile.addrDrawBitmap),
              ReadPointer(profile.addrCrosshairTexture));
    return ok;
}

}  // namespace ArxHeadTracking
