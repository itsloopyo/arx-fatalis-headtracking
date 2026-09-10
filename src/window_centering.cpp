// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "window_centering.h"

#include "logging.h"

#include "cameraunlock/os/game_window.h"

#include <cstdlib>

#include <windows.h>

namespace ArxHeadTracking {

namespace {

namespace os = cameraunlock::os;

constexpr int kPollIntervalMs = 250;
constexpr int kPollAttempts = 240;  // 60s, which covers a cold start off a hard disk.

// The rect has to hold still before it is worth acting on. The window is up
// well before the engine has finished sizing and placing it, and this mod has
// not measured when the game stops moving it, so the wait is on three seconds
// of an unchanged rect rather than on a fixed delay that would be a guess.
constexpr int kSettlePolls = 12;

void ForwardWindowLog(os::WindowLogLevel level, const char* message) {
    Log::Line("%s%s", level == os::WindowLogLevel::Warning ? "WARN: " : "", message);
}

int CenteredOrigin(int areaStart, int areaExtent, int windowExtent) {
    return areaStart + (areaExtent - windowExtent) / 2;
}

bool IsCenteredOn(const RECT& window, const RECT& area) {
    // A game that centres its own window rounds the odd half-pixel up where the
    // integer maths here rounds it down, so an exact comparison would move the
    // window one pixel and report that as a fix.
    constexpr int kTolerance = 2;
    const int dx = window.left -
                   CenteredOrigin(area.left, area.right - area.left, window.right - window.left);
    const int dy = window.top -
                   CenteredOrigin(area.top, area.bottom - area.top, window.bottom - window.top);
    return std::abs(dx) <= kTolerance && std::abs(dy) <= kTolerance;
}

void CenterUnlessAlready(HWND window, const RECT& rect) {
    MONITORINFO info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &info)) {
        Log::Line("WARN: window: GetMonitorInfoW failed: %lu", GetLastError());
        return;
    }

    // Either reading counts as centred. A game centres on the monitor, this mod
    // centres on the work area, and the two differ by half the taskbar. Moving a
    // window that is already centred trades a visible jump for nothing, and a
    // fullscreen or borderless window is centred by definition, which is how it
    // is left alone here.
    if (IsCenteredOn(rect, info.rcWork) || IsCenteredOn(rect, info.rcMonitor)) {
        Log::Line("window: %dx%d at (%d, %d) is already centred, leaving it alone",
                  static_cast<int>(rect.right - rect.left),
                  static_cast<int>(rect.bottom - rect.top), static_cast<int>(rect.left),
                  static_cast<int>(rect.top));
        return;
    }

    os::CenterGameWindowOnce(&ForwardWindowLog);
}

// Waits for a game window whose rect has held still for kSettlePolls, writing it
// and that rect to the out-params. False when none settled in time, in which
// case the out-params are left alone.
bool WaitForSettledWindow(HWND& settled, RECT& settledRect) {
    RECT previous{};
    bool havePrevious = false;
    int stablePolls = 0;

    for (int attempt = 0; attempt < kPollAttempts; ++attempt) {
        Sleep(kPollIntervalMs);

        const HWND window = os::FindGameWindow();
        RECT current{};
        if (!window || !GetWindowRect(window, &current)) {
            havePrevious = false;
            stablePolls = 0;
            continue;
        }

        if (havePrevious && EqualRect(&previous, &current)) {
            if (++stablePolls < kSettlePolls) continue;
            settled = window;
            settledRect = current;
            return true;
        }
        previous = current;
        havePrevious = true;
        stablePolls = 0;
    }
    return false;
}

}  // namespace

void CenterWindowWhenReady() {
    HWND window = nullptr;
    RECT rect{};
    if (!WaitForSettledWindow(window, rect)) {
        Log::Line("window: no window settled within %ds, leaving placement alone",
                  kPollAttempts * kPollIntervalMs / 1000);
        return;
    }
    CenterUnlessAlready(window, rect);
}

}  // namespace ArxHeadTracking
