// SPDX-License-Identifier: MIT
// Copyright (c) 2026 itsloopyo
//
// The pure logic the hooks inject through, exercised without a game.
//
// Each of these is a place a wrong answer is invisible in play: the engine
// boundary where the tracker's conventions become Arx's, the projection the
// cursor is placed with, the focal-to-field-of-view map that the zoom factor and
// a chosen field of view are both built on, and the INI reader with its
// sanitizers. Everything here is a behaviour lock - the assertions record what
// the shipped code does, so a restructure that changes an answer fails rather
// than ships.

#include "arx_game.h"
#include "build_profile.h"
#include "config.h"
#include "engine_boundary.h"
#include "roll_math.h"
#include "lean_trace.h"

#include <windows.h>

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

namespace {

using namespace ArxHeadTracking;

int g_failures = 0;

void Check(bool ok, const char* what) {
    if (!ok) {
        ++g_failures;
        std::printf("FAIL: %s\n", what);
    }
}

void CheckNear(float actual, float expected, const char* what, float tolerance = 1e-3f) {
    if (!(std::fabs(actual - expected) <= tolerance)) {
        ++g_failures;
        std::printf("FAIL: %s (expected %g, got %g)\n", what, expected, actual);
    }
}

FrameSample Pose(float yaw, float pitch, float x, float y, float z) {
    FrameSample s;
    s.has_rotation = true;
    s.yaw = yaw;
    s.pitch = pitch;
    s.has_position = true;
    s.pos_x = x;
    s.pos_y = y;
    s.pos_z = z;
    return s;
}

// ---------------------------------------------------------------------------
void TestRotationSigns() {
    const EngineRotation left = ToEngineRotation(Pose(25.0f, 0.0f, 0, 0, 0), 1.0f);
    CheckNear(left.dYaw, -25.0f, "tracker yaw inverts at the engine boundary");
    CheckNear(left.dPitch, 0.0f, "yaw does not leak into pitch");

    const EngineRotation up = ToEngineRotation(Pose(0.0f, 20.0f, 0, 0, 0), 1.0f);
    CheckNear(up.dPitch, -20.0f, "looking up subtracts from angle.a");
    CheckNear(up.dYaw, 0.0f, "pitch does not leak into yaw");

    FrameSample tilted = Pose(0, 0, 0, 0, 0);
    tilted.roll = 30.0f;
    CheckNear(ToEngineRotation(tilted, 0.5f).dRoll, -30.0f,
              "tracker roll inverts at the engine boundary, one-to-one through zoom");
}

void TestRollProjection() {
    float x = 700.0f;
    float y = 300.0f;
    ViewRoll(90.0f).ApplyScreen(x, y, 400.0f, 300.0f);
    CheckNear(x, 400.0f, "a quarter roll rotates around the viewport centre");
    CheckNear(y, 0.0f, "positive roll moves the right edge upward");
    ViewRoll(-90.0f).ApplyScreen(x, y, 400.0f, 300.0f);
    CheckNear(x, 700.0f, "opposite roll restores horizontal projection");
    CheckNear(y, 300.0f, "opposite roll restores vertical projection");

    for (float degrees : {-45.0f, 0.0f, 30.0f, 90.0f}) {
        const ViewRoll roll(degrees);
        float matrix[16] = {1, 0, 0, 0, 0, 1, 0, 0,
                            0, 0, 1, 0, -10, 20, -30, 1};
        roll.ApplyViewMatrix(matrix);
        float screenX = 90.0f;
        float screenY = -220.0f;
        roll.Apply(screenX, screenY);
        CheckNear(100 * matrix[0] + 200 * matrix[4] + matrix[12], screenX,
                  "culling and screen roll agree after eye translation", 0.001f);
        CheckNear(-(100 * matrix[1] + 200 * matrix[5] + matrix[13]), screenY,
                  "culling compensates the upward-positive D3D y axis", 0.001f);
        CheckNear(matrix[10], 1.0f, "roll preserves the depth axis");
        CheckNear(matrix[14], -30.0f, "roll preserves eye depth");
    }
}

// A narrowed field of view magnifies everything in the frame, so the pose has to
// shrink by the ratio of the tangents to keep the picture moving by the same
// amount. The round trip is through tangents rather than a plain multiply, so a
// large angle stays honest.
void TestZoomScaling() {
    CheckNear(MakeZoomBasis(kReferenceFocal, kReferenceFocal).factor, 1.0f,
              "the un-zoomed field of view scales the pose by exactly 1", 1e-6f);

    // A fully drawn bow: BASE_FOCAL is 310 + BOW_FOCAL/4 and BOW_FOCAL tops out
    // at 710, so the focal reaches 487.5 and the vertical FOV narrows from 75.95
    // degrees to 52.38.
    const float drawn = MakeZoomBasis(487.5f, kReferenceFocal).factor;
    Check(drawn > 0.6f && drawn < 0.65f, "a fully drawn bow shrinks the pose to about 0.63");

    const EngineRotation r = ToEngineRotation(Pose(20.0f, 20.0f, 0, 0, 0), drawn);
    Check(std::fabs(r.dYaw) < 20.0f, "yaw shrinks with the zoom");
    Check(std::fabs(r.dPitch) < 20.0f, "pitch shrinks with the zoom");
    CheckNear(std::tan(r.dYaw * kDegToRad), std::tan(-20.0f * kDegToRad) * drawn,
              "the scaled yaw holds the tangent ratio");
}

// ---------------------------------------------------------------------------
// The position boundary. Core hands out metres with x right, y up and NEGATIVE z
// forward; Arx wants centimetres with x right, y DOWN and z forward, and the
// tracker's x is mirrored the same way its yaw is.
void TestPositionSigns() {
    float out[3];

    // Facing +z (yaw 0), so right is +x and forward is +z.
    ToEngineOffset(Pose(0, 0, 0.30f, 0.0f, 0.0f), 0.0f, 1.0f, out);
    CheckNear(out[0], -30.0f, "a tracker x of +0.30 m leans 30 units LEFT");
    CheckNear(out[1], 0.0f, "x does not leak into the vertical");
    CheckNear(out[2], 0.0f, "x does not leak into forward");

    ToEngineOffset(Pose(0, 0, 0.0f, 0.20f, 0.0f), 0.0f, 1.0f, out);
    CheckNear(out[1], -20.0f, "head up is negative y, because Arx y grows downward");

    ToEngineOffset(Pose(0, 0, 0.0f, 0.0f, -0.40f), 0.0f, 1.0f, out);
    CheckNear(out[2], 40.0f, "core's negative z is a forward lean, which is +z in Arx");

    ToEngineOffset(Pose(0, 0, 0.0f, 0.0f, 0.10f), 0.0f, 1.0f, out);
    CheckNear(out[2], -10.0f, "core's positive z pulls back");
}

// The lean rides the body's facing, not the camera's, so turning the player
// turns the offset with them and nothing else changes.
void TestPositionFollowsFacing() {
    float straight[3];
    float turned[3];
    ToEngineOffset(Pose(0, 0, 0.0f, 0.0f, -0.40f), 0.0f, 1.0f, straight);
    ToEngineOffset(Pose(0, 0, 0.0f, 0.0f, -0.40f), 90.0f, 1.0f, turned);

    CheckNear(straight[2], 40.0f, "facing zero, a forward lean is +z");
    CheckNear(turned[0], -40.0f, "facing 90 degrees, the same lean is -x");
    CheckNear(turned[2], 0.0f, "and nothing is left on z");

    const float lenA = std::sqrt(straight[0] * straight[0] + straight[2] * straight[2]);
    const float lenB = std::sqrt(turned[0] * turned[0] + turned[2] * turned[2]);
    CheckNear(lenA, lenB, "turning does not change how far the head leans");
}

// ---------------------------------------------------------------------------
// The projection. It reproduces the engine's own transform, and the check that
// matters is the one the game confirmed: the point the clean camera is aimed at
// lands exactly on the screen centre when the head is at rest, and moves by
// scale * lean / depth when it is not.
EerieCamera MakeCamera(float pitch, float yaw, float px, float py, float pz,
                       float centreX, float centreY) {
    EerieCamera cam{};
    cam.angle_a = pitch;
    cam.angle_b = yaw;
    cam.pos_x = px;
    cam.pos_y = py;
    cam.pos_z = pz;
    cam.Xcos = std::cos(pitch * kDegToRad);
    cam.Xsin = std::sin(pitch * kDegToRad);
    cam.Ycos = std::cos(yaw * kDegToRad);
    cam.Ysin = std::sin(yaw * kDegToRad);
    cam.posleft = centreX;
    cam.postop = centreY;
    return cam;
}

void TestProjection() {
    const float scale = ProjectionScale(kReferenceFocal, 739);
    CheckNear(scale, 473.36f, "the projection scale matches the engine's ProjectionMatrix._22",
              0.05f);

    const EerieCamera clean = MakeCamera(0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 509.0f, 369.0f);
    const Eerie3D aim{0.0f, 0.0f, 1278.0f};
    ScreenPoint sp{};
    Check(ProjectWorldPoint(clean, scale, aim, sp), "a point ahead projects");
    CheckNear(sp.x, 509.0f, "with the head at rest the aim point is at the screen centre");
    CheckNear(sp.y, 369.0f, "vertically too");
    CheckNear(sp.depth, 1278.0f, "and the depth is the distance to it");

    // The same aim point seen from an eye leaned 30 units left. Measured in game:
    // 509.1 -> 520.2 at this depth.
    const EerieCamera leaned = MakeCamera(0.0f, 0.0f, -30.0f, 0.0f, 0.0f, 509.0f, 369.0f);
    Check(ProjectWorldPoint(leaned, scale, aim, sp), "the leaned eye projects too");
    CheckNear(sp.x, 509.0f + scale * 30.0f / 1278.0f,
              "the mark moves by scale * lean / depth", 0.05f);

    // Half the distance, twice the shift. This is the near/far gate: a fixed or
    // stale depth would move the mark by the same amount at both.
    const Eerie3D nearPoint{0.0f, 0.0f, 639.0f};
    ScreenPoint spNear{};
    Check(ProjectWorldPoint(leaned, scale, nearPoint, spNear), "a nearer point projects");
    CheckNear(spNear.x - 509.0f, (sp.x - 509.0f) * 2.0f,
              "halving the distance doubles the shift", 0.05f);

    // Behind the eye there is no screen position to give.
    const Eerie3D behind{0.0f, 0.0f, -100.0f};
    Check(!ProjectWorldPoint(clean, scale, behind, sp),
          "a point behind the camera has no screen position");
}

void TestForwardVector() {
    // The engine builds this same vector for its sound listener, so the signs are
    // not ours to choose.
    const Eerie3D f = ForwardFromAngles(0.0f, 0.0f);
    CheckNear(f.x, 0.0f, "facing zero, forward has no x");
    CheckNear(f.z, 1.0f, "facing zero, forward is +z");

    const Eerie3D left = ForwardFromAngles(0.0f, 90.0f);
    CheckNear(left.x, -1.0f, "yaw 90 faces -x, which is why angle.b grows turning left");

    const Eerie3D down = ForwardFromAngles(45.0f, 0.0f);
    Check(down.y > 0.0f, "a positive angle.a looks DOWN, because Arx y grows downward");
}

// ---------------------------------------------------------------------------
// The focal-to-field-of-view map, reproduced from the engine because the mod
// needs the FOV of a focal the game is not currently rendering.
//
// The expected values are the shipped executable's own, read out of the
// function at 0x005487A0 rather than out of the published source: the binary
// gives focal 700 to 800 its own line and flattens everything past 800, which
// the source does not, and a chosen field of view reaches both.
void TestFocalToFov() {
    CheckNear(FocalToFovDegrees(310.0f), 75.95f, "the un-zoomed focal is 75.95 degrees", 0.01f);
    CheckNear(FocalToFovDegrees(487.5f), 52.375f, "a drawn bow narrows to 52.4", 0.01f);
    CheckNear(FocalToFovDegrees(320.0f), 74.4f, "Magic Sight widens slightly", 0.01f);
    Check(FocalToFovDegrees(200.0f) > FocalToFovDegrees(400.0f),
          "a smaller focal is always a wider field of view");

    // Every line meets the next, so the map is continuous everywhere except the
    // documented two-degree step at focal 300.
    CheckNear(FocalToFovDegrees(700.0f), 38.0f, "the 700 line starts where the 600 one ends",
              0.01f);
    CheckNear(FocalToFovDegrees(750.0f), 35.75f, "and runs on past it", 0.01f);
    CheckNear(FocalToFovDegrees(900.0f), 33.5f, "past focal 800 the map is flat at 33.5", 0.01f);
    CheckNear(FocalToFovDegrees(800.0f), 33.5f, "which is where the 700 line ends", 0.01f);
}

// The inverse, which is what a chosen field of view is turned into before it is
// written to the camera. It has to land back on the focal it came from, or the
// engine renders one field of view while the cursor is placed through another.
void TestFovToFocal() {
    const float focals[] = {172.0f, 250.0f, 310.0f, 350.0f, 400.0f, 487.5f,
                            500.0f, 550.0f, 600.0f, 664.0f, 700.0f, 750.0f};
    for (float focal : focals) {
        CheckNear(FocalFromFovDegrees(FocalToFovDegrees(focal)), focal,
                  "a focal survives the round trip through its field of view", 0.05f);
    }

    // The one band the engine reaches two ways. 77.0 degrees is both focal 294
    // and focal 303; the inverse takes the second, the line the game's own 310
    // sits on.
    const float ambiguous = FocalFromFovDegrees(77.0f);
    Check(ambiguous >= 300.0f && ambiguous < 400.0f,
          "the two-degree step at focal 300 resolves onto the line holding 310");

    CheckNear(FocalFromFovDegrees(kMinFieldOfView), 663.64f,
              "the narrowest field of view a player may ask for is focal 664", 0.05f);
    CheckNear(FocalFromFovDegrees(kMaxFieldOfView), 172.06f,
              "and the widest is focal 172", 0.05f);
}

// A chosen field of view. It is written into the game's own base focal, so the
// engine renders it and the pose measures against it: ordinary play still scales
// by exactly 1, and the game's own zoom still counts.
void TestChosenFieldOfView() {
    const long chosen = BaseFocalForFov(90.0f);
    Check(chosen < kReferenceFocal, "a wider view is a smaller focal");
    CheckNear(FocalToFovDegrees(static_cast<float>(chosen)), 90.0f,
              "and the focal written renders the field of view that was asked for", 0.1f);

    const ZoomBasis rest = MakeZoomBasis(static_cast<float>(chosen), static_cast<float>(chosen));
    CheckNear(rest.factor, 1.0f, "which is the new reference, so the pose is untouched", 1e-6f);

    // A fully drawn bow adds BOW_FOCAL/4 to whatever the base is, so the zoom is
    // the game's own arithmetic either way. It still shrinks the pose.
    const ZoomBasis drawn =
        MakeZoomBasis(static_cast<float>(chosen) + 177.5f, static_cast<float>(chosen));
    Check(drawn.factor < 1.0f && drawn.factor > 0.5f,
          "drawing a bow at a chosen field of view still shrinks the pose");
    Check(drawn.fov < rest.fov, "and the frame really is narrower");

    CheckNear(static_cast<float>(BaseFocalForFov(FocalToFovDegrees(kReferenceFocal))),
              kReferenceFocal,
              "asking for the game's own field of view asks for the game's own focal", 0.6f);
}

// ---------------------------------------------------------------------------
// The INI, driven end to end through the reader that ships.
std::string TempIni(const char* body) {
    char dir[MAX_PATH] = {};
    const DWORD written = GetTempPathA(MAX_PATH, dir);
    if (written == 0 || written >= MAX_PATH) {
        ++g_failures;
        std::printf("FAIL: could not resolve a temp directory to write the test INI into\n");
        return {};
    }
    std::string path = std::string(dir) + "arx_ht_test.ini";
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) {
        ++g_failures;
        std::printf("FAIL: could not create %s\n", path.c_str());
        return {};
    }
    fputs(body, f);
    fclose(f);
    return path;
}

void TestConfig() {
    const std::string path = TempIni(
        "[Network]\nPort=6039\n"
        "[General]\nEnableOnStartup=0\nMoveCrosshair=0\nFieldOfView=90\n"
        "[Smoothing]\nLocalSmoothing=0.25\nRemoteSmoothing=0.40\n"
        "[Position]\nPositionLimitX=0.15\nPositionLimitZ=0.55\nCollisionRadius=25\n"
        "[Hotkeys]\nToggleKey=0x24\n");
    Config cfg;
    Check(cfg.LoadOrCreate(path.c_str()), "an existing INI loads");
    Check(cfg.udp_port == 6039, "the port is read");
    Check(!cfg.enabled_on_startup, "EnableOnStartup=0 is honoured");
    Check(!cfg.move_crosshair, "MoveCrosshair=0 is honoured");
    CheckNear(cfg.field_of_view, 90.0f, "a chosen field of view is read");
    CheckNear(cfg.local_smoothing, 0.25f, "LocalSmoothing is read");
    CheckNear(cfg.remote_smoothing, 0.40f, "RemoteSmoothing is read");
    CheckNear(cfg.pos_limit_x, 0.15f, "a position limit is read");
    CheckNear(cfg.pos_limit_z, 0.55f, "the forward limit is read");
    CheckNear(cfg.collision_radius, 25.0f, "the collision standoff is read in Arx units");
    Check(cfg.vk_toggle == 0x24, "a bindable key is accepted");
    DeleteFileA(path.c_str());

    // A limit outside its range is pulled back into it and the correction is
    // logged. Zero is the floor rather than the shipped default, which is the
    // point: a negative limit would invert the processor's bounds and pin the
    // lean at a fixed offset instead of freeing it, and zero simply disables
    // that axis until the player fixes the value the log named.
    const std::string bad = TempIni("[Position]\nPositionLimitX=-1.0\n"
                                    "[General]\nFieldOfView=200\n"
                                    "[Smoothing]\nLocalSmoothing=0,25\n"
                                    "[Hotkeys]\nToggleKey=0x230\n");
    Config guarded;
    Check(guarded.LoadOrCreate(bad.c_str()), "a malformed INI still loads");
    CheckNear(guarded.pos_limit_x, 0.0f, "a negative limit is pulled up to the range floor");
    CheckNear(guarded.field_of_view, 0.0f,
              "a field of view outside the range falls back to the game's own rather than "
              "being clamped to something nobody asked for");
    CheckNear(guarded.local_smoothing, Config{}.local_smoothing,
              "a European decimal comma keeps the default rather than reading as zero");
    Check(guarded.vk_toggle == Config{}.vk_toggle,
          "a key code the OS cannot poll keeps the default");
    DeleteFileA(bad.c_str());

    // The values that used to slip past the guards entirely. Every comparison
    // against a NaN is false, so FieldOfView=nan passed the range test AND the
    // "is a field of view set" gate in the camera hook, and went into the
    // engine's own CURRENT_BASE_FOCAL. ToggleKey=End parsed as hex 0x0E - a real
    // virtual key that no keyboard can produce. A trailing comment on a bool
    // matched none of the accepted words and the edit was discarded in silence.
    // And a standoff under the near plane holds the eye off a wall the engine
    // then culls, which is the complaint the clamp exists to answer.
    const std::string sneaky = TempIni("[General]\nFieldOfView=nan\nMoveCrosshair=0 ; off\n"
                                       "[Position]\nCollisionRadius=0.5\n"
                                       "PositionEnabled=maybe\n"
                                       "[Hotkeys]\nToggleKey=End\n");
    Config sane;
    Check(sane.LoadOrCreate(sneaky.c_str()), "an INI full of near-misses still loads");
    CheckNear(sane.field_of_view, 0.0f,
              "FieldOfView=nan renders the game's own rather than reaching the engine");
    Check(!sane.move_crosshair,
          "a bool with a trailing comment is honoured rather than silently discarded");
    Check(sane.position_enabled == Config{}.position_enabled,
          "and one that is not a yes or a no keeps the default and is reported");
    Check(sane.collision_radius >= kMinCollisionRadius,
          "a standoff under the engine's near clip is pulled up to it");
    Check(sane.vk_toggle == Config{}.vk_toggle,
          "a key NAME is refused rather than read as a hex prefix");
    DeleteFileA(sneaky.c_str());

    // A comma decimal and a hex literal are both prefix-parseable and both
    // silently wrong; FieldOfView was the one number in the file still reading
    // them that way.
    const std::string prefixes = TempIni("[General]\nFieldOfView=75,95\n");
    Config commaFov;
    Check(commaFov.LoadOrCreate(prefixes.c_str()), "the comma INI loads");
    CheckNear(commaFov.field_of_view, 0.0f,
              "a European decimal comma is refused rather than read as 75");
    DeleteFileA(prefixes.c_str());
}

// ---------------------------------------------------------------------------
// The player.Interface bits the gameplay gate and the crosshair test are decided
// on. Which panels suppress tracking is a judgement rather than an engine fact,
// so it is locked here: a full-screen panel the player is reading suppresses it,
// the belt strip and a drawn weapon do not.
void TestInterfaceGateFlags() {
    constexpr int16_t kInterMap = 0x0001;
    constexpr int16_t kInterInventory = 0x0002;
    constexpr int16_t kInterInventoryAll = 0x0004;
    constexpr int16_t kInterCombatMode = 0x0040;
    constexpr int16_t kInterNote = 0x0080;
    constexpr int16_t kInterSteal = 0x0100;

    Check((kInterfaceBlockingFlags & kInterMap) != 0, "the book or map suppresses tracking");
    Check((kInterfaceBlockingFlags & kInterInventoryAll) != 0,
          "the full inventory page suppresses tracking");
    Check((kInterfaceBlockingFlags & kInterNote) != 0, "a note suppresses tracking");
    Check((kInterfaceBlockingFlags & kInterSteal) != 0, "the steal panel suppresses tracking");

    Check((kInterfaceBlockingFlags & kInterInventory) == 0,
          "the belt strip is ordinary play and does not suppress tracking");
    Check((kInterfaceBlockingFlags & kInterCombatMode) == 0,
          "a drawn weapon is ordinary play and does not suppress tracking");

    Check(kInterfaceCombatMode == kInterCombatMode,
          "the cursor test reads the drawn-weapon bit");
    Check(kPlayerAngleOffset == 12,
          "player.angle sits 12 bytes past player.pos, which is where the profile points");
}

// ---------------------------------------------------------------------------
// The build registry stays append-only and its head is the diagnostic primary.
void TestBuildProfile() {
    // MatchRunningProfile needs the game; what is checkable here is that the
    // constant the registry is keyed on has not drifted.
    Check(std::string(kGameExeName) == "arx.exe", "the registry points at arx.exe");
}

// ---------------------------------------------------------------------------
// The angle handed to the zoom scaling has to stay inside the domain that
// scaling is single-valued on. Nothing upstream bounds it: the wire carries
// whatever the tracker sends, and the INI sensitivity multiplying it is
// deliberately unbounded short of kMaxSensitivity. Past 90 degrees the tangent
// round trip wraps, and the view snaps round to face the other way at a zoom
// factor of exactly 1.0 - in ordinary play, with nothing zoomed.
void TestRotationDomainClamp() {
    const EngineRotation ordinary = ToEngineRotation(Pose(25.0f, 15.0f, 0, 0, 0), 1.0f);
    CheckNear(ordinary.dYaw, -25.0f, "ordinary yaw keeps its magnitude after conversion");
    CheckNear(ordinary.dPitch, -15.0f, "and so is its pitch");

    const EngineRotation past = ToEngineRotation(Pose(120.0f, 0.0f, 0, 0, 0), 1.0f);
    Check(past.dYaw < 0.0f, "a yaw past the domain preserves the engine boundary sign");
    CheckNear(past.dYaw, -kMaxTrackedAngleDegrees, "and saturates rather than wrapping", 0.01f);

    const EngineRotation behind = ToEngineRotation(Pose(-200.0f, 0.0f, 0, 0, 0), 1.0f);
    CheckNear(behind.dYaw, kMaxTrackedAngleDegrees, "the same holds the other way", 0.01f);

    // Pitch is negated on the way across, so the bound has to catch it after
    // that rather than before.
    const EngineRotation steep = ToEngineRotation(Pose(0.0f, 150.0f, 0, 0, 0), 1.0f);
    CheckNear(steep.dPitch, -kMaxTrackedAngleDegrees,
              "a pitch past the domain saturates looking up", 0.01f);
}

// The zoom factor is built from a focal read live off the camera, which is zero
// until the game has set one up. The focal map answers zero with 168.5 degrees,
// and taking that as a rendered field of view would multiply the pose by more
// than ten.
void TestZoomBasisRejectsUnreadableFocal() {
    CheckNear(MakeZoomBasis(0.0f, kReferenceFocal).factor, 1.0f,
              "a focal of zero applies no compensation rather than a huge one", 1e-6f);
    CheckNear(MakeZoomBasis(-310.0f, kReferenceFocal).factor, 1.0f,
              "and neither does a negative one", 1e-6f);
    CheckNear(MakeZoomBasis(kReferenceFocal, 0.0f).factor, 1.0f,
              "an unreadable base is the same answer", 1e-6f);
    Check(!MakeZoomBasis(0.0f, kReferenceFocal).usable,
          "and it says so, rather than passing an unmeasured 1.0 off as a measured one");

    // A readable pair has to come back as the ratio itself, not as the guard's
    // pass-through. Asserting it against the literal is the point: comparing the
    // call to itself, which is what this did, cannot fail.
    // The literal, worked out independently rather than re-derived from the code
    // this is testing: focal 487.5 renders 52.375 degrees and focal 310 renders
    // 75.95, so the factor is tan(26.1875) / tan(37.975) = 0.49180 / 0.78059.
    const ZoomBasis bow = MakeZoomBasis(487.5f, kReferenceFocal);
    Check(bow.usable, "a readable pair is measured rather than passed through");
    CheckNear(bow.factor, 0.63004f,
              "and a drawn bow's factor is the ratio of the two half-tangents", 1e-4f);
    Check(bow.factor < 1.0f, "a drawn bow still shrinks the pose");
}

// The projected aim point becomes the game's own cursor, which is two int16s.
// Converting a float outside their range is undefined, and the screen size that
// would otherwise bound it is read out of the game and is zero until the window
// has been sized.
void TestCursorClamp() {
    float x = 400.0f, y = 300.0f;
    Check(ClampToScreenPoint(x, y, 1024, 768), "a position inside the frame is writable");
    CheckNear(x, 400.0f, "and is left where it was");
    CheckNear(y, 300.0f, "vertically too");

    x = -50.0f; y = -1.0f;
    Check(ClampToScreenPoint(x, y, 1024, 768), "a position off the left edge is still writable");
    CheckNear(x, 0.0f, "pulled back to the left edge");
    CheckNear(y, 0.0f, "and to the top");

    x = 5000.0f; y = 4000.0f;
    Check(ClampToScreenPoint(x, y, 1024, 768), "a position off the right edge is writable");
    CheckNear(x, 1023.0f, "pulled back to the last column");
    CheckNear(y, 767.0f, "and the last row");

    // The frame size the game has not written yet. The bound has to come from
    // somewhere, because this is exactly when the projection produces a
    // coordinate in the hundreds of thousands.
    x = 500000.0f; y = 500000.0f;
    Check(ClampToScreenPoint(x, y, 0, 0), "an unsized frame still yields a writable position");
    CheckNear(x, kMaxCursorCoordinate, "bounded to what EERIE_S2D can hold");
    CheckNear(y, kMaxCursorCoordinate, "on both axes");
    Check(x <= 32767.0f && y <= 32767.0f, "so the conversion to int16 is defined");

    // Every comparison against a NaN is false, so it survives a clamp written
    // the obvious way. It has to be refused instead.
    x = std::numeric_limits<float>::quiet_NaN();
    y = 300.0f;
    Check(!ClampToScreenPoint(x, y, 1024, 768), "a NaN coordinate has no position to write");
    x = 400.0f;
    y = std::numeric_limits<float>::infinity();
    Check(!ClampToScreenPoint(x, y, 1024, 768), "and neither has an infinite one");
}


// ---------------------------------------------------------------------------
// The composed pitch has to stay inside the band the engine itself keeps the
// player in. ARX_Interface.cpp pushes player.desiredangle.a out of [74.9, 301]
// on every mouse frame, so the game never renders past 74.9 down or 59 up. Head
// pitch is ADDED to that, and past 90 degrees cos(angle.a) flips sign: the world
// in front is culled, the world behind is drawn, and the view snaps round. A
// 75 degree look at the floor plus 16 degrees of head tilt gets there.
void TestPitchStaysInsideTheEngineBand() {
    CheckNear(ComposeEnginePitch(30.0f, 10.0f), 40.0f,
              "an ordinary pose is added straight on");

    // The engine stores a look upward as 301..360, so it has to be unwrapped
    // before anything is added and rewrapped before it is written back.
    CheckNear(ComposeEnginePitch(310.0f, -5.0f), 305.0f,
              "a wrapped upward pitch composes without jumping a full turn");

    CheckNear(ComposeEnginePitch(kEnginePitchDown, 16.0f), kEnginePitchDown,
              "looking at the floor and tilting the head down cannot cross 90");
    CheckNear(ComposeEnginePitch(301.0f, -32.0f), 360.0f + kEnginePitchUp,
              "and looking up and tilting up cannot cross -90");

    // A pitch the engine put outside the band itself is left exactly as it is:
    // the clamp is there to stop the mod pushing past what the game renders, not
    // to correct the game.
    CheckNear(ComposeEnginePitch(100.0f, 0.0f), 100.0f,
              "a scripted out-of-band pitch is not moved when the head asks for nothing");

    // For any clean pitch the engine's own mouse path can produce, the composed
    // pitch keeps the sign of cos(angle.a) that the renderer needs, so nothing in
    // front of the eye is ever culled. A clean pitch the engine itself put
    // outside the band is passed through untouched instead, which the assertion
    // above covers.
    const float poses[][2] = {{74.9f, 89.0f}, {301.0f, -89.0f}, {0.0f, 89.0f},
                              {74.9f, 0.0f}, {60.0f, 89.0f}};
    for (const auto& p : poses) {
        const float a = ComposeEnginePitch(p[0], p[1]);
        Check(std::cos(UnwrapEngineAngle(a) * kDegToRad) > 0.0f,
              "the composed pitch never flips the renderer's depth term");
    }
}

// ---------------------------------------------------------------------------
// Every projection assertion above is taken at yaw 0 and pitch 0, where the four
// trig terms are 1/0/1/0 and the rotation half of the transform is multiplied by
// identity. A swapped sine or a flipped sign would pass all of them. A point
// straight down the camera's own forward vector has to land on the screen centre
// at any orientation, and that is what ties ForwardFromAngles - which the aim
// cast is fired along - to ProjectWorldPoint, which places the mark.
void TestProjectionAgreesWithForwardAtAnyOrientation() {
    const float scale = ProjectionScale(kReferenceFocal, 739);
    const float poses[][2] = {{0.0f, 0.0f},   {30.0f, 0.0f},     {0.0f, 90.0f},
                              {-25.0f, 200.0f}, {35.0f, 259.65f}, {60.0f, 315.0f}};

    for (const auto& p : poses) {
        const EerieCamera cam = MakeCamera(p[0], p[1], 100.0f, -170.0f, 250.0f, 509.0f, 369.0f);
        const Eerie3D f = ForwardFromAngles(p[0], p[1]);
        const Eerie3D ahead{cam.pos_x + f.x * 800.0f, cam.pos_y + f.y * 800.0f,
                            cam.pos_z + f.z * 800.0f};
        ScreenPoint sp{};
        Check(ProjectWorldPoint(cam, scale, ahead, sp),
              "a point down the camera's forward vector projects at any orientation");
        CheckNear(sp.x, 509.0f, "and lands on the screen centre", 0.05f);
        CheckNear(sp.y, 369.0f, "vertically too", 0.05f);
    }

    // Leaning left while facing sideways still moves the mark horizontally and
    // only horizontally. This is what locks MakeHorizonBasis to the projection:
    // the basis is built from the clean yaw, so the offset has to arrive on the
    // camera's own right axis whatever that yaw is.
    const float yaw = 90.0f;
    const Eerie3D f = ForwardFromAngles(0.0f, yaw);
    const Eerie3D target{f.x * 1000.0f, f.y * 1000.0f, f.z * 1000.0f};

    FrameSample lean;
    lean.has_position = true;
    lean.pos_x = 0.30f;
    float offset[3];
    ToEngineOffset(lean, yaw, 1.0f, offset);

    const EerieCamera leaned =
        MakeCamera(0.0f, yaw, offset[0], offset[1], offset[2], 509.0f, 369.0f);
    ScreenPoint sp{};
    Check(ProjectWorldPoint(leaned, scale, target, sp), "the leaned eye projects");
    CheckNear(sp.y, 369.0f, "a sideways lean does not move the mark vertically", 0.05f);
    Check(sp.x > 509.0f + 5.0f,
          "and it moves it to the right, the way leaning left does at any facing");
}

// ---------------------------------------------------------------------------
// A lean query that could not run has to say so. Core keeps `queried` and
// `blocked` apart precisely because a clamp that has quietly stopped clamping
// looks exactly like a clear room, and the two need different fixes.
void TestLeanQueryReportsItsOwnFailure() {
    const cameraunlock::math::Vec3 eye(0.0f, 0.0f, 0.0f);
    const cameraunlock::math::Vec3 dir(1.0f, 0.0f, 0.0f);
    const cameraunlock::camera::LeanObstruction out = LeanQuery(nullptr, eye, dir, 30.0f);
    Check(!out.queried, "with no ray function the query reports a failure");
    Check(!out.blocked, "and does not pass a failure off as a clear path");
}

}  // namespace

int main() {
    TestRotationSigns();
    TestRollProjection();
    TestZoomScaling();
    TestPositionSigns();
    TestPositionFollowsFacing();
    TestProjection();
    TestProjectionAgreesWithForwardAtAnyOrientation();
    TestPitchStaysInsideTheEngineBand();
    TestLeanQueryReportsItsOwnFailure();
    TestForwardVector();
    TestFocalToFov();
    TestFovToFocal();
    TestChosenFieldOfView();
    TestRotationDomainClamp();
    TestZoomBasisRejectsUnreadableFocal();
    TestCursorClamp();
    TestConfig();
    TestInterfaceGateFlags();
    TestBuildProfile();

    if (g_failures == 0) {
        std::printf("all tests passed\n");
        return 0;
    }
    std::printf("%d test(s) failed\n", g_failures);
    return 1;
}
