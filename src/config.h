// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "cameraunlock/camera/lean_clamp.h"
#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/config_table.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/legacy_import.h"
#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

#include <cstdint>
#include <string>

namespace ArxHeadTracking {

constexpr const char* kConfigFileName = "CameraUnlock.ini";
// The file the dev pre-release read, beside kConfigFileName. Imported once while kConfigFileName
// is absent, and never written.
constexpr const char* kLegacyConfigFileName = "ArxFatalisHeadTracking.ini";
// The game's name as cameraunlock-core's data/games.json spells it.
constexpr const char* kConfigDisplayName = "Arx Fatalis";

// The lean standoff's bounds, in Arx units (centimetres). Arx builds its projection with a near
// plane of 1 unit (EERIE_CreateMatriceProj is called with 1.f), and geometry nearer the eye than
// that is culled, so a standoff at or under the near plane stops the eye short of the wall and has
// the wall vanish anyway. Two units puts the eye a whole unit clear of the plane. Two metres is far
// past anything sane and still finite.
constexpr float kMinCollisionMargin = 2.0f;
constexpr float kMaxCollisionMargin = 200.0f;
constexpr float kDefaultCollisionMargin = 18.0f;

// The vertical field of view a player may ask for, in degrees. The bounds are where Arx's own
// focal map stops being invertible cleanly: 40 degrees is focal 664 and 110 degrees is focal 172,
// both on lines with a single inverse, and the map runs flat below 33.5 degrees where no focal
// answers at all.
constexpr float kMinFieldOfView = 40.0f;
constexpr float kMaxFieldOfView = 110.0f;

struct Config {
    uint16_t udp_port = 4242;
    bool enable_on_startup = true;
    // The startup tracking mode. The mode hotkey saves both.
    bool rotation_enabled = true;
    bool position_enabled = true;

    float local_smoothing = static_cast<float>(cameraunlock::math::kDefaultLocalSmoothing);
    float remote_smoothing = static_cast<float>(cameraunlock::math::kDefaultRemoteSmoothing);

    // The limits; the sensitivities and inversions stay at identity; the tracker shapes the pose.
    cameraunlock::PositionSettings position;

    // Lean collision, confirmed in game: against a wall 116.5 units away with a 120-unit lean
    // requested, the eye came to rest 98.5 units out, the wall distance less the standoff.
    bool collision_enabled = true;
    cameraunlock::camera::LeanClampSettings lean_clamp{kDefaultCollisionMargin, 0.9f};

    std::string toggle_key_name = "End, Ctrl+Shift+Y";
    // Arx binds G to drinking a mana potion, so the cycle chord is J, the next free letter of the
    // cluster. The row is PerGame for that reason.
    std::string cycle_tracking_mode_key_name = "PageUp, Ctrl+Shift+J";

    // The player's own vertical field of view in degrees, or 0 to render the game's. Arx has no
    // field of view control of its own and renders a fixed 75.95 degrees vertically.
    float field_of_view = 0.0f;

    // A per-second line naming the clean camera, the tracked camera and the aim point.
    bool diagnostics = false;
};

// The rows of CameraUnlock.ini. Only the tracking mode pair is Writable: the mode hotkey saves the
// player's choice, and End changes the session only.
cameraunlock::config::ConfigTable<Config> MakeConfigTable();

// ArxFatalisHeadTracking.ini as the dev build read it (legacy_config/), mapped into Config.
cameraunlock::config::LegacyImport<Config> MakeLegacyImport();

// The owner's options for the files in `folder` (with its trailing separator): the settings in
// CameraUnlock.ini, imported once from ArxFatalisHeadTracking.ini. The mod passes
// DefaultsFile::PerUser() and a test a scratch file.
cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults);

}  // namespace ArxHeadTracking
