// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include "legacy_config/legacy_config.h"

#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ArxHeadTracking {

namespace {

namespace cfg = cameraunlock::config;
using cfg::DroppedValue;
using cfg::DropRule;
using cfg::ImportResult;
using cfg::LegacyFollowsDefaultsIni;
using cfg::LegacyInput;
using cfg::LegacyPoseShaping;
using cfg::PoseShapingValue;
using cfg::schema::Concept;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// 0, the game's own field of view, or a chosen one from kMinFieldOfView to kMaxFieldOfView.
// Between 0 and kMinFieldOfView no focal answers, so those values are refused like any value
// outside the range.
class FieldOfViewCodec {
public:
    using Value = float;

    cfg::CodecParseResult<float> Parse(std::string_view text) const {
        cfg::CodecParseResult<float> read = inner_.Parse(text);
        if (!read.ok() || (read.value != 0.0f && read.value < kMinFieldOfView)) {
            return {0.0f, "0, or a number from 40 to 110"};
        }
        return read;
    }

    std::string Render(float value) const { return inner_.Render(value); }

    bool Equal(float a, float b) const { return inner_.Equal(a, b); }

private:
    cfg::FloatCodec inner_{0.0f, kMaxFieldOfView};
};

// A legacy hotkey code and the Ctrl+Shift chord the dev build always registered beside it, as one
// key list: the code's binding (none for a code no hotkey can hold, N1 and N3), then the chord.
std::string KeyList(int vk, char letter, const char* key, std::vector<DroppedValue>& dropped) {
    const std::string code = cfg::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    const std::string chord = cameraunlock::input::FormatKeyBindings(
        std::vector<KeyBinding>{{KeyModifiers::kCtrl | KeyModifiers::kShift, letter}});
    return code.empty() ? chord : code + ", " + chord;
}

ImportResult Import(const LegacyInput& input, Config& out) {
    // The dev build opened the file by the ANSI path GetModuleFileNameA gave it, which is the
    // owner's ANSI form of the same path.
    legacy::Config c;
    const legacy::ReadStatus read = legacy::Read(input.ansi_path.c_str(), c);
    const legacy::Config shipped;

    std::vector<DroppedValue> dropped;
    std::vector<PoseShapingValue> shaping;

    out.udp_port = c.udp_port;
    out.enable_on_startup = c.enabled_on_startup;

    // [Position] PositionEnabled chose only the startup mode: the cycle key reached every mode
    // either way.
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(
        c.position_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                           : cameraunlock::TrackingMode::RotationOnly);
    out.rotation_enabled = mode.rotation_enabled;
    out.position_enabled = mode.position_enabled;

    out.local_smoothing = c.local_smoothing;
    out.remote_smoothing = c.remote_smoothing;

    // The old file had one vertical limit, which the old runtime applied both ways.
    out.position.limit_x = c.pos_limit_x;
    out.position.limit_y = c.pos_limit_y;
    out.position.limit_y_down = c.pos_limit_y;
    out.position.limit_z = c.pos_limit_z;
    out.position.limit_z_back = c.pos_limit_z_back;

    out.collision_enabled = c.collision_enabled;
    out.lean_clamp.skin = c.collision_radius;
    out.lean_clamp.release_smoothing = c.collision_release_smoothing;

    out.field_of_view = c.field_of_view;
    out.diagnostics = c.diagnostics;

    // Every sensitivity and inversion shipped at identity, so nothing folds and a value the player
    // changed is dropped.
    LegacyPoseShaping(c.sens_yaw, shipped.sens_yaw, "Sensitivity", "YawSensitivity", shaping, dropped);
    LegacyPoseShaping(c.sens_pitch, shipped.sens_pitch, "Sensitivity", "PitchSensitivity", shaping, dropped);
    LegacyPoseShaping(c.sens_roll, shipped.sens_roll, "Sensitivity", "RollSensitivity", shaping, dropped);
    LegacyPoseShaping(c.invert_yaw, shipped.invert_yaw, "Inversion", "InvertYaw", shaping, dropped);
    LegacyPoseShaping(c.invert_pitch, shipped.invert_pitch, "Inversion", "InvertPitch", shaping, dropped);
    LegacyPoseShaping(c.invert_roll, shipped.invert_roll, "Inversion", "InvertRoll", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_x, shipped.pos_sens_x, "Position", "PositionSensitivityX", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_y, shipped.pos_sens_y, "Position", "PositionSensitivityY", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_z, shipped.pos_sens_z, "Position", "PositionSensitivityZ", shaping, dropped);

    // The crosshair always follows the aim now.
    if (!c.move_crosshair) dropped.push_back({DropRule::Reticle, "General", "MoveCrosshair", "false"});

    out.toggle_key_name = KeyList(c.vk_toggle, 'Y', "ToggleKey", dropped);
    out.cycle_tracking_mode_key_name = KeyList(c.vk_cycle_mode, 'J', "CycleTrackingModeKey", dropped);

    // A row still at what the dev build ran on with no file is no player's choice, so it follows
    // Defaults.ini. The chords were fixed in code, so each hotkey's code decides alone.
    // CycleTrackingModeKey is PerGame and CollisionMargin is not global, so neither is given.
    LegacyFollowsDefaultsIni follows;
    follows.Setting(Concept::UdpPort, c.udp_port, shipped.udp_port);
    follows.Setting(Concept::EnableOnStartup, c.enabled_on_startup, shipped.enabled_on_startup);
    follows.TrackingMode(c.position_enabled, shipped.position_enabled);
    follows.Setting(Concept::LocalSmoothing, c.local_smoothing, shipped.local_smoothing);
    follows.Setting(Concept::RemoteSmoothing, c.remote_smoothing, shipped.remote_smoothing);
    follows.Setting(Concept::PositionLimitX, c.pos_limit_x, shipped.pos_limit_x);
    follows.Setting(Concept::PositionLimitY, c.pos_limit_y, shipped.pos_limit_y);
    follows.Setting(Concept::PositionLimitYDown, c.pos_limit_y, shipped.pos_limit_y);
    follows.Setting(Concept::PositionLimitZ, c.pos_limit_z, shipped.pos_limit_z);
    follows.Setting(Concept::PositionLimitZBack, c.pos_limit_z_back, shipped.pos_limit_z_back);
    follows.Setting(Concept::CollisionEnabled, c.collision_enabled, shipped.collision_enabled);
    follows.Setting(Concept::CollisionReleaseSmoothing, c.collision_release_smoothing,
                    shipped.collision_release_smoothing);
    follows.Setting(Concept::ToggleKey, c.vk_toggle, shipped.vk_toggle);

    return read == legacy::ReadStatus::Absent
               ? ImportResult::Absent(std::move(dropped), std::move(shaping), follows.Concepts())
               : ImportResult::Imported(std::move(dropped), std::move(shaping), follows.Concepts());
}

}  // namespace

cfg::ConfigTable<Config> MakeConfigTable() {
    cfg::ConfigTable<Config> table{Config{}};
    table.Concept<Concept::UdpPort>(&Config::udp_port)
        .Concept<Concept::EnableOnStartup>(&Config::enable_on_startup)
        .Concept<Concept::RotationEnabled>(&Config::rotation_enabled)
        .Writable()
        .Concept<Concept::LocalSmoothing>(&Config::local_smoothing)
        .Concept<Concept::RemoteSmoothing>(&Config::remote_smoothing)
        .Concept<Concept::PositionEnabled>(&Config::position_enabled)
        .Writable()
        .Concept<Concept::PositionLimitX>([](const Config& c) { return c.position.limit_x; },
                                          [](Config& c, float v) { c.position.limit_x = v; })
        .Concept<Concept::PositionLimitY>([](const Config& c) { return c.position.limit_y; },
                                          [](Config& c, float v) { c.position.limit_y = v; })
        .Concept<Concept::PositionLimitYDown>([](const Config& c) { return c.position.limit_y_down; },
                                              [](Config& c, float v) { c.position.limit_y_down = v; })
        .Concept<Concept::PositionLimitZ>([](const Config& c) { return c.position.limit_z; },
                                          [](Config& c, float v) { c.position.limit_z = v; })
        .Concept<Concept::PositionLimitZBack>([](const Config& c) { return c.position.limit_z_back; },
                                              [](Config& c, float v) { c.position.limit_z_back = v; })
        .Concept<Concept::CollisionEnabled>(&Config::collision_enabled)
        .Concept<Concept::CollisionMargin>([](const Config& c) { return c.lean_clamp.skin; },
                                           [](Config& c, float v) { c.lean_clamp.skin = v; })
        .Comment("How far, in Arx units (1 unit = 1 cm), a lean holds the eye off a wall. 2 to 200.\n"
                 "It must stay above the engine's 1-unit near clip, or the wall is culled anyway.")
        .Concept<Concept::CollisionReleaseSmoothing>([](const Config& c) { return c.lean_clamp.release_smoothing; },
                                                     [](Config& c, float v) { c.lean_clamp.release_smoothing = v; })
        .Concept<Concept::ToggleKey>(&Config::toggle_key_name)
        .Concept<Concept::CycleTrackingModeKey>(&Config::cycle_tracking_mode_key_name)
        .PerGame();
    table.Local("General", "FieldOfView", &Config::field_of_view, FieldOfViewCodec(),
                "Vertical field of view in degrees. Arx has no setting of its own and renders\n"
                "75.95 degrees vertically, which widens horizontally on a wide monitor. 0 leaves\n"
                "the game's own alone. 40 to 110 can be set, and the view still narrows when you\n"
                "draw a bow either way.");
    table.Local("General", "Diagnostics", &Config::diagnostics, cfg::BoolCodec(),
                "true: write a line a second to ArxFatalisHeadTracking.log naming the camera the\n"
                "shot leaves from, the camera the frame is drawn through and the point the cursor\n"
                "is placed on. Only useful for reporting a problem.");
    return table;
}

cfg::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cfg::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder, cfg::DefaultsFile defaults) {
    const auto wide = [](const char* name) { return std::wstring(name, name + std::char_traits<char>::length(name)); };
    cfg::ConfigOwnerOptions<Config> options;
    options.path = folder + wide(kConfigFileName);
    options.legacy_path = folder + wide(kLegacyConfigFileName);
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

}  // namespace ArxHeadTracking
