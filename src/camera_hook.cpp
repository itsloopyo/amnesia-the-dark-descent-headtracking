#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "camera_hook.h"
#include "game_offsets.h"
#include "mod.h"

#include "cameraunlock/memory/pattern_scanner.h"
#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/logging/file_log.h"

namespace AmnesiaHT {

// Head tracking must reach the picture and nothing else, and in HPL2 that needs
// three hooks rather than one.
//
// cCamera::GetForward, GetRight, GetUp and UnProject are all thin wrappers over
// cCamera::GetViewMatrix, and the game leans on them hard: the interaction ray
// (cLuxMapHelper::GetClosestEntity along GetPosition()/GetForward()), the throw
// impulse on a grabbed body, the lever / door / wheel / slide interaction axes,
// and the sound listener all read the camera through them. Writing head
// tracking into the matrix those return puts the head into every one of them.
// The camera frustum is worse: iLuxEnemy::IsSeenByPlayer,
// cLuxPlayerSanity::UpdateCheckEnemySeen, cLuxProp_Object::UpdateInsanityVision
// and friends test against cCamera::GetFrustum, so a tracked frustum moves
// monster behaviour and sanity drain.
//
// All of those run in the update tick. The renderer, by contrast, reaches the
// camera through exactly one path: cScene::Render calls cCamera::GetFrustum
// once per viewport, and GetFrustum is the only caller of GetViewMatrix that
// feeds cFrustum::SetupPerspectiveProj. So the injection is gated to
// GetViewMatrix calls made from inside GetFrustum, made from inside
// cScene::Render - and the camera's cached matrix and frustum are marked dirty
// on the way out, so the next gameplay read rebuilds them clean.

using SceneRender_t    = void  (__fastcall*)(void* self, void* edx, float frameTime, unsigned int flags);
using GetFrustum_t     = void* (__fastcall*)(void* self, void* edx);
using GetViewMatrix_t  = void* (__fastcall*)(void* self, void* edx);

static SceneRender_t   oSceneRender = nullptr;
static GetFrustum_t    oGetFrustum = nullptr;
static GetViewMatrix_t oGetViewMatrix = nullptr;

static void* g_sceneRender = nullptr;
static void* g_getFrustum = nullptr;
static void* g_getViewMatrix = nullptr;

// Single-threaded by construction: the whole chain runs on the game's render
// thread inside one cScene::Render call.
static bool g_inSceneRender = false;
static bool g_injectView = false;

static void SetFlag(void* camera, unsigned int offset, char value) {
    *reinterpret_cast<volatile char*>(reinterpret_cast<char*>(camera) + offset) = value;
}

static void __fastcall DetourSceneRender(void* self, void* edx, float frameTime, unsigned int flags) {
    Mod::Instance().BeginRenderFrame();
    g_inSceneRender = true;
    oSceneRender(self, edx, frameTime, flags);
    g_inSceneRender = false;
}

static void* __fastcall DetourGetFrustum(void* self, void* edx) {
    if (!g_inSceneRender) return oGetFrustum(self, edx);

    // Force both caches to rebuild so the frustum is built from a view matrix
    // that carries this frame's head pose rather than whatever the update tick
    // left behind.
    SetFlag(self, offsets::kCamFrustumUpdated, 1);
    SetFlag(self, offsets::kCamViewUpdated, 1);

    g_injectView = true;
    void* frustum = oGetFrustum(self, edx);
    g_injectView = false;

    // The camera's own cached view matrix and frustum now hold the tracked
    // values. Mark them dirty so the next read - which will be game logic, in
    // the next update tick - rebuilds them clean. The renderer keeps using the
    // cFrustum it was just handed, which is unaffected by the flag.
    SetFlag(self, offsets::kCamViewUpdated, 1);
    SetFlag(self, offsets::kCamFrustumUpdated, 1);
    return frustum;
}

static void* __fastcall DetourGetViewMatrix(void* self, void* edx) {
    if (!g_injectView) return oGetViewMatrix(self, edx);

    // Force a clean rebuild so the original always returns the game's intended
    // view matrix (head tracking is written into the same member in place,
    // which would otherwise compound frame over frame).
    SetFlag(self, offsets::kCamViewUpdated, 1);

    void* view = oGetViewMatrix(self, edx);
    Mod::Instance().ApplyToView(self, reinterpret_cast<float*>(view));
    return view;
}

static bool Install(void* module, const char* name, const char* pattern, void* detour,
                    void** original, void** target) {
    void* addr = cameraunlock::memory::ScanPattern(module, pattern);
    if (!addr) {
        cameraunlock::logging::Line("Camera: %s AOB not found - mod dormant (unsupported build).", name);
        return false;
    }
    cameraunlock::logging::Line("Camera: %s found at %p", name, addr);

    using namespace cameraunlock::hooks;
    auto& hm = HookManager::Instance();
    HookStatus s = hm.CreateHook(addr, detour, original);
    if (s != HookStatus::Ok) {
        cameraunlock::logging::Line("Camera: CreateHook(%s) failed (%s)", name, HookStatusToString(s));
        return false;
    }
    s = hm.EnableHook(addr);
    if (s != HookStatus::Ok) {
        cameraunlock::logging::Line("Camera: EnableHook(%s) failed (%s)", name, HookStatusToString(s));
        return false;
    }
    *target = addr;
    return true;
}

bool InstallCameraHook() {
    void* module = GetModuleHandleW(nullptr);

    using namespace cameraunlock::hooks;
    auto& hm = HookManager::Instance();
    HookStatus s = hm.Initialize();
    if (s != HookStatus::Ok && s != HookStatus::ErrorAlreadyInitialized) {
        cameraunlock::logging::Line("Camera: MinHook init failed (%s)", HookStatusToString(s));
        return false;
    }

    // All three or none: injecting without the render gate would put the head
    // into the game's interaction ray, enemy visibility and sanity checks.
    if (!Install(module, "cScene::Render", offsets::kSceneRenderPattern,
                 reinterpret_cast<void*>(&DetourSceneRender),
                 reinterpret_cast<void**>(&oSceneRender), &g_sceneRender) ||
        !Install(module, "cCamera::GetFrustum", offsets::kGetFrustumPattern,
                 reinterpret_cast<void*>(&DetourGetFrustum),
                 reinterpret_cast<void**>(&oGetFrustum), &g_getFrustum) ||
        !Install(module, "cCamera::GetViewMatrix", offsets::kGetViewMatrixPattern,
                 reinterpret_cast<void*>(&DetourGetViewMatrix),
                 reinterpret_cast<void**>(&oGetViewMatrix), &g_getViewMatrix)) {
        RemoveCameraHook();
        return false;
    }

    cameraunlock::logging::Line("Camera: hooks installed (render-gated injection).");
    return true;
}

void RemoveCameraHook() {
    auto& hm = cameraunlock::hooks::HookManager::Instance();
    for (void** target : { &g_getViewMatrix, &g_getFrustum, &g_sceneRender }) {
        if (*target) {
            hm.RemoveHook(*target);
            *target = nullptr;
        }
    }
}

}  // namespace AmnesiaHT
