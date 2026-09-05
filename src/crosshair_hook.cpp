#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "crosshair_hook.h"
#include "game_offsets.h"
#include "game_state.h"
#include "mod.h"

#include "cameraunlock/memory/pattern_scanner.h"
#include "cameraunlock/hooks/hook_manager.h"
#include "cameraunlock/logging/file_log.h"

namespace AmnesiaHT {

// Amnesia's crosshair is a HUD sprite drawn at a fixed screen centre, and it is
// the interaction marker: its shape changes with what the ray in front of the
// player is on, and pressing use acts on that. Once head tracking moves the
// rendered view off the aim, screen centre stops being where the ray points, so
// the crosshair has to be projected onto the point the ray actually lands on.
//
// Three hooks:
//   cLuxMapHelper::GetClosestEntity - the game's own interaction ray. Taking
//       its result rather than casting again means the depth is the live one,
//       measured against exactly the bodies the game accepts as interactable or
//       solid, on the frame that consumes it.
//   cLuxPlayer::DrawHud - a window. It contains exactly one cGuiSet::DrawGfx
//       call, the crosshair, so the window identifies that draw without having
//       to recognise the sprite.
//   cGuiSet::DrawGfx - moves it.

struct Vec2f { float x, y; };
struct Vec3f { float x, y, z; };

using GetClosestEntity_t = char (__fastcall*)(void* self, void* edx, const Vec3f* start,
                                              const Vec3f* dir, float rayLength, float* distance,
                                              void** body, void** entity);
using DrawHud_t = void (__fastcall*)(void* self, void* edx, float frameTime);
using DrawGfx_t = void (__fastcall*)(void* self, void* edx, void* gfx, const Vec3f* pos,
                                     const Vec2f* size, const void* color, int material,
                                     float rotationAngle, int useCustomPivot,
                                     const Vec3f* customPivot);

static GetClosestEntity_t oGetClosestEntity = nullptr;
static DrawHud_t oDrawHud = nullptr;
static DrawGfx_t oDrawGfx = nullptr;

static void* g_getClosestEntity = nullptr;
static void* g_drawHud = nullptr;
static void* g_drawGfx = nullptr;

static bool g_inDrawHud = false;

static char __fastcall DetourGetClosestEntity(void* self, void* edx, const Vec3f* start,
                                              const Vec3f* dir, float rayLength, float* distance,
                                              void** body, void** entity) {
    char result = oGetClosestEntity(self, edx, start, dir, rayLength, distance, body, entity);
    if (start && dir && distance) {
        const float o[3] = { start->x, start->y, start->z };
        const float d[3] = { dir->x, dir->y, dir->z };
        const bool hit = *distance < offsets::kNoHitDistance;
        Mod::Instance().OnAimRay(o, d, *distance, hit);
    }
    return result;
}

static void __fastcall DetourDrawHud(void* self, void* edx, float frameTime) {
    g_inDrawHud = true;
    oDrawHud(self, edx, frameTime);
    g_inDrawHud = false;
}

static void __fastcall DetourDrawGfx(void* self, void* edx, void* gfx, const Vec3f* pos,
                                     const Vec2f* size, const void* color, int material,
                                     float rotationAngle, int useCustomPivot,
                                     const Vec3f* customPivot) {
    float ndcX = 0.0f, ndcY = 0.0f;
    CrosshairPlacement placement = CrosshairPlacement::Unchanged;
    const char* base = g_inDrawHud ? LuxBase() : nullptr;
    if (base != nullptr && pos != nullptr) {
        placement = Mod::Instance().GetCrosshairPlacement(ndcX, ndcY);
    }

    if (placement == CrosshairPlacement::Hide) {
        // The head has turned past the aim; there is no screen position that
        // marks it, and a crosshair clamped to an edge would claim there is.
        return;
    }
    if (placement == CrosshairPlacement::Unchanged) {
        oDrawGfx(self, edx, gfx, pos, size, color, material, rotationAngle, useCustomPivot,
                 customPivot);
        return;
    }

    // The HUD set's orthographic projection spans mvHudVirtualSize across the
    // whole screen, so half of it is one NDC unit. GUI y runs downwards.
    const float halfW = *reinterpret_cast<const float*>(base + offsets::kBaseHudVirtualSizeX) * 0.5f;
    const float halfH = *reinterpret_cast<const float*>(base + offsets::kBaseHudVirtualSizeY) * 0.5f;

    const Vec3f moved = { pos->x + ndcX * halfW, pos->y - ndcY * halfH, pos->z };
    oDrawGfx(self, edx, gfx, &moved, size, color, material, rotationAngle, useCustomPivot,
             customPivot);
}

static bool Install(void* module, const char* name, const char* pattern, void* detour,
                    void** original, void** target) {
    void* addr = cameraunlock::memory::ScanPattern(module, pattern);
    if (!addr) {
        cameraunlock::logging::Line("Crosshair: %s AOB not found.", name);
        return false;
    }

    using namespace cameraunlock::hooks;
    auto& hm = HookManager::Instance();
    HookStatus s = hm.CreateHook(addr, detour, original);
    if (s != HookStatus::Ok) {
        cameraunlock::logging::Line("Crosshair: CreateHook(%s) failed (%s)", name,
                                    HookStatusToString(s));
        return false;
    }
    s = hm.EnableHook(addr);
    if (s != HookStatus::Ok) {
        cameraunlock::logging::Line("Crosshair: EnableHook(%s) failed (%s)", name,
                                    HookStatusToString(s));
        return false;
    }
    *target = addr;
    return true;
}

bool InstallCrosshairHook() {
    void* module = GetModuleHandleW(nullptr);

    if (!Install(module, "cLuxMapHelper::GetClosestEntity", offsets::kGetClosestEntityPattern,
                 reinterpret_cast<void*>(&DetourGetClosestEntity),
                 reinterpret_cast<void**>(&oGetClosestEntity), &g_getClosestEntity) ||
        !Install(module, "cLuxPlayer::DrawHud", offsets::kDrawHudPattern,
                 reinterpret_cast<void*>(&DetourDrawHud),
                 reinterpret_cast<void**>(&oDrawHud), &g_drawHud) ||
        !Install(module, "cGuiSet::DrawGfx", offsets::kDrawGfxPattern,
                 reinterpret_cast<void*>(&DetourDrawGfx),
                 reinterpret_cast<void**>(&oDrawGfx), &g_drawGfx)) {
        RemoveCrosshairHook();
        return false;
    }

    cameraunlock::logging::Line("Crosshair: hooks installed.");
    return true;
}

void RemoveCrosshairHook() {
    auto& hm = cameraunlock::hooks::HookManager::Instance();
    for (void** target : { &g_drawGfx, &g_drawHud, &g_getClosestEntity }) {
        if (*target) {
            hm.RemoveHook(*target);
            *target = nullptr;
        }
    }
}

}  // namespace AmnesiaHT
