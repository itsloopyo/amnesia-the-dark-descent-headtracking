@echo off
:: ============================================
:: Amnesia: The Dark Descent - Install
:: ============================================
:: Thin wrapper - install body lives in cameraunlock-core/scripts/install-body-asi.cmd.

:: --- CONFIG BLOCK ---
set "GAME_ID=amnesia-the-dark-descent"
set "MOD_DISPLAY_NAME=Amnesia Head Tracking"
set "MOD_DLLS=AmnesiaHeadTracking.asi HeadTracking.ini"
set "MOD_INTERNAL_NAME=AmnesiaHeadTracking"
set "MOD_VERSION=0.0.0"
set "STATE_FILE=.headtracking-state.json"
set "FRAMEWORK_TYPE=ASILoader"
set "ASI_LOADER_NAME=wininet.dll"
set "MOD_CONTROLS=Controls:&echo   End - Toggle   PageUp - Cycle mode"
:: ASI_LOADER_NAME is the filename the ASI DLL is renamed to. Amnesia.exe
:: imports wininet.dll (but not winmm/dinput8/version), so the loader is
:: dropped in as wininet.dll. vendor/ultimate-asi-loader/dinput8.dll is the
:: bundled source (x86); install copies it to ASI_LOADER_NAME in EXE_DIR.
:: Bump it via `pixi run update-deps`.
:: --- END CONFIG BLOCK ---

:: Pin delayed expansion off before `%*` is expanded on the `call` below.
:: Under `cmd /V:ON`, or with DelayedExpansion=1 in
:: HKCU\Software\Microsoft\Command Processor, cmd.exe eats a `!` out of the
:: expanded line, and a real game path like C:\Games\Oh! My Game reaches the
:: body already mangled. The body pins expansion off at its own outer scope
:: too, but that is one `call` too late to save the argument it was handed.
setlocal disabledelayedexpansion

set "WRAPPER_DIR=%~dp0"
set "_BODY=%WRAPPER_DIR%shared\install-body-asi.cmd"
if not exist "%_BODY%" set "_BODY=%WRAPPER_DIR%..\cameraunlock-core\scripts\install-body-asi.cmd"
if not exist "%_BODY%" (
    echo ERROR: install-body-asi.cmd not found in shared\ or ..\cameraunlock-core\scripts\.
    echo If this is a release ZIP, re-download it from GitHub ^(corrupt installer^).
    echo If this is the dev tree, run: git submodule update --init --recursive
    exit /b 1
)
call "%_BODY%" %*
exit /b %errorlevel%