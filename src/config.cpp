// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include "logging.h"
#include "legacy_config/legacy_config.h"

#include "cameraunlock/config/ini_reader.h"

#include <windows.h>

namespace ArxHeadTracking {

namespace {

using cameraunlock::IniWriter;

bool FileExists(const char* path) {
    const DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

void WriteDefaultIni(const char* path, const Config& d) {
    IniWriter w;
    if (!w.Open(path)) {
        Log::Line("WARN: could not create %s. The defaults are in use for this session and "
                  "nothing will persist - the game directory is not writable by this account.",
                  path);
        return;
    }

    w.WriteComment(" Arx Fatalis Head Tracking");
    w.WriteComment("");
    w.WriteComment(" Head tracking moves the view. The mouse still turns the character and");
    w.WriteComment(" still decides where arrows, spells and sword swings go, so what you see");
    w.WriteComment(" and what you aim at come apart. The cursor is redrawn on the point you");
    w.WriteComment(" are actually aiming at, and picks up whatever is under it there.");
    w.WriteComment("");
    w.WriteComment(" Centre your head in the tracker (OpenTrack's Center bind, or the CENTER");
    w.WriteComment(" button in a phone app). This mod keeps no centre of its own.");
    w.WriteBlankLine();

    w.WriteSection("Network");
    w.WriteComment(" UDP port to listen on. 4242 is what OpenTrack sends to by default.");
    w.WriteInt("Port", d.udp_port);
    w.WriteBlankLine();

    w.WriteSection("General");
    w.WriteComment(" Whether tracking is live as soon as the game starts.");
    w.WriteBool("EnableOnStartup", d.enabled_on_startup);
    w.WriteComment(" Move the game's cursor onto the point you are aiming at. Turning this");
    w.WriteComment(" off leaves the cursor where the game puts it, which is wherever your");
    w.WriteComment(" head is pointed rather than where your character is.");
    w.WriteBool("MoveCrosshair", d.move_crosshair);
    w.WriteComment(" Vertical field of view in degrees. Arx has no setting of its own and");
    w.WriteComment(" renders 75.95 degrees vertically, which widens horizontally on a wide");
    w.WriteComment(" monitor. 0 leaves the game's own alone. 40 to 110 can be set, and the");
    w.WriteComment(" view still narrows when you draw a bow either way.");
    w.WriteDouble("FieldOfView", d.field_of_view);
    w.WriteComment(" Write a line a second to ArxFatalisHeadTracking.log naming the camera the");
    w.WriteComment(" shot leaves from, the camera the frame is drawn through and the point the");
    w.WriteComment(" cursor is placed on. Only useful for reporting a problem.");
    w.WriteBool("Diagnostics", d.diagnostics);
    w.WriteBlankLine();

    w.WriteSection("Sensitivity");
    w.WriteComment(" Shape the pose in your tracker, not here, so one profile behaves the");
    w.WriteComment(" same in every game. These stay at 1.0 unless you have a reason.");
    w.WriteDouble("YawSensitivity", d.sens_yaw);
    w.WriteDouble("PitchSensitivity", d.sens_pitch);
    w.WriteDouble("RollSensitivity", d.sens_roll);
    w.WriteBlankLine();

    w.WriteSection("Inversion");
    w.WriteComment(" Fix a mirrored axis in your tracker where you can, so it is right in");
    w.WriteComment(" every game at once.");
    w.WriteBool("InvertYaw", d.invert_yaw);
    w.WriteBool("InvertPitch", d.invert_pitch);
    w.WriteBool("InvertRoll", d.invert_roll);
    w.WriteBlankLine();

    w.WriteSection("Smoothing");
    w.WriteComment(" Which of these applies is decided per connection, by the address the");
    w.WriteComment(" packets arrive from. Only loopback (127.0.0.1) counts as local: a");
    w.WriteComment(" tracker running on this same PC but sending to the machine's LAN");
    w.WriteComment(" address is treated as remote.");
    w.WriteComment(" Both cover rotation and position. 0 is no smoothing at all.");
    w.WriteDouble("LocalSmoothing", d.local_smoothing);
    w.WriteDouble("RemoteSmoothing", d.remote_smoothing);
    w.WriteBlankLine();

    w.WriteSection("Position");
    w.WriteComment(" Positional (6DOF) tracking - leaning. Limits are in metres.");
    w.WriteBool("PositionEnabled", d.position_enabled);
    w.WriteComment(" As above: shape the pose in your tracker. X is side to side, Y is up");
    w.WriteComment(" and down, Z is forward and back.");
    w.WriteDouble("PositionSensitivityX", d.pos_sens_x);
    w.WriteDouble("PositionSensitivityY", d.pos_sens_y);
    w.WriteDouble("PositionSensitivityZ", d.pos_sens_z);
    w.WriteComment(" How far the view may move from where the game put it, in metres.");
    w.WriteDouble("PositionLimitX", d.pos_limit_x);
    w.WriteDouble("PositionLimitY", d.pos_limit_y);
    w.WriteComment(" Forward gets more room than backward so pulling back does not put the");
    w.WriteComment(" view inside your own body.");
    w.WriteDouble("PositionLimitZ", d.pos_limit_z);
    w.WriteDouble("PositionLimitZBack", d.pos_limit_z_back);
    w.WriteBlankLine();

    w.WriteComment(" Stop a lean pushing the view through a wall. The trace uses the game's");
    w.WriteComment(" own level collision. CollisionRadius is how far off a surface the eye is");
    w.WriteComment(" held, in Arx units (1 unit = 1 cm), and must stay above the engine's");
    w.WriteComment(" 1-unit near clip or the wall is culled and you see through it anyway.");
    w.WriteBool("CollisionEnabled", d.collision_enabled);
    w.WriteDouble("CollisionRadius", d.collision_radius);
    w.WriteDouble("CollisionReleaseSmoothing", d.collision_release_smoothing);
    w.WriteBlankLine();

    w.WriteSection("Hotkeys");
    w.WriteComment(" Virtual key codes. Every action also has a Ctrl+Shift chord for");
    w.WriteComment(" keyboards with no navigation cluster.");
    w.WriteComment(" End      / Ctrl+Shift+Y : tracking on or off");
    w.WriteComment(" Page Up  / Ctrl+Shift+J : cycle 6DOF -> rotation only -> position only");
    w.WriteComment("");
    w.WriteComment(" Both sets collide with something Arx already uses; pick whichever you");
    w.WriteComment(" mind less. End, Page Up and Page Down are centre view, look up and look");
    w.WriteComment(" down. Ctrl is magic mode and Shift is stealth mode, so holding a chord");
    w.WriteComment(" enters both for as long as you hold it.");
    w.WriteComment(" There is no recenter key. Centre your head in the tracker.");
    w.WriteHex("ToggleKey", d.vk_toggle);
    w.WriteHex("CycleTrackingModeKey", d.vk_cycle_mode);
    w.Close();

    Log::Line("Wrote default config to %s", path);
}

}  // namespace

bool Config::LoadOrCreate(const char* path) {
    if (!path || !*path) {
        Log::Line("ERROR: could not resolve the directory this mod was loaded from, so there "
                  "is nowhere to read or write the config.");
        return false;
    }

    if (!FileExists(path)) {
        WriteDefaultIni(path, Config{});
    }

    legacy::Config l;
    if (legacy::Read(path, l) == legacy::ReadStatus::Absent) {
        Log::Line("WARN: %s could not be opened; running on defaults.", path);
    }

    udp_port = l.udp_port;
    enabled_on_startup = l.enabled_on_startup;
    move_crosshair = l.move_crosshair;
    diagnostics = l.diagnostics;
    field_of_view = l.field_of_view;
    sens_yaw = l.sens_yaw;
    sens_pitch = l.sens_pitch;
    sens_roll = l.sens_roll;
    invert_yaw = l.invert_yaw;
    invert_pitch = l.invert_pitch;
    invert_roll = l.invert_roll;
    local_smoothing = l.local_smoothing;
    remote_smoothing = l.remote_smoothing;
    position_enabled = l.position_enabled;
    pos_sens_x = l.pos_sens_x;
    pos_sens_y = l.pos_sens_y;
    pos_sens_z = l.pos_sens_z;
    pos_limit_x = l.pos_limit_x;
    pos_limit_y = l.pos_limit_y;
    pos_limit_z = l.pos_limit_z;
    pos_limit_z_back = l.pos_limit_z_back;
    collision_enabled = l.collision_enabled;
    collision_radius = l.collision_radius;
    collision_release_smoothing = l.collision_release_smoothing;
    vk_toggle = l.vk_toggle;
    vk_cycle_mode = l.vk_cycle_mode;

    return true;
}

}  // namespace ArxHeadTracking
