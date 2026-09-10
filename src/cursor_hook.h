// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "build_profile.h"
#include "config.h"

namespace ArxHeadTracking {

// Moves the game's own cursor onto the point the player is aiming at.
//
// Arx has no separate crosshair sprite to chase: in mouselook it pins DANAEMouse
// to the screen centre, picks whatever is under it, and later draws the cursor
// there. Writing the compensated position into DANAEMouse therefore moves the
// mark and what it selects together, which is the whole point - a crosshair that
// sits on the shot but picks up whatever the head happens to face would be worse
// than not moving it at all.
bool InstallCursorHook(const BuildProfile& profile, const Config& cfg);

// Puts DANAEMouse on the aim point, if the game currently has it pinned to the
// screen centre.
//
// Called twice per frame from two places, for two reasons. The camera hook calls
// it once this frame's aim point is known, so what is DRAWN matches the frame it
// is drawn over. The FlyingOverObject detour calls it far earlier in the frame,
// where the only aim point available is the previous frame's - which is the
// right one there, because the pick tests against bounding boxes that same frame
// projected.
//
// Known gap: the game also picks through a stack-local copy of MemoMouse when
// its "Mouse Look Toggle" option is off or "Auto Ready Weapon" is on. That
// position was never pinned to centre, so it is deliberately left alone rather
// than moved - writing it would change which object is picked with the head
// sitting still. In that option set the crosshair sprite still follows the aim
// point while the pick does not, which is Arx's own two control schemes showing
// through and needs the addresses of those two options to close properly.
void PlaceCursorOnAimPoint();

// Undoes the move at the frame boundary, so the value the game finds next frame
// is the one it left.
void RestoreCursor();

}  // namespace ArxHeadTracking
