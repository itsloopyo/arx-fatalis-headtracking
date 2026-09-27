#pragma once

// The oracle: the config reader and hotkey registration of the dev pre-release (2e9ca90), the
// newest published build, compiled from oracle/ with the core sources they included at its pin
// (152b471). Two libraries build it, each with its namespaces renamed at compile time so it links
// beside the current core: the reader, and Hotkeys::Start against oracle_fake's recording poller.
// This header names no core type, so the test includes it without the renaming.

#include <array>
#include <string>
#include <vector>

namespace arx_oracle_view {

struct OracleConfig {
    int udp_port;
    bool enabled_on_startup;
    float sens_yaw, sens_pitch, sens_roll;
    bool invert_yaw, invert_pitch, invert_roll;
    float local_smoothing, remote_smoothing;
    bool position_enabled;
    float pos_sens_x, pos_sens_y, pos_sens_z;
    float pos_limit_x, pos_limit_y, pos_limit_z, pos_limit_z_back;
    bool collision_enabled;
    float collision_radius, collision_release_smoothing;
    float field_of_view;
    bool move_crosshair;
    int vk_toggle, vk_cycle_mode;
    bool diagnostics;
};

// Config::LoadOrCreate on `path` as the dev build's init thread ran it, from a default Config. It
// creates the file when there is none, as that build did. The dev build never refused a file.
OracleConfig RunOracle(const std::string& path);

// Which actions a key press fires, for every key a binding can name (0x01-0xFE) under every set
// of held modifiers. Entry (vk - kFirstKey) * kHeldStates + held counts the toggle and cycle
// actions fired, in that order. held: 1 Ctrl, 2 Shift, 4 Alt.
constexpr int kFirstKey = 0x01;
constexpr int kLastKey = 0xFE;
constexpr int kHeldStates = 8;
using FireTable = std::vector<std::array<int, 2>>;

// The dev build's Hotkeys::Start run on the two codes, pressing each key under each held set.
FireTable OracleFires(int vk_toggle, int vk_cycle_mode);

}  // namespace arx_oracle_view
