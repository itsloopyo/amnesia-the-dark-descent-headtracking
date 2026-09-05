#pragma once

#include <atomic>
#include <cstdint>

#include "config.h"
#include "hpl_math.h"
#include "cameraunlock/protocol/udp_receiver.h"
#include "cameraunlock/tracking/head_tracking_session.h"
#include "cameraunlock/input/hotkey_poller.h"

namespace AmnesiaHT {

// Where cLuxPlayer::DrawHud should put the crosshair this frame.
enum class CrosshairPlacement {
    Unchanged,  // tracking is off, or no rendered view was captured
    Offset,     // draw at the game's position plus the returned NDC offset
    Hide,       // the aim is at or behind the rendered view; there is nowhere to draw
};

class Mod {
public:
    static Mod& Instance();

    bool Initialize();
    void Shutdown();

    bool IsEnabled() const { return m_enabled.load(); }
    void Toggle();
    void CycleMode();

    // Called from the cScene::Render hook, before the camera is consulted.
    void BeginRenderFrame();

    // Called from the camera hook for each cCamera::GetViewMatrix that the
    // render phase asks for. Applies the current head pose to the freshly
    // rebuilt clean view matrix in place, and records the basis it wrote so the
    // crosshair is projected through the same composition.
    void ApplyToView(void* camera, float* view16);

    // Called from the interaction-ray hook with the clean origin and direction
    // the game cast along, and the distance it came back with. This is the
    // game's own aim result: reusing it is what keeps the crosshair on the live
    // impact point instead of a fixed or smoothed depth.
    void OnAimRay(const float origin[3], const float dir[3], float distance, bool hit);

    // Called from the crosshair hook.
    CrosshairPlacement GetCrosshairPlacement(float& ndcX, float& ndcY);

    Mod(const Mod&) = delete;
    Mod& operator=(const Mod&) = delete;

private:
    Mod() : m_session(m_receiver) {}
    ~Mod() = default;

    void LoadConfig();
    void RegisterHotkeys();
    // Reports which of the two smoothing parameters the session is now using,
    // which follows the receiver's source-address classification.
    void LogConnectionLocality();
    // Runs the tracking pipeline once per frame (deduped across multiple
    // GetViewMatrix calls per frame for extra viewports).
    void ComputeFrame();

    Config m_config;
    cameraunlock::UdpReceiver m_receiver;
    cameraunlock::HeadTrackingSession<cameraunlock::UdpReceiver> m_session;
    cameraunlock::input::HotkeyPoller m_hotkeys;

    std::atomic<bool> m_enabled{false};
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_cameraHooked{false};

    // Last locality reported; the session owns the flag the processors use.
    bool m_isRemoteConnection = false;
    bool m_remoteConnectionKnown = false;

    bool m_hookRanLogged = false;
    bool m_firstPoseLogged = false;
    bool m_gameplayLogged = false;

    // Sampled once per render frame: head tracking is suppressed outside
    // gameplay, and the menus that suppress it keep the world rendering behind
    // them.
    bool m_inGameplay = false;

    uint64_t m_lastFrameUs = 0;
    float m_yaw = 0.0f, m_pitch = 0.0f, m_roll = 0.0f;
    bool  m_rotValid = false;
    float m_ox = 0.0f, m_oy = 0.0f, m_oz = 0.0f;
    bool  m_posValid = false;

    // Render frames, counted at cScene::Render. Used only to tell a live
    // interaction ray from one left behind by a player state that stopped
    // casting - a stale depth is the classic way a crosshair drifts.
    uint32_t m_renderFrame = 0;

    // The interaction ray, as the game cast it during the update tick.
    float    m_rayOrigin[3] = {0, 0, 0};
    float    m_rayDir[3] = {0, 0, 0};
    float    m_rayDist = 0.0f;
    bool     m_rayHit = false;
    bool     m_rayValid = false;
    uint32_t m_rayFrame = 0;

    // The basis actually written into the camera this frame, and the clean aim
    // to project through it.
    math::ViewBasis m_renderView{};
    float m_tanX = 0.0f, m_tanY = 0.0f;
    bool  m_renderViewValid = false;
    float m_aimPoint[3] = {0, 0, 0};
    bool  m_aimIsPoint = false;   // false = m_aimPoint holds a direction
};

}  // namespace AmnesiaHT
