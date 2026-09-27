// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "legacy_config/legacy_config.h"

#include "logging.h"

#include "cameraunlock/config/ini_reader.h"
#include "cameraunlock/config/value_guards.h"

#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>

namespace ArxHeadTracking::legacy {

namespace {

using cameraunlock::IniReader;
namespace guards = cameraunlock::config;

constexpr int kMinUdpPort = 1024;
constexpr int kMaxUdpPort = 65535;
constexpr float kMaxCollisionRadius = 200.0f;
constexpr float kMinCollisionRadius = 2.0f;
constexpr float kMinFieldOfView = 40.0f;
constexpr float kMaxFieldOfView = 110.0f;

void LogSink(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buffer[512];
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    Log::Line("%s", buffer);
}

float ReadSensitivity(const IniReader& ini, const char* section, const char* key,
                      float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, -guards::kMaxSensitivity,
                                    guards::kMaxSensitivity, LogSink);
}

float ReadPositionLimit(const IniReader& ini, const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, "Position", key, fallback, 0.0f,
                                    guards::kMaxPositionLimit, LogSink);
}

float ReadFraction(const IniReader& ini, const char* section, const char* key, float fallback) {
    return guards::ReadFloatChecked(ini, section, key, fallback, 0.0f, 1.0f, LogSink);
}

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

}  // namespace

ReadStatus Read(const char* path, Config& c) {
    IniReader ini;
    if (!ini.Open(path)) {
        return ReadStatus::Absent;
    }

    const int port = ini.ReadInt("Network", "Port", c.udp_port);
    if (port < kMinUdpPort || port > kMaxUdpPort) {
        Log::Line("Port=%d is outside %d-%d; keeping %u.", port, kMinUdpPort, kMaxUdpPort,
                  c.udp_port);
    } else {
        c.udp_port = static_cast<uint16_t>(port);
    }

    c.enabled_on_startup = ReadBoolChecked(ini, "General", "EnableOnStartup", c.enabled_on_startup);
    c.move_crosshair = ReadBoolChecked(ini, "General", "MoveCrosshair", c.move_crosshair);
    c.diagnostics = ReadBoolChecked(ini, "General", "Diagnostics", c.diagnostics);

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
            c.field_of_view = fov;
        }
    }

    c.sens_yaw = ReadSensitivity(ini, "Sensitivity", "YawSensitivity", c.sens_yaw);
    c.sens_pitch = ReadSensitivity(ini, "Sensitivity", "PitchSensitivity", c.sens_pitch);
    c.sens_roll = ReadSensitivity(ini, "Sensitivity", "RollSensitivity", c.sens_roll);

    c.invert_yaw = ReadBoolChecked(ini, "Inversion", "InvertYaw", c.invert_yaw);
    c.invert_pitch = ReadBoolChecked(ini, "Inversion", "InvertPitch", c.invert_pitch);
    c.invert_roll = ReadBoolChecked(ini, "Inversion", "InvertRoll", c.invert_roll);

    c.local_smoothing = ReadFraction(ini, "Smoothing", "LocalSmoothing", c.local_smoothing);
    c.remote_smoothing = ReadFraction(ini, "Smoothing", "RemoteSmoothing", c.remote_smoothing);

    c.position_enabled = ReadBoolChecked(ini, "Position", "PositionEnabled", c.position_enabled);
    c.pos_sens_x = ReadSensitivity(ini, "Position", "PositionSensitivityX", c.pos_sens_x);
    c.pos_sens_y = ReadSensitivity(ini, "Position", "PositionSensitivityY", c.pos_sens_y);
    c.pos_sens_z = ReadSensitivity(ini, "Position", "PositionSensitivityZ", c.pos_sens_z);
    c.pos_limit_x = ReadPositionLimit(ini, "PositionLimitX", c.pos_limit_x);
    c.pos_limit_y = ReadPositionLimit(ini, "PositionLimitY", c.pos_limit_y);
    c.pos_limit_z = ReadPositionLimit(ini, "PositionLimitZ", c.pos_limit_z);
    c.pos_limit_z_back = ReadPositionLimit(ini, "PositionLimitZBack", c.pos_limit_z_back);

    c.collision_enabled = ReadBoolChecked(ini, "Position", "CollisionEnabled", c.collision_enabled);
    c.collision_radius = guards::ReadFloatChecked(ini, "Position", "CollisionRadius",
                                                  c.collision_radius, kMinCollisionRadius,
                                                  kMaxCollisionRadius, LogSink);
    c.collision_release_smoothing =
        ReadFraction(ini, "Position", "CollisionReleaseSmoothing", c.collision_release_smoothing);

    c.vk_toggle = ReadKeyChecked(ini, "ToggleKey", c.vk_toggle);
    c.vk_cycle_mode = ReadKeyChecked(ini, "CycleTrackingModeKey", c.vk_cycle_mode);

    return ReadStatus::Read;
}

std::vector<cameraunlock::config::LegacyKey> ReadKeys() {
    return {
        {"Network", "Port"},
        {"General", "EnableOnStartup"},
        {"General", "MoveCrosshair"},
        {"General", "Diagnostics"},
        {"General", "FieldOfView"},
        {"Sensitivity", "YawSensitivity"},
        {"Sensitivity", "PitchSensitivity"},
        {"Sensitivity", "RollSensitivity"},
        {"Inversion", "InvertYaw"},
        {"Inversion", "InvertPitch"},
        {"Inversion", "InvertRoll"},
        {"Smoothing", "LocalSmoothing"},
        {"Smoothing", "RemoteSmoothing"},
        {"Position", "PositionEnabled"},
        {"Position", "PositionSensitivityX"},
        {"Position", "PositionSensitivityY"},
        {"Position", "PositionSensitivityZ"},
        {"Position", "PositionLimitX"},
        {"Position", "PositionLimitY"},
        {"Position", "PositionLimitZ"},
        {"Position", "PositionLimitZBack"},
        {"Position", "CollisionEnabled"},
        {"Position", "CollisionRadius"},
        {"Position", "CollisionReleaseSmoothing"},
        {"Hotkeys", "ToggleKey"},
        {"Hotkeys", "CycleTrackingModeKey"},
    };
}

}  // namespace ArxHeadTracking::legacy
