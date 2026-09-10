// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo

#pragma once

#include "config.h"

#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/time/frame_clock.h"
#include "cameraunlock/tracking/head_tracking_session.h"

#include <atomic>

namespace ArxHeadTracking {

// One frame's processed head pose: rotation in degrees (YPR) and position offset
// in metres, in the CORE's basis - x right, y up, and NEGATIVE z the forward
// lean. That z sign is what puts the generous LimitZ on leaning in and the
// restricted LimitZBack on pulling away, and it is why camera_hook.cpp converts
// the signs at the engine boundary rather than here.
struct FrameSample {
    // Seconds since the previous rendered frame, clamped. Carried with the pose
    // because the lean clamp's release ease needs the same dt the pipeline ran
    // on, and a second clock would drift from it.
    float delta_time = 0.0f;
    bool has_rotation = false;
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    bool has_position = false;
    float pos_x = 0.0f, pos_y = 0.0f, pos_z = 0.0f;
};

class TrackingRuntime {
public:
    TrackingRuntime() : m_session(m_receiver) {}

    void Start(const Config& cfg);
    void Stop();

    // Runs the per-frame pipeline once and returns the processed pose. Called
    // from the DANAE::Render detour on the game thread and nowhere else: the
    // frame clock and the session are stateful and not reentrant, and that
    // restriction is what keeps them single-threaded.
    FrameSample SampleFrame();

    bool IsReceiving() const { return m_receiver.IsReceiving(); }

    void ToggleEnabled();
    void CycleTrackingMode();

private:
    // A frame longer than this is a stall - a load, a save, an alt-tab - and
    // feeding its real duration to the smoothing would snap the view.
    static constexpr float kMaxFrameDtSec = 0.25f;

    void ConfigureRotation();
    void ConfigurePosition();
    void ConfigureSmoothing();

    Config m_cfg{};
    cameraunlock::UdpReceiver m_receiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session;
    cameraunlock::time::FrameClock m_clock{kMaxFrameDtSec};

    std::atomic<bool> m_enabled{false};
};

}  // namespace ArxHeadTracking
