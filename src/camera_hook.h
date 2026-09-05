#pragma once

namespace AmnesiaHT {

// Locates cScene::Render, cCamera::GetFrustum and cCamera::GetViewMatrix by AOB
// and installs the MinHook detours. Returns false (and stays dormant) if any
// pattern is not found.
bool InstallCameraHook();
void RemoveCameraHook();

}  // namespace AmnesiaHT
