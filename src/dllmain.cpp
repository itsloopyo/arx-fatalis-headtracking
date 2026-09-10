// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "build_profile.h"
#include "camera_hook.h"
#include "config.h"
#include "cursor_hook.h"
#include "game_state.h"
#include "hotkeys.h"
#include "lean_trace.h"
#include "logging.h"
#include "path_utils.h"
#include "tracking_runtime.h"
#include "window_centering.h"

#include "cameraunlock/diagnostics/crash_handler.h"
#include "cameraunlock/memory/pe_fingerprint.h"

#include <windows.h>
#include <process.h>

#include <exception>
#include <string>

namespace {

using namespace ArxHeadTracking;

constexpr const char* kModName = "ArxFatalisHeadTracking";
constexpr const char* kModVersion = "0.0.0";
constexpr const char* kLogFile = "ArxFatalisHeadTracking.log";
constexpr const char* kIniFile = "ArxFatalisHeadTracking.ini";

constexpr int kInitMaxWaitMs = 30000;
constexpr int kInitPollMs = 100;
constexpr int kHeartbeatMs = 5000;
// Tracker state changes worth logging before the log stops being a record of the
// session and becomes a record of one flaky link.
constexpr int kMaxHeartbeatReports = 20;

// Deliberately never destroyed. As namespace-scope objects these would have
// non-trivial destructors, and the CRT runs those from DLL_PROCESS_DETACH -
// under the loader lock, joining the UDP receiver's threads and the hotkey
// poller's, which is exactly what PinSelf below says must never happen here.
// The OS reclaims the sockets, threads and handles at exit regardless.
TrackingRuntime& Tracking() {
    static TrackingRuntime* instance = new TrackingRuntime();
    return *instance;
}

Hotkeys& Input() {
    static Hotkeys* instance = new Hotkeys();
    return *instance;
}

// Whether GetModuleHandleExW pinned this module. Written from DllMain, read
// once the log exists.
bool g_pinned = false;

void LogFingerprint() {
    HMODULE exe = GetModuleHandleA(kGameExeName);
    cameraunlock::memory::PeFingerprint fp{};
    if (exe && cameraunlock::memory::ReadPeFingerprint(exe, fp)) {
        Log::Line("PE fingerprint: TimeDateStamp=0x%08X SizeOfImage=0x%08X CheckSum=0x%08X "
                  "base=0x%08X",
                  fp.TimeDateStamp, fp.SizeOfImage, fp.CheckSum,
                  static_cast<unsigned>(reinterpret_cast<uintptr_t>(exe)));
    } else {
        Log::Line("WARN: could not read the PE fingerprint of %s", kGameExeName);
    }
}

// Opens the session log next to this module. False when the module's own
// directory could not be resolved, which leaves nowhere to write at all. The
// fingerprint line follows separately, once the game module is known to exist.
bool OpenModLog() {
    const std::wstring logPath = GetModulePathW(kLogFile);
    if (logPath.empty()) {
        OutputDebugStringA("ArxFatalisHeadTracking: could not resolve the directory this mod "
                           "was loaded from; no log will be written\n");
        return false;
    }
    Log::Open(logPath);
    Log::Line("%s v%s attached to %s", kModName, kModVersion, kGameExeName);
    return true;
}

// The whole no-teardown design rests on the pin holding. Without it a
// FreeLibrary would unmap this image while its detours are still patched into
// the game, and the next frame calls into freed memory - the exact failure
// PinSelf exists to prevent. The log does not exist yet when the pin is taken,
// so the report waits for it.
void ReportPinResult() {
    if (g_pinned) return;
    Log::Line("WARN: this module could not be pinned in the process. Nothing is expected to "
              "unload it, but if something did, the camera hooks would outlive it and the game "
              "would crash on the next frame.");
}

void LogConfigSummary(const Config& cfg) {
    Log::Line("Config: port=%u enabled=%d smoothing=(local %.2f, remote %.2f) "
              "sens=(%.2f,%.2f,%.2f) position=%d crosshair=%d",
              cfg.udp_port, cfg.enabled_on_startup ? 1 : 0, cfg.local_smoothing,
              cfg.remote_smoothing, cfg.sens_yaw, cfg.sens_pitch, cfg.sens_roll,
              cfg.position_enabled ? 1 : 0, cfg.move_crosshair ? 1 : 0);
}

// The ASI loader can run us before the game module is mapped. False means it
// never appeared, so this is not the process we belong in.
bool WaitForGameModule() {
    for (int waited = 0; waited < kInitMaxWaitMs; waited += kInitPollMs) {
        if (GetModuleHandleA(kGameExeName)) return true;
        Sleep(kInitPollMs);
    }
    return GetModuleHandleA(kGameExeName) != nullptr;
}

// Reports every change in whether tracker packets are arriving. This is the log
// a player is asked for when head tracking does nothing in game.
void RunHeartbeat() {
    bool lastReceiving = false;
    bool firstReport = true;
    int reports = 0;
    for (;;) {
        const bool receiving = Tracking().IsReceiving();
        if (firstReport || receiving != lastReceiving) {
            ++reports;
            Log::Line("OpenTrack: %s%s", receiving ? "receiving data" : "no data",
                      reports == kMaxHeartbeatReports
                          ? " (further tracker state changes not logged)" : "");
            if (reports == kMaxHeartbeatReports) return;
            lastReceiving = receiving;
            firstReport = false;
        }
        Sleep(kHeartbeatMs);
    }
}

unsigned InitThreadBody() {
    // The log comes first. It needs only this module's own directory, which is
    // available immediately, and opening it after the wait meant that the one
    // case the wait exists for - the mod loaded beside something that is not
    // arx.exe - produced thirty seconds of silence and then no log file at all.
    // "It does nothing and there is no log to send" is the worst bug report
    // there is.
    if (!OpenModLog()) return 1;
    ReportPinResult();
    if (!WaitForGameModule()) {
        Log::Line("Staying dormant: %s never appeared in this process within %dms, so this is "
                  "not the process this mod belongs in.", kGameExeName, kInitMaxWaitMs);
        return 1;
    }

    Config cfg;
    if (!cfg.LoadOrCreate(GetModulePath(kIniFile).c_str())) {
        Log::Line("ERROR: config load failed");
        return 1;
    }
    LogFingerprint();
    LogConfigSummary(cfg);

    const BuildProfile* profile = MatchRunningProfile();
    if (!profile) {
        // No matching build: no hooks, no process modification, the game runs
        // vanilla. MatchRunningProfile has already logged which way it differs.
        return 0;
    }

    // After the match, not before. Dormant has to mean the process is left
    // exactly as it was found, and SetUnhandledExceptionFilter is a process-wide
    // change - a small one, since the handler chains to whatever was there, but
    // not one a component that has just declined to engage gets to make.
    cameraunlock::diagnostics::InstallCrashHandler();

    InitGameState(*profile);
    InitLeanTrace(*profile, cfg.collision_radius);

    Tracking().Start(cfg);

    if (!Input().Start(
            cfg,
            [] { Tracking().ToggleEnabled(); },
            [] { Tracking().CycleTrackingMode(); })) {
        Log::Line("ERROR: hotkeys failed to start");
        Tracking().Stop();
        return 1;
    }

    // The camera hook is the mod; without it there is nothing to run. Moving the
    // cursor is an enhancement to a working camera hook, so a failure there is
    // reported and the mod carries on.
    if (!InstallCameraHook(*profile, Tracking(), cfg)) {
        Log::Line("ERROR: camera hook install failed");
        Input().Stop();
        Tracking().Stop();
        return 1;
    }
    InstallCursorHook(*profile, cfg);

    Log::Line("%s ready", kModName);

    // Before the heartbeat, which runs for as long as it has tracker state
    // changes left to report. This blocks until the game has a window that has
    // stopped moving, so the first heartbeat line lands a few seconds later than
    // it otherwise would; the heartbeat reports the state it finds on its first
    // pass either way, so nothing is lost but the timestamp.
    CenterWindowWhenReady();

    RunHeartbeat();
    return 0;
}

// An exception escaping a bare __stdcall thread proc is std::terminate - the
// game dying outright with the log stopping mid-startup - and there are three
// places above that can throw: the UDP receiver and the hotkey poller each
// construct a std::thread, which throws std::system_error when the process
// cannot spawn one, and the config path allocates.
unsigned __stdcall InitThread(void*) {
    try {
        return InitThreadBody();
    } catch (const std::exception& e) {
        Log::Line("ERROR: startup failed with an exception: %s", e.what());
        return 1;
    } catch (...) {
        Log::Line("ERROR: startup failed with an unknown exception");
        return 1;
    }
}

// Makes FreeLibrary on this module a no-op, which is what lets
// DLL_PROCESS_DETACH do nothing at all.
//
// There is no safe teardown from DllMain. Both detach cases hold the loader
// lock, and everything a teardown would have to do is forbidden while holding
// it: joining the hotkey poller and the UDP receiver threads deadlocks, and
// MinHook's unhook suspends every other thread while we hold a lock some of them
// may be waiting on. Meanwhile the detour bodies live in this image, so an
// in-flight render thread would be executing code that is about to be unmapped.
void PinSelf(HMODULE self) {
    HMODULE pinned = nullptr;
    g_pinned = GetModuleHandleExW(
                   GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                   reinterpret_cast<LPCWSTR>(self), &pinned) != FALSE;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID lpReserved) {
    UNREFERENCED_PARAMETER(lpReserved);
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            DisableThreadLibraryCalls(hModule);
            PinSelf(hModule);
            // The thread cannot run until DllMain returns and the loader lock is
            // released, which is exactly why the real work goes on it.
            const HANDLE thread = reinterpret_cast<HANDLE>(
                _beginthreadex(nullptr, 0, InitThread, nullptr, 0, nullptr));
            if (thread) {
                CloseHandle(thread);
            } else {
                OutputDebugStringA("ArxFatalisHeadTracking: init thread could not start\n");
            }
            break;
        }
        case DLL_PROCESS_DETACH:
            // Nothing. See PinSelf.
            break;
    }
    return TRUE;
}
