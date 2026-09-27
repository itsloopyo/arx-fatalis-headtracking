// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "hotkeys.h"

#include "logging.h"

#include "cameraunlock/input/chord_hotkeys.h"

#include <exception>

namespace ArxHeadTracking {

namespace {
// ~60Hz: fast enough that a deliberate press is never missed, slow enough to
// cost nothing.
constexpr int kPollIntervalMs = 16;
}  // namespace

bool Hotkeys::Start(const Config& cfg, Action onToggle, Action onCycleMode) {
    if (m_started) return true;

    using cameraunlock::input::ChordGuarded;
    using cameraunlock::input::NavGuarded;

    // The nav keys are suppressed while Ctrl+Shift is held so a Ctrl+Shift+<nav>
    // press cannot fire the same action through both paths.
    m_poller.SetToggleKey(cfg.vk_toggle, NavGuarded(onToggle));
    m_poller.AddHotkey(cfg.vk_cycle_mode, NavGuarded(onCycleMode));

    // Both binding sets are always live. The chords are not an alternative the
    // player opts into; they are the same actions reached from a keyboard with
    // no nav cluster.
    //
    // The cycle chord is J, not the fleet's usual G. Arx binds G to drinking a
    // mana potion, H to a health potion and T to the torch, and it binds Ctrl to
    // magic mode and Shift to stealth mode - so Ctrl+Shift+G would have cycled
    // the tracking mode AND swallowed a potion on every press. The doctrine's
    // answer to a game that binds one of these is to take the next free letter
    // from the same cluster rather than change the modifier, and J is the one
    // Arx leaves alone.
    m_poller.AddHotkey('Y', ChordGuarded(std::move(onToggle)));
    m_poller.AddHotkey('J', ChordGuarded(std::move(onCycleMode)));

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

    Log::Line("Hotkeys: toggle=0x%02X or Ctrl+Shift+Y, cyclemode=0x%02X or Ctrl+Shift+J. "
              "No recenter key - centre in the tracker.",
              cfg.vk_toggle, cfg.vk_cycle_mode);
    m_started = true;
    return true;
}

void Hotkeys::Stop() {
    if (!m_started) return;
    m_poller.Stop();
    m_started = false;
}

}  // namespace ArxHeadTracking
