// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "config.h"

#include "logging.h"

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/config/value_guards.h"

#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace ArxHeadTracking {

namespace {

using cameraunlock::IniReader;
using cameraunlock::IniWriter;
namespace guards = cameraunlock::config;

void LogSink(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Log::Line("%s", buffer);
}

bool FileExists(const char* path) {
    const DWORD attrs = GetFileAttributesA(path);
    return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// The sanitised reads below all share one of three bounds. Naming each bound
// once keeps a key from quietly picking up a different range than its siblings.

// A sensitivity is symmetric about zero: a negative one is a legitimate
// inversion rather than a mistake.
float ReadSensitivity(const IniReader& ini, const char* section, const char* key,
                      float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, -guards::kMaxSensitivity,
                                    guards::kMaxSensitivity, LogSink);
}

// A position limit is a distance in metres, so zero is the floor - a negative
// one would invert the processor's bounds and pin the lean at a fixed offset.
float ReadPositionLimit(const IniReader& ini, const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, "Position", key, fallback, 0.0f,
                                    guards::kMaxPositionLimit, LogSink);
}

// A 0-to-1 fraction: both smoothing values and the collision release ease.
float ReadFraction(const IniReader& ini, const char* section, const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, 0.0f, 1.0f, LogSink);
}

// IniReader::ReadBool compares the WHOLE value against its accepted words, so
// `CollisionEnabled=0 ; off while I test` matches none of them and the user's
// edit is discarded with nothing in the log. Strip the comment first, the way
// every numeric key here already does, and say so when what is left is still
// not a yes or a no.
bool ReadBoolChecked(const IniReader& ini, const char* section, const char* key,
                     bool fallback) {
    const std::string text = guards::ReadRawValue(ini, section, key);
    if (text.empty()) return fallback;

    if (text == "1" || text == "true" || text == "True" || text == "TRUE" ||
        text == "yes" || text == "Yes" || text == "YES" ||
        text == "on" || text == "On" || text == "ON") {
        return true;
    }
    if (text == "0" || text == "false" || text == "False" || text == "FALSE" ||
        text == "no" || text == "No" || text == "NO" ||
        text == "off" || text == "Off" || text == "OFF") {
        return false;
    }
    Log::Line("%s=%s is not a yes or no value; keeping %d.", key, text.c_str(),
              fallback ? 1 : 0);
    return fallback;
}

// True only when the WHOLE of @p text is a hex number, with an optional 0x.
//
// strtol parses a PREFIX, so IniReader::ReadHex answers `ToggleKey=End` with
// 0x0E - an undefined virtual key that passes every validity test there is and
// can never be pressed. Key NAMES are the spelling the fleet's config schema
// uses for these two entries, so a user carrying that convention into this file
// is the expected mistake rather than a far-fetched one, and it has to be
// refused out loud.
bool ParseHexStrict(const std::string& text, int& out) {
    size_t i = (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) ? 2 : 0;
    if (i >= text.size()) return false;
    int value = 0;
    for (; i < text.size(); ++i) {
        const char c = text[i];
        int digit;
        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = c - 'a' + 10;
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return false;
        }
        if (value > (0xFFFF - digit) / 16) return false;
        value = value * 16 + digit;
    }
    out = value;
    return true;
}

int ReadKeyChecked(const IniReader& ini, const char* key, int fallback) {
    const std::string text = guards::ReadRawValue(ini, "Hotkeys", key);
    if (text.empty()) return fallback;

    int vk = 0;
    if (!ParseHexStrict(text, vk)) {
        Log::Line("%s=%s is not a hex key code; keeping 0x%02X. Write the code itself, as in "
                  "0x23, rather than the key's name.", key, text.c_str(), fallback);
        return fallback;
    }
    if (!guards::IsBindableVirtualKey(vk)) {
        Log::Line("%s=0x%02X is not a key this mod can watch; keeping 0x%02X.",
                  key, vk, fallback);
        return fallback;
    }
    return vk;
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

    IniReader ini;
    if (!ini.Open(path)) {
        Log::Line("WARN: %s could not be opened; running on defaults.", path);
        return true;
    }

    const int port = ini.ReadInt("Network", "Port", udp_port);
    if (port < kMinUdpPort || port > kMaxUdpPort) {
        Log::Line("Port=%d is outside %d-%d; keeping %u.", port, kMinUdpPort, kMaxUdpPort,
                  udp_port);
    } else {
        udp_port = static_cast<uint16_t>(port);
    }

    enabled_on_startup = ReadBoolChecked(ini, "General", "EnableOnStartup", enabled_on_startup);
    move_crosshair = ReadBoolChecked(ini, "General", "MoveCrosshair", move_crosshair);
    diagnostics = ReadBoolChecked(ini, "General", "Diagnostics", diagnostics);

    // 0 is the whole of "leave the game alone", so it is let through rather than
    // range-checked; anything else outside the bounds falls back to it and says
    // so, because silently clamping a field of view to 110 is a stranger answer
    // than rendering what the game always did.
    //
    // Parsed strictly rather than through IniReader::ReadFloat. Every float in
    // this file goes through the guards; this one did not. Every comparison against a
    // NaN is false, so `FieldOfView=nan` passed the range test here AND the
    // `<= 0` gate in ApplyFieldOfView, and std::lround of it went into
    // CURRENT_BASE_FOCAL - which the whole session's projection, sprite scale
    // and culling frustum are built from. A prefix parse would have taken
    // `75,95` for 75 just as quietly.
    const std::string fovText = guards::ReadRawValue(ini, "General", "FieldOfView");
    if (!fovText.empty()) {
        float fov = 0.0f;
        if (!guards::ParseFloatStrict(fovText, fov) || !std::isfinite(fov)) {
            Log::Line("FieldOfView=%s is not a number; rendering the game's own field of view "
                      "instead.", fovText.c_str());
        } else if (fov != 0.0f && (fov < kMinFieldOfView || fov > kMaxFieldOfView)) {
            Log::Line("FieldOfView=%.1f is outside %.0f-%.0f degrees; rendering the game's own "
                      "field of view instead.", fov, kMinFieldOfView, kMaxFieldOfView);
        } else {
            field_of_view = fov;
        }
    }

    sens_yaw = ReadSensitivity(ini, "Sensitivity", "YawSensitivity", sens_yaw);
    sens_pitch = ReadSensitivity(ini, "Sensitivity", "PitchSensitivity", sens_pitch);
    sens_roll = ReadSensitivity(ini, "Sensitivity", "RollSensitivity", sens_roll);

    invert_yaw = ReadBoolChecked(ini, "Inversion", "InvertYaw", invert_yaw);
    invert_pitch = ReadBoolChecked(ini, "Inversion", "InvertPitch", invert_pitch);
    invert_roll = ReadBoolChecked(ini, "Inversion", "InvertRoll", invert_roll);

    local_smoothing = ReadFraction(ini, "Smoothing", "LocalSmoothing", local_smoothing);
    remote_smoothing = ReadFraction(ini, "Smoothing", "RemoteSmoothing", remote_smoothing);

    position_enabled = ReadBoolChecked(ini, "Position", "PositionEnabled", position_enabled);
    pos_sens_x = ReadSensitivity(ini, "Position", "PositionSensitivityX", pos_sens_x);
    pos_sens_y = ReadSensitivity(ini, "Position", "PositionSensitivityY", pos_sens_y);
    pos_sens_z = ReadSensitivity(ini, "Position", "PositionSensitivityZ", pos_sens_z);
    pos_limit_x = ReadPositionLimit(ini, "PositionLimitX", pos_limit_x);
    pos_limit_y = ReadPositionLimit(ini, "PositionLimitY", pos_limit_y);
    pos_limit_z = ReadPositionLimit(ini, "PositionLimitZ", pos_limit_z);
    pos_limit_z_back = ReadPositionLimit(ini, "PositionLimitZBack", pos_limit_z_back);

    collision_enabled = ReadBoolChecked(ini, "Position", "CollisionEnabled", collision_enabled);
    // The standoff is in Arx units, so its ceiling is centimetres rather than
    // the metres the position limits are in.
    // The floor is the engine's near clip, not zero. A standoff at or below it
    // stops the eye short of the wall and has the wall culled anyway, so the
    // player still sees through it while the log reports collision as on - the
    // exact complaint the clamp exists to answer, with an extra step.
    collision_radius = guards::ReadFloatChecked(ini, "Position", "CollisionRadius",
                                                collision_radius, kMinCollisionRadius,
                                                kMaxCollisionRadius, LogSink);
    collision_release_smoothing =
        ReadFraction(ini, "Position", "CollisionReleaseSmoothing", collision_release_smoothing);

    vk_toggle = ReadKeyChecked(ini, "ToggleKey", vk_toggle);
    vk_cycle_mode = ReadKeyChecked(ini, "CycleTrackingModeKey", vk_cycle_mode);

    return true;
}

}  // namespace ArxHeadTracking
