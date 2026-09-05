#pragma once

namespace AmnesiaHT {

// Locates cLuxMapHelper::GetClosestEntity, cLuxPlayer::DrawHud and
// cGuiSet::DrawGfx by AOB and installs the MinHook detours that move the
// crosshair onto the point the interaction ray lands on. Returns false if any
// pattern is not found; head tracking still runs without it.
bool InstallCrosshairHook();
void RemoveCrosshairHook();

}  // namespace AmnesiaHT
