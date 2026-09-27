// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "hotkeys.h"

#include "logging.h"

#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"

#include <exception>
#include <stdexcept>
#include <string>

namespace ArxHeadTracking {

namespace {
// ~60Hz: fast enough that a deliberate press is never missed, slow enough to
// cost nothing.
constexpr int kPollIntervalMs = 16;

// The table read every list through the hotkey codec, so a list that does not parse here is a
// bug, not a player's typo.
void Register(cameraunlock::input::HotkeyPoller& poller, const std::string& list, const char* key,
              std::function<void()> action) {
    const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(list);
    if (!parsed.ok()) {
        throw std::logic_error(std::string("[Hotkeys] ") + key + "=" + list + " does not parse: " + parsed.error);
    }
    cameraunlock::input::RegisterKeyBindings(poller, parsed.bindings, std::move(action));
}

}  // namespace

bool Hotkeys::Start(const Config& cfg, Action onToggle, Action onCycleMode) {
    if (m_started) return true;

    // A binding without modifiers does not fire while Ctrl and Shift are both
    // held, so a Ctrl+Shift+<nav> press cannot fire an action through both its
    // nav key and its chord.
    Register(m_poller, cfg.toggle_key_name, "ToggleKey", std::move(onToggle));
    Register(m_poller, cfg.cycle_tracking_mode_key_name, "CycleTrackingModeKey", std::move(onCycleMode));

    // The poller rethrows std::system_error when the process cannot spawn its
    // thread. The caller runs on a bare thread procedure with no handler above
    // it, where an escaping exception is std::terminate.
    bool started = false;
    try {
        started = m_poller.Start(kPollIntervalMs);
    } catch (const std::exception& e) {
        Log::Line("ERROR: HotkeyPoller could not start its thread: %s", e.what());
        return false;
    }
    if (!started) {
        Log::Line("ERROR: HotkeyPoller failed to start");
        return false;
    }

    Log::Line("Hotkeys: toggle=%s, cycle mode=%s. No recenter key - centre in the tracker.",
              cfg.toggle_key_name.c_str(), cfg.cycle_tracking_mode_key_name.c_str());
    m_started = true;
    return true;
}

void Hotkeys::Stop() {
    if (!m_started) return;
    m_poller.Stop();
    m_started = false;
}

}  // namespace ArxHeadTracking
