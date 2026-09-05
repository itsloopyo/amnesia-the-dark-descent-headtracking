#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cmath>
#include <string>

#include "mod.h"
#include "aim_projection.h"
#include "camera_hook.h"
#include "crosshair_hook.h"
#include "game_offsets.h"
#include "game_state.h"
#include "hpl_math.h"

#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/input/chord_hotkeys.h"
#include "cameraunlock/data/tracking_pose.h"
#include "cameraunlock/data/position_settings.h"
#include "cameraunlock/math/smoothing_utils.h"

namespace AmnesiaHT {

static uint64_t NowMicros() {
    static LARGE_INTEGER freq = {};
    if (freq.QuadPart == 0) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return static_cast<uint64_t>(now.QuadPart * 1000000 / freq.QuadPart);
}

static std::string ExeDirFile(const char* name) {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string p(path);
    size_t slash = p.find_last_of("\\/");
    std::string dir = (slash == std::string::npos) ? "." : p.substr(0, slash);
    return dir + "\\" + name;
}

Mod& Mod::Instance() {
    static Mod instance;
    return instance;
}

void Mod::LoadConfig() {
    std::string path = ExeDirFile("HeadTracking.ini");
    if (m_config.Load(path.c_str())) {
        cameraunlock::logging::Line("Config loaded from %s", path.c_str());
    } else {
        cameraunlock::logging::Line("No config at %s - using defaults.", path.c_str());
    }
}

bool Mod::Initialize() {
    if (m_initialized.load()) return true;
    cameraunlock::logging::Line("Initializing mod...");

    LoadConfig();

    // Rotation pipeline.
    cameraunlock::SensitivitySettings sens;
    sens.yaw = m_config.yawSens;   sens.invert_yaw = m_config.invertYaw;
    sens.pitch = m_config.pitchSens; sens.invert_pitch = m_config.invertPitch;
    sens.roll = m_config.rollSens;  sens.invert_roll = m_config.invertRoll;
    m_session.GetProcessor().SetSensitivity(sens);

    // Position pipeline. Position takes the same connection-selected smoothing
    // as rotation, so there is no separate position smoothing setting.
    cameraunlock::PositionSettings pos;
    pos.sensitivity_x = m_config.posSensX;
    pos.sensitivity_y = m_config.posSensY;
    pos.sensitivity_z = m_config.posSensZ;
    pos.limit_x      = m_config.limitX;
    // The clamp is [-limit_y_down, +limit_y] and limit_y_down carries its own
    // default, so mirror the one configured vertical limit the way
    // PositionSettings::Symmetric does. Left unset, raising LimitY widened the
    // upward budget only and downward travel stayed pinned at 0.20m.
    pos.limit_y      = m_config.limitY;
    pos.limit_y_down = m_config.limitY;
    pos.limit_z      = m_config.limitZ;
    pos.limit_z_back = m_config.limitZBack;
    pos.invert_x = m_config.invertPosX;
    pos.invert_y = m_config.invertPosY;
    pos.invert_z = m_config.invertPosZ;
    m_session.GetPositionProcessor().SetSettings(pos);

    // After SetSettings, which would otherwise reset what the user configured.
    // Both values go to rotation and position; the session picks one per
    // connection from the address the packets arrive from.
    static_assert(cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver>::kHasRemoteConnection,
                  "receiver must classify connection locality, or smoothing "
                  "silently stays on the local parameter forever");
    m_session.SetLocalSmoothing(m_config.localSmoothing);
    m_session.SetRemoteSmoothing(m_config.remoteSmoothing);
    if (!m_config.positionEnabled) {
        m_session.SetMode(cameraunlock::TrackingMode::RotationOnly);
    }

    // Camera hook. The game-state anchor goes with it rather than being
    // optional: without it the mod cannot tell gameplay from a menu, and would
    // swing the view around under the main menu, the inventory and the journal,
    // all of which keep the map rendering behind them.
    //
    // The crosshair hooks ARE optional: without them head tracking still works,
    // the crosshair just stops marking the thing the interaction ray is on once
    // the head leaves centre.
    m_cameraHooked.store(InitGameState() && InstallCameraHook());
    if (!m_cameraHooked.load()) {
        cameraunlock::logging::Line("Camera hook not installed - head tracking disabled.");
    } else if (!InstallCrosshairHook()) {
        cameraunlock::logging::Line(
            "Crosshair compensation not installed - the crosshair will stay at screen centre.");
    }

    // UDP receiver.
    m_receiver.SetLog([](const std::string& s) { cameraunlock::logging::Line("UDP: %s", s.c_str()); });
    if (!m_receiver.Start(static_cast<uint16_t>(m_config.udpPort))) {
        cameraunlock::logging::Line("UDP receiver retrying on port %d...", m_config.udpPort);
    } else {
        cameraunlock::logging::Line("UDP receiver listening on port %d.", m_config.udpPort);
    }

    RegisterHotkeys();

    m_enabled.store(m_config.autoEnable && m_cameraHooked.load());
    m_initialized.store(true);
    cameraunlock::logging::Line("Mod initialized (tracking %s).", m_enabled.load() ? "ON" : "OFF");
    return true;
}

void Mod::RegisterHotkeys() {
    using cameraunlock::input::NavGuarded;
    using cameraunlock::input::ChordGuarded;

    // Nav-cluster keys (suppressed while the chord modifier is held).
    m_hotkeys.AddHotkey(m_config.toggleKey,       NavGuarded([] { Instance().Toggle(); }));
    m_hotkeys.AddHotkey(m_config.trackingModeKey, NavGuarded([] { Instance().CycleMode(); }));

    // Ctrl+Shift chord fallbacks (Y / G cluster).
    m_hotkeys.AddHotkey('Y', ChordGuarded([] { Instance().Toggle(); }));
    m_hotkeys.AddHotkey('G', ChordGuarded([] { Instance().CycleMode(); }));

    m_hotkeys.Start(16);
    cameraunlock::logging::Line("Hotkeys: End=Toggle PageUp=CycleMode (+ Ctrl+Shift+Y/G).");
}

void Mod::Shutdown() {
    if (!m_initialized.load()) return;
    m_hotkeys.Stop();
    m_receiver.Stop();
    RemoveCrosshairHook();
    RemoveCameraHook();
    m_initialized.store(false);
    cameraunlock::logging::Line("Mod shut down.");
}

void Mod::Toggle() {
    bool now = !m_enabled.load();
    m_enabled.store(now);
    cameraunlock::logging::Line("Head tracking %s", now ? "ENABLED" : "DISABLED");
}

void Mod::CycleMode() {
    cameraunlock::TrackingMode mode = m_session.CycleMode();
    const char* label = mode == cameraunlock::TrackingMode::RotationAndPosition ? "6DOF"
                      : mode == cameraunlock::TrackingMode::RotationOnly ? "Rotation only"
                      : "Position only";
    cameraunlock::logging::Line("Tracking mode: %s", label);
}

void Mod::LogConnectionLocality() {
    const bool isRemote = m_session.IsRemoteConnection();
    if (m_remoteConnectionKnown && isRemote == m_isRemoteConnection) return;
    m_isRemoteConnection = isRemote;
    m_remoteConnectionKnown = true;

    const double effective = cameraunlock::math::GetEffectiveSmoothing(
        m_config.localSmoothing, m_config.remoteSmoothing, isRemote);
    cameraunlock::logging::Line("Tracker connection is %s; smoothing=%.2f",
                                isRemote ? "remote" : "local", effective);
}

void Mod::ComputeFrame() {
    uint64_t now = NowMicros();
    if (m_lastFrameUs != 0 && (now - m_lastFrameUs) < 1000) return;  // same frame

    float dt = 0.016f;
    if (m_lastFrameUs != 0) {
        dt = static_cast<float>(now - m_lastFrameUs) / 1000000.0f;
        if (dt > 0.1f) dt = 0.1f;
        if (dt < 0.0001f) dt = 0.0001f;
    }
    m_lastFrameUs = now;

    // The session re-reads the receiver's locality every Update, so a tracker
    // swap (local OpenTrack <-> phone on WiFi) picks up the other smoothing
    // parameter without a restart. This only reports the change.
    const bool fresh = m_session.Update(dt);
    // Only reported once a packet has actually parsed: the receiver's locality
    // flag starts at false, so reporting it before then reads as proof a local
    // tracker connected when nothing has arrived.
    if (fresh) LogConnectionLocality();

    // Latched, because "did any tracker data reach the mod" is the one question
    // the startup lines cannot answer: the UDP bind succeeds whether or not a
    // tracker ever sends.
    if (fresh && !m_firstPoseLogged) {
        m_firstPoseLogged = true;
        float y = 0.0f, p = 0.0f, r = 0.0f;
        m_session.GetRotation(y, p, r);
        cameraunlock::logging::Line("Tracker data received: yaw=%.2f pitch=%.2f roll=%.2f", y, p,
                                    r);
    }

    if (fresh) {
        m_rotValid = m_session.GetRotation(m_yaw, m_pitch, m_roll);
        m_posValid = m_session.GetPositionOffset(m_ox, m_oy, m_oz);
    } else {
        m_rotValid = false;
        m_posValid = false;
    }
}

void Mod::BeginRenderFrame() {
    ++m_renderFrame;
    m_renderViewValid = false;
    m_inGameplay = IsInGameplay();

    // Latched, because "tracking is off" and "the mod never sees gameplay" read
    // the same in the log otherwise, and the second is what a wrong gpBase or a
    // patched cLuxBase layout would look like.
    if (m_inGameplay && !m_gameplayLogged) {
        m_gameplayLogged = true;
        cameraunlock::logging::Line("Game state: in gameplay.");
    }
}

void Mod::OnAimRay(const float origin[3], const float dir[3], float distance, bool hit) {
    for (int i = 0; i < 3; ++i) {
        m_rayOrigin[i] = origin[i];
        m_rayDir[i] = dir[i];
    }
    m_rayDist = distance;
    m_rayHit = hit;
    m_rayValid = true;
    m_rayFrame = m_renderFrame;
}

void Mod::ApplyToView(void* camera, float* view16) {
    // Latched ahead of the enabled check: an installed hook that never runs and
    // a hook that runs with tracking off read the same in the log otherwise.
    if (!m_hookRanLogged) {
        m_hookRanLogged = true;
        cameraunlock::logging::Line("Camera: GetViewMatrix hook is running.");
    }
    // Ahead of the enabled check as well: whether any tracker data reached the
    // mod has to be answerable with tracking toggled off or AutoEnable=false.
    ComputeFrame();
    if (!m_enabled.load() || !m_inGameplay || view16 == nullptr) return;
    if (!m_rotValid && !m_posValid) return;

    float yaw = m_rotValid ? m_yaw : 0.0f;
    float pitch = m_rotValid ? m_pitch : 0.0f;
    float roll = m_rotValid ? m_roll : 0.0f;
    float ox = m_posValid ? m_ox : 0.0f;
    float oy = m_posValid ? m_oy : 0.0f;
    float oz = m_posValid ? m_oz : 0.0f;

    // The clean view, before injection, is the aim: it is what the game's own
    // interaction ray was cast along and what every gameplay query still sees.
    math::ViewBasis clean{};
    math::DecomposeView(view16, clean);

    math::ApplyHeadTracking(view16, yaw, pitch, roll, ox, oy, oz);

    math::DecomposeView(view16, m_renderView);

    const char* cam = reinterpret_cast<const char*>(camera);
    const float fov = *reinterpret_cast<const float*>(cam + offsets::kCamFov);
    const float aspect = *reinterpret_cast<const float*>(cam + offsets::kCamAspect);
    m_tanY = std::tan(fov * 0.5f);
    m_tanX = aspect * m_tanY;

    // The point the interaction ray lands on, taken from the game's own cast on
    // this frame. A ray left over from a player state that has stopped casting
    // is refused rather than reused: a stale depth is how a crosshair ends up
    // agreeing at one distance and drifting either side of it.
    const bool rayFresh = m_rayValid && (m_rayFrame + 1 >= m_renderFrame);
    if (rayFresh && m_rayHit) {
        for (int i = 0; i < 3; ++i) m_aimPoint[i] = m_rayOrigin[i] + m_rayDir[i] * m_rayDist;
        m_aimIsPoint = true;
    } else {
        // Nothing hit, or nothing cast: the aim point is at infinity, so the
        // clean aim direction for this frame is the honest answer.
        const float* dir = rayFresh ? m_rayDir : clean.fwd;
        for (int i = 0; i < 3; ++i) m_aimPoint[i] = dir[i];
        m_aimIsPoint = false;
    }
    m_renderViewValid = true;
}

CrosshairPlacement Mod::GetCrosshairPlacement(float& ndcX, float& ndcY) {
    if (!m_enabled.load() || !m_renderViewValid) return CrosshairPlacement::Unchanged;

    float aim[3];
    if (m_aimIsPoint) {
        for (int i = 0; i < 3; ++i) aim[i] = m_aimPoint[i] - m_renderView.eye[i];
    } else {
        for (int i = 0; i < 3; ++i) aim[i] = m_aimPoint[i];
    }

    if (!ProjectAimToNdc(aim, m_renderView.fwd, m_renderView.right, m_renderView.up,
                         m_tanX, m_tanY, ndcX, ndcY)) {
        return CrosshairPlacement::Hide;
    }
    return CrosshairPlacement::Offset;
}

}  // namespace AmnesiaHT
