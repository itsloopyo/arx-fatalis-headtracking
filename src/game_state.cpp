// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "game_state.h"

#include "arx_game.h"
#include "engine_memory.h"

namespace ArxHeadTracking {

namespace {

const BuildProfile* g_profile = nullptr;

}  // namespace

void InitGameState(const BuildProfile& profile) {
    g_profile = &profile;
}

bool IsGameplayFrame() {
    // Third person: the death camera, the Magic Sight eyeball, a scripted
    // camera, a conversation. The eye is no longer the player's.
    if (ReadLong(g_profile->addrExternalView) != 0) return false;

    // The game has taken the player's controls for a scripted sequence.
    if (ReadLong(g_profile->addrBlockControls) != 0) return false;

    // A cinematic is playing.
    if (ReadLong(g_profile->addrCinemascope) != 0) return false;

    // The book, the inventory page or a note is open.
    if ((ReadShort(g_profile->addrPlayerInterface) & kInterfaceBlockingFlags) != 0) return false;

    // The camera the game is about to render through has to be the player's.
    // In first person the game assigns subj.angle straight from player.angle,
    // so the three compare bit-exact; every path that takes the camera away
    // from the player writes something else there, including the one frame a
    // transition into the death camera starts on.
    const auto* subj = reinterpret_cast<const EerieCamera*>(g_profile->addrSubj);
    const auto* playerAngle =
        reinterpret_cast<const float*>(g_profile->addrPlayerPos + kPlayerAngleOffset);
    return subj->angle_a == playerAngle[0] && subj->angle_b == playerAngle[1] &&
           subj->angle_g == playerAngle[2];
}

}  // namespace ArxHeadTracking
