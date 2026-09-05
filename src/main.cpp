// Amnesia: The Dark Descent head tracking - entry point.
//
// Loaded as an .asi by Ultimate ASI Loader (renamed to wininet.dll). Spawns an
// init thread that opens the log, records the running EXE's PE fingerprint, and
// brings up the mod (config, UDP receiver, hotkeys, cCamera::GetViewMatrix hook).

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>

#include <string>

#include "mod.h"
#include "cameraunlock/logging/file_log.h"
#include "cameraunlock/os/game_window.h"
#include "cameraunlock/memory/pe_fingerprint.h"
#include "version.h"

static HANDLE g_initThread = nullptr;

static std::wstring LogPathNextToExe() {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring path(exePath);
    size_t slash = path.find_last_of(L"\\/");
    std::wstring dir = (slash == std::wstring::npos) ? L"." : path.substr(0, slash);
    return dir + L"\\HeadTracking.log";
}

static void ForwardWindowLog(cameraunlock::os::WindowLogLevel level, const char* message) {
    cameraunlock::logging::Line(
        "%s%s", level == cameraunlock::os::WindowLogLevel::Warning ? "WARNING: " : "", message);
}

// Where HPL2's window lands in windowed mode is not fixed: across launches on
// the same machine it has come up centred on the display and with its title bar
// 26 px above the top of the screen, where it cannot be dragged back.
//
// The wait is not optional: the init thread runs long before the engine has
// created the window, and CenterGameWindowOnce spends its single shot whether
// or not it found one.
static void CenterGameWindowWhenItExists() {
    for (int attempt = 0; attempt < 100 && !cameraunlock::os::FindGameWindow(); ++attempt) {
        Sleep(100);
    }
    cameraunlock::os::CenterGameWindowOnce(&ForwardWindowLog);
}

static unsigned __stdcall InitThread(void*) {
    Sleep(1500);  // let the process finish early init

    cameraunlock::logging::Open(LogPathNextToExe());
    cameraunlock::logging::Line("Amnesia Head Tracking v%s attached.", AmnesiaHT::VERSION_STRING);

    cameraunlock::memory::PeFingerprint fp{};
    if (cameraunlock::memory::ReadPeFingerprint(GetModuleHandleW(nullptr), fp)) {
        cameraunlock::logging::Line(
            "EXE fingerprint: TimeDateStamp=0x%08X SizeOfImage=0x%08X CheckSum=0x%08X",
            fp.TimeDateStamp, fp.SizeOfImage, fp.CheckSum);
    }

    AmnesiaHT::Mod::Instance().Initialize();

    CenterGameWindowWhenItExists();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    switch (reason) {
        case DLL_PROCESS_ATTACH:
            DisableThreadLibraryCalls(hModule);
            g_initThread = reinterpret_cast<HANDLE>(
                _beginthreadex(nullptr, 0, InitThread, nullptr, 0, nullptr));
            break;
        case DLL_PROCESS_DETACH:
            if (g_initThread) {
                WaitForSingleObject(g_initThread, 2000);
                CloseHandle(g_initThread);
                g_initThread = nullptr;
            }
            AmnesiaHT::Mod::Instance().Shutdown();
            cameraunlock::logging::Close();
            break;
    }
    return TRUE;
}
