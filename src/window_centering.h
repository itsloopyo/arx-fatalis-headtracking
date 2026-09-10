// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

namespace ArxHeadTracking {

// Waits for the game to bring its window up and hold it still, then centres it
// on the work area of the monitor it is already on. A window that already fills
// the work area, or that the game centred itself, is left alone - so this only
// ever moves a windowed-mode game.
//
// Blocks until the window settles or the wait times out, so call it after
// everything else the init thread has to do.
void CenterWindowWhenReady();

}  // namespace ArxHeadTracking
