// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#include "tracking_runtime.h"

#include "logging.h"

#include "cameraunlock/tracking/tracking_mode.h"

#include <cmath>

namespace ArxHeadTracking {

namespace {

// Distinct reasons a frame can produce no pose. One slot per branch in
// SampleFrame, so the table cannot fill before every reason has had its line.
constexpr int kMaxEmptyFrameReasons = 3;

bool AllFinite(float a, float b, float c) {
    return std::isfinite(a) && std::isfinite(b) && std::isfinite(c);
}

// The packet parser rejects a non-finite datagram, so nothing the pipeline does
// after it should produce one; if it does, it must not reach the camera angles.
// Drop the channel and say so once - a silently dropped pose reads in game
// exactly like a tracker that has stopped sending.
void ReportNonFinite(const char* channel) {
    static bool s_warned = false;
    if (s_warned) return;
    s_warned = true;
    Log::Line("WARN: the processed %s came out non-finite and is being dropped.", channel);
}

// Names the reason a frame produced no pose, once per distinct reason. Compared
// by POINTER: every caller passes a string literal, so identity is the cheapest
// way to say "this reason has already been reported".
void ReportEmptyFrame(const char* why) {
    static const char* s_seen[kMaxEmptyFrameReasons] = {};
    static int s_count = 0;
    for (int i = 0; i < s_count; ++i) {
        if (s_seen[i] == why) return;
    }
    if (s_count == kMaxEmptyFrameReasons) return;
    s_seen[s_count++] = why;
    Log::Line("Tracking produced no pose: %s", why);
}

}  // namespace

void TrackingRuntime::ConfigurePosition() {
    // The limits come from the config; the sensitivities and inversions stay at
    // PositionSettings' identity, because the tracker shapes the pose.
    m_session.SetPositionSettings(m_cfg.position);
}

void TrackingRuntime::ConfigureSmoothing() {
    // The session forwards both values to the rotation AND position processors
    // and re-reads the receiver's connection locality inside every Update() to
    // pick the one that applies. Without IsRemoteConnection() on the receiver
    // that selection silently pins to local, so assert the trait.
    static_assert(decltype(m_session)::kHasRemoteConnection,
                  "receiver must expose IsRemoteConnection()");
    m_session.SetLocalSmoothing(m_cfg.local_smoothing);
    m_session.SetRemoteSmoothing(m_cfg.remote_smoothing);
}

void TrackingRuntime::Start(const Config& cfg) {
    m_cfg = cfg;

    ConfigurePosition();
    ConfigureSmoothing();

    m_enabled.store(m_cfg.enable_on_startup, std::memory_order_relaxed);
    // The table reads a pair that names no mode as its defaults, so the pair always decodes.
    const cameraunlock::TrackingMode mode =
        cameraunlock::DecodeTrackingMode(m_cfg.rotation_enabled, m_cfg.position_enabled).value();
    m_session.SetMode(mode);
    m_desiredMode.store(mode, std::memory_order_relaxed);

    m_receiver.SetLog([](const std::string& msg) { Log::Line("UDP: %s", msg.c_str()); });

    if (m_receiver.Start(m_cfg.udp_port)) {
        Log::Line("UDP receiver listening on port %u", m_cfg.udp_port);
    } else {
        Log::Line("WARN: UDP receiver did not bind immediately on port %u; background retry "
                  "active", m_cfg.udp_port);
    }
}

void TrackingRuntime::Stop() {
    m_receiver.Stop();
}

void TrackingRuntime::ToggleEnabled() {
    const bool prev = m_enabled.load(std::memory_order_relaxed);
    m_enabled.store(!prev, std::memory_order_relaxed);
    Log::Line("Tracking %s", !prev ? "enabled" : "disabled");
}

cameraunlock::TrackingMode TrackingRuntime::CycleTrackingMode() {
    const cameraunlock::TrackingMode mode = static_cast<cameraunlock::TrackingMode>(
        (static_cast<int>(m_session.GetMode()) + 1) % 3);
    m_desiredMode.store(mode, std::memory_order_relaxed);
    switch (mode) {
        case cameraunlock::TrackingMode::RotationAndPosition:
            Log::Line("Tracking mode: rotation + position (6DOF)");
            break;
        case cameraunlock::TrackingMode::RotationOnly:
            Log::Line("Tracking mode: rotation only");
            break;
        case cameraunlock::TrackingMode::PositionOnly:
            Log::Line("Tracking mode: position only");
            break;
    }
    return mode;
}

FrameSample TrackingRuntime::SampleFrame() {
    FrameSample out;
    // Ticked unconditionally: the clock measures frames, not tracked frames, so
    // skipping it on a gated frame would hand the next live frame the whole
    // gated interval as one delta.
    out.delta_time = m_clock.Tick();

    const cameraunlock::TrackingMode desired = m_desiredMode.load(std::memory_order_relaxed);
    if (desired != m_session.GetMode()) m_session.SetMode(desired);

    if (!m_enabled.load(std::memory_order_relaxed)) {
        ReportEmptyFrame("tracking is switched off");
        return out;
    }
    // Deliberately NOT gated on IsReceiving(). That is a 500ms liveness test, and
    // returning an empty frame when it goes false snaps the view to centre in a
    // single frame every time a webcam tracker loses the face - a hand passing
    // in front of it, someone walking behind, a light changing. Tracking loss
    // holds the last known pose instead: the receiver keeps reporting it, so
    // Update() keeps succeeding and the view simply stops moving, and smoothing
    // blends back in when the tracker re-acquires. Before the FIRST packet the
    // receiver has no pose at all and Update() returns false, which the branch
    // below reports. The heartbeat in dllmain.cpp is what logs the link
    // dropping.
    if (!m_session.Update(out.delta_time)) {
        ReportEmptyFrame("the session had no fresh sample to process this frame");
        return out;
    }

    // GetRotation reports success in PositionOnly mode too, handing back zeros.
    // Taking that as a rotation channel would run the rotation path with no head
    // input at all, which still rewrites the camera angles - the mod moving the
    // view while the player had rotation switched off.
    out.has_rotation = m_session.IsRotationActive() &&
                       m_session.GetRotation(out.yaw, out.pitch, out.roll);
    if (out.has_rotation && !AllFinite(out.yaw, out.pitch, out.roll)) {
        out.has_rotation = false;
        ReportNonFinite("head rotation");
    }

    out.has_position = m_session.GetPositionOffset(out.pos_x, out.pos_y, out.pos_z);
    if (out.has_position && !AllFinite(out.pos_x, out.pos_y, out.pos_z)) {
        out.has_position = false;
        ReportNonFinite("head position");
    }
    if (!out.has_rotation && !out.has_position) {
        ReportEmptyFrame("the processor returned neither a rotation nor a position");
    }
    return out;
}

}  // namespace ArxHeadTracking
