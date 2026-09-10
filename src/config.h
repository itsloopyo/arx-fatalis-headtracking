// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

#include <cstdint>

namespace ArxHeadTracking {

// The UDP port this mod listens on. 4242 is the OpenTrack default and is what a
// user's tracker sends to; nothing here changes that, it is only the default
// this build ships with in the INI.
constexpr uint16_t kDefaultUdpPort = 4242;

// The range a port may be set to. Below 1024 is the privileged range, which a
// game process has no business binding into.
constexpr int kMinUdpPort = 1024;
constexpr int kMaxUdpPort = 65535;

// The largest lean standoff a player may ask for, in Arx units (centimetres).
// Two metres is far past anything sane and still finite.
constexpr float kMaxCollisionRadius = 200.0f;

// The smallest one. Arx builds its projection with a near plane of 1 unit
// (EERIE_CreateMatriceProj is called with 1.f), and geometry nearer the eye than
// that is culled - so a standoff at or under the near plane stops the eye short
// of the wall and has the wall vanish anyway. The player still sees through it,
// while the log reports the clamp as engaging. Two units puts the eye a whole
// unit clear of the plane, which is the whole of the requirement.
constexpr float kMinCollisionRadius = 2.0f;

// The vertical field of view a player may ask for, in degrees. The bounds are
// where Arx's own focal map stops being invertible cleanly: 40 degrees is focal
// 664 and 110 degrees is focal 172, both on lines with a single inverse, and
// the map runs flat below 33.5 degrees where no focal answers at all.
constexpr float kMinFieldOfView = 40.0f;
constexpr float kMaxFieldOfView = 110.0f;

struct Config {
    uint16_t udp_port = kDefaultUdpPort;
    bool enabled_on_startup = true;

    float sens_yaw = 1.0f;
    float sens_pitch = 1.0f;
    // Read, carried through the pipeline and then dropped at the engine
    // boundary: Arx projects through a transform with no roll term, so no roll
    // value can change a pixel. Kept so the INI matches the rest of the fleet
    // rather than growing a hole where a familiar key should be.
    float sens_roll = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;

    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    bool position_enabled = true;
    float pos_sens_x = 1.0f;
    float pos_sens_y = 1.0f;
    float pos_sens_z = 1.0f;
    float pos_limit_x = cameraunlock::PositionSettings{}.limit_x;
    float pos_limit_y = cameraunlock::PositionSettings{}.limit_y;
    float pos_limit_z = cameraunlock::PositionSettings{}.limit_z;
    float pos_limit_z_back = cameraunlock::PositionSettings{}.limit_z_back;

    // Lean collision. On, because the trace has been confirmed in game: against a
    // wall 116.5 units away with a 120-unit lean requested, the eye came to rest
    // 98.5 units out - exactly the wall distance less the standoff - and the log
    // reported contact. The radius is in Arx units (centimetres) and has to stay
    // above the engine's 1-unit near clip distance, or the wall is culled and the
    // player sees through it anyway.
    bool collision_enabled = true;
    float collision_radius = 18.0f;
    float collision_release_smoothing = 0.9f;

    // The player's own vertical field of view in degrees, or 0 to render the
    // game's.
    //
    // Arx has no field of view control of its own - the video options are
    // resolution, colour depth, detail, fog, gamma, luminosity, contrast,
    // crosshair and antialiasing, and nothing else - and it renders a fixed
    // 75.95 degrees vertically, widening horizontally on a wide monitor.
    // Setting this writes the focal the engine builds its whole projection
    // from, so the world, the sprites and the cursor mark all move together,
    // and whatever zoom the game applies on top of it is kept as a ratio.
    float field_of_view = 0.0f;

    // Whether the game's own cursor is moved to the point the player is aiming
    // at. Off means head tracking still works but the crosshair marks where the
    // head is pointed rather than where the body is.
    bool move_crosshair = true;

    // Nav-cluster hotkeys.
    // Arx binds all three nav-cluster keys itself (End centres the view, Page Up
    // and Page Down tilt it), so the chords below are the conflict-free way to
    // reach these actions. The nav bindings stay on the fleet's standard keys so
    // muscle memory carries between mods; a player who minds can rebind either
    // side.
    int vk_toggle = 0x23;      // End
    int vk_cycle_mode = 0x21;  // Page Up

    // Writes a per-second line naming the clean camera, the tracked camera and
    // the aim point it projects. Off by default; it is what a bug report about
    // the cursor drifting is answered with.
    bool diagnostics = false;

    // Reads the INI, writing a fully commented default file when there is none.
    // Returns false only when the path could not be resolved at all.
    bool LoadOrCreate(const char* path);
};

}  // namespace ArxHeadTracking
