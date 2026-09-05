#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "game_state.h"
#include "game_offsets.h"

#include "cameraunlock/memory/pattern_scanner.h"
#include "cameraunlock/logging/file_log.h"

namespace AmnesiaHT {

// The world keeps rendering when the player is not in it. Opening the main
// menu, the inventory or the journal leaves the map drawn behind the overlay,
// and a load screen renders too, so the render-phase gate the camera hook uses
// says nothing about whether the player is actually playing. Head tracking that
// swings the view around under a menu is the symptom; the input state is the
// fix, because the game switches it on every one of those transitions.
//
// gpBase is not exported and has no signature of its own, so it is read out of
// the `mov eax, [gpBase]` that opens cLuxPlayer::DrawHud - the same instruction
// the crosshair hook takes it from.

static const char* const* g_gpBase = nullptr;

bool InitGameState() {
    void* module = GetModuleHandleW(nullptr);
    void* drawHud = cameraunlock::memory::ScanPattern(module, offsets::kDrawHudPattern);
    if (!drawHud) {
        cameraunlock::logging::Line(
            "Game state: cLuxPlayer::DrawHud AOB not found - cannot tell gameplay from menus.");
        return false;
    }

    g_gpBase = *reinterpret_cast<const char* const* const*>(
        reinterpret_cast<char*>(drawHud) + offsets::kDrawHudGpBaseOperand);
    cameraunlock::logging::Line("Game state: gpBase at %p", static_cast<const void*>(g_gpBase));
    return true;
}

const char* LuxBase() {
    return g_gpBase ? *g_gpBase : nullptr;
}

bool IsInGameplay() {
    // Both indirections are the game's own lifecycle, not impossible cases: the
    // mod initialises from DllMain, long before cLuxBase exists, and the
    // handlers hang off it later still.
    const char* base = LuxBase();
    if (base == nullptr) return false;

    const char* input =
        *reinterpret_cast<const char* const*>(base + offsets::kBaseInputHandler);
    if (input == nullptr) return false;

    return *reinterpret_cast<const int*>(input + offsets::kInputHandlerState) ==
           offsets::kInputStateGame;
}

}  // namespace AmnesiaHT
