// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

// The config reader of the dev pre-release (2e9ca90), the last build that read
// ArxFatalisHeadTracking.ini, frozen so a player updating from it is converted exactly as that
// build read the file. Nothing in this folder is ever edited. Three things differ from the reader
// it was taken from: it fills this frozen copy of that build's Config and defaults rather than the
// runtime type, it never writes the file (a missing file reads as the defaults, which is what the
// old reader read from the file it created there), and it reports an absent file apart from one
// it read. The core default values the old Config took from PositionSettings and
// smoothing_utils.h are written out here as numbers, so a later core cannot move what an old file
// converts to.

#include "cameraunlock/config/legacy_import.h"

#include <cstdint>
#include <vector>

namespace ArxHeadTracking::legacy {

enum class ReadStatus {
    Read,
    // No file at the path, or none the old reader could open. Config holds the defaults.
    Absent,
};

struct Config {
    uint16_t udp_port = 4242;
    bool enabled_on_startup = true;

    float sens_yaw = 1.0f;
    float sens_pitch = 1.0f;
    float sens_roll = 1.0f;
    bool invert_yaw = false;
    bool invert_pitch = false;
    bool invert_roll = false;

    float local_smoothing = 0.0f;
    float remote_smoothing = 0.15f;

    bool position_enabled = true;
    float pos_sens_x = 1.0f;
    float pos_sens_y = 1.0f;
    float pos_sens_z = 1.0f;
    float pos_limit_x = 0.30f;
    float pos_limit_y = 0.20f;
    float pos_limit_z = 0.40f;
    float pos_limit_z_back = 0.10f;

    bool collision_enabled = true;
    float collision_radius = 18.0f;
    float collision_release_smoothing = 0.9f;

    float field_of_view = 0.0f;
    bool move_crosshair = true;

    int vk_toggle = 0x23;      // End
    int vk_cycle_mode = 0x21;  // Page Up

    bool diagnostics = false;
};

// Reads the file at `path`, the ANSI path the dev build opened it by, into a default-constructed
// `c`.
ReadStatus Read(const char* path, Config& c);

// Every section and key Read reads, in the order it reads them.
std::vector<cameraunlock::config::LegacyKey> ReadKeys();

}  // namespace ArxHeadTracking::legacy
