#pragma once
#include <cstdint>

// The engine structures and functions this mod has to interoperate with.
// Member names follow Frictional Games' GPLv3 HPL2 source release, which is
// where they were read from; no HPL2 code is copied here (see
// THIRD-PARTY-NOTICES.md).
//
// Every function is located by a byte signature at runtime rather than a fixed
// RVA, so one build of the mod survives the Steam / NoSteam exe split and minor
// patches; if a signature is not found the mod stays dormant and the game runs
// vanilla. Each signature below was checked to match exactly once in both
// shipped exes.

namespace AmnesiaHT::offsets {

// ---------------------------------------------------------------- cCamera --
// Field offsets in bytes from the cCamera* (the class has no vtable;
// mvPosition is the first member).
constexpr uint32_t kCamPosX           = 0x00;   // cVector3f mvPosition (metres)
constexpr uint32_t kCamPosY           = 0x04;
constexpr uint32_t kCamPosZ           = 0x08;
constexpr uint32_t kCamFov            = 0x0c;   // float mfFOV - VERTICAL, radians
constexpr uint32_t kCamAspect         = 0x10;   // float mfAspect
constexpr uint32_t kCamPitch          = 0x28;   // float mfPitch
constexpr uint32_t kCamYaw            = 0x2c;   // float mfYaw
constexpr uint32_t kCamRoll           = 0x30;   // float mfRoll
constexpr uint32_t kCamRotateMode     = 0x44;   // int   mRotateMode (0 = EulerAngles)
constexpr uint32_t kCamViewMatrix     = 0x4c;   // cMatrixf m_mtxView (16 floats, row-major)
constexpr uint32_t kCamViewUpdated    = 0x851;  // char  mbViewUpdated
constexpr uint32_t kCamFrustumUpdated = 0x854;  // char  mbFrustumUpdated

// mRotateMode value for Euler-angle cameras (the gameplay camera).
constexpr int kRotateModeEuler = 0;

// ---------------------------------------------------------------- cLuxBase --
// gpBase is a single global; its address is read out of the operand of the
// `mov eax, [gpBase]` that opens cLuxPlayer::DrawHud.
constexpr uint32_t kDrawHudGpBaseOperand = 43;
constexpr uint32_t kBaseHudVirtualSizeX  = 0x30;  // cVector2f mvHudVirtualSize
constexpr uint32_t kBaseHudVirtualSizeY  = 0x34;
constexpr uint32_t kBaseInputHandler     = 0x8c;  // cLuxInputHandler* mpInputHandler

// -------------------------------------------------------- cLuxInputHandler --
// The input state is what the game switches when it leaves gameplay, and it is
// the only such flag that covers every way out at once: the main menu Escape
// opens, the inventory, the journal and its notes, load screens, the credits,
// the pre-menu and the debug menu each get their own value. mState was read off
// the switch in cLuxInputHandler::Update, which dispatches cases 1..9 in
// eLuxInputState order; mpInputHandler off the store cLuxBase::Init makes with
// the constructed handler.
constexpr uint32_t kInputHandlerState = 0x3c;  // eLuxInputState mState
constexpr int kInputStateGame         = 1;     // eLuxInputState_Game

// -------------------------------------------------------------- signatures --

// cCamera::GetViewMatrix - rebuilds and returns m_mtxView. Called by the
// renderer (through GetFrustum) AND by cCamera::GetForward/GetRight/GetUp,
// which is why the injection has to be gated on the render phase rather than
// applied to every caller.
constexpr const char* kGetViewMatrixPattern =
    "55 8B EC 81 EC 80 00 00 00 53 8B D9 80 BB 51 08 00 00 00 56 57 "
    "8D 43 4C 0F 84 ?? ?? ?? ?? B9 10 00 00 00 BE ?? ?? ?? ?? 8B F8 F3 A5 8B 4B 44";

// cCamera::GetFrustum - the ONLY consumer of the camera's view matrix that
// feeds the renderer. Everything else the frustum reaches is game logic.
constexpr const char* kGetFrustumPattern =
    "55 8B EC 83 EC 10 56 8B F1 80 BE 54 08 00 00 00 0F 84 ?? ?? ?? ?? 53 32 DB "
    "38 9E 50 08 00 00 74 ?? 88 9E 50 08 00 00 C6 86 52 08 00 00 01 B3 01 "
    "83 7E 24 00 75 ?? D9 46 10";

// cScene::Render - the render phase. Gameplay's own camera and frustum queries
// (enemy visibility, sanity drain, interaction focus) all run in the update
// tick, outside this call.
constexpr const char* kSceneRenderPattern =
    "55 8B EC 83 EC 18 FF 05 ?? ?? ?? ?? 8B 41 44 89 4D FC 8B 08 89 4D F0 3B C8 "
    "0F 84 ?? ?? ?? ?? 53 56 57 8B 45 F0 8B 70 08 80 7E 0D 00 0F 84 ?? ?? ?? ?? "
    "8B 4E 10 8B 7E 14";

// cLuxPlayer::DrawHud - draws the crosshair at a fixed screen centre, then the
// focus text. It contains exactly one cGuiSet::DrawGfx call (the crosshair),
// which is what makes it usable as a window around that one draw.
constexpr const char* kDrawHudPattern =
    "55 8B EC 6A FF 68 ?? ?? ?? ?? 64 A1 00 00 00 00 50 83 EC 68 53 56 57 "
    "A1 ?? ?? ?? ?? 33 C5 50 8D 45 F4 64 A3 00 00 00 00 8B F1 A1 ?? ?? ?? ?? "
    "D9 40 28 D9 5D DC D9 40 2C";

// cGuiSet::DrawGfx(apGfx, avPos, avSize, aColor, aMaterial, afRotationAngle,
//                  abUseCustomPivot, avCustomPivot)
constexpr const char* kDrawGfxPattern =
    "55 8B EC 83 EC 6C 53 56 8B F1 8B 8E 50 01 00 00 57 85 C9 0F 84 ?? ?? ?? ?? "
    "D9 41 08 D9 EE D9 C0 DD EA DF E0 DD D9 F6 C4 44 0F 8B ?? ?? ?? ?? "
    "D9 41 0C D9 C1 DA E9 DF E0";

// cLuxMapHelper::GetClosestEntity(avStart, avDir, afRayLength, afDistance,
//                                 apBody, apEntity) - the game's own
// interaction ray. Its distance is the depth the crosshair has to be drawn at.
constexpr const char* kGetClosestEntityPattern =
    "55 8B EC A1 ?? ?? ?? ?? 83 EC 18 56 8B F1 ?? ?? ?? ?? 00 00 8B 41 60 57 "
    "33 FF 3B C7 75 ?? 5F 32 C0 5E 8B E5 5D C2 18 00 8B 48 60 D9 05 ?? ?? ?? ?? "
    "8B 55 0C D9 5E 3C 89 7E 40";

// cLuxClosestEntityCallback::Reset seeds the distance with this when nothing is
// hit. A ray that comes back with it hit nothing, and the aim point is at
// infinity: project the direction instead of a point.
constexpr float kNoHitDistance = 9999999.0f;

}  // namespace AmnesiaHT::offsets
