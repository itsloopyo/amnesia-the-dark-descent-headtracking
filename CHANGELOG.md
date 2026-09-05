# Changelog

All notable changes to this project are documented here.

## [Unreleased]

### Changed

- Strip the third-party DLLs Ultimate ASI Loader carries as resources out of the vendored copy. The upstream 32-bit build embeds `binkw32.dll` (RAD Game Tools' Bink and Smacker 1.994i, proprietary middleware licensed per title), `wndmode.dll` (DirectX Windower Embedded, (C) 2008 VEG and (C) 2004 menopem, no licence) and `vorbisfile.dll` (Xiph.Org, BSD-3-Clause) so that a user who renames the loader over one of those libraries still gets the original exports. Both release ZIPs ship that binary, so both were redistributing all three. `scripts/strip-loader-payload.ps1` now zeroes them, `pixi run update-deps` runs it on every refresh, and `pixi run package` refuses to build a ZIP from a loader that still has them. Only the `.rsrc` section changes: the loader's code, imports, relocations and appended PDB are byte-identical to upstream, and nothing in this mod could reach the stripped resources anyway
- Correct `THIRD-PARTY-NOTICES.md`: it pinned cameraunlock-core at a commit the submodule no longer points at, said the vendored loader was taken from upstream untouched, and claimed in one section that no byte patterns from the game were stored here and derived by independent analysis, while another section correctly said six machine-code signatures are and that Frictional Games' GPLv3 source release was the reference for the offsets

- Gate head tracking to the render phase. `cCamera::GetForward`, `GetRight`, `GetUp` and `UnProject` are all wrappers over `cCamera::GetViewMatrix`, and the injection used to modify the matrix every one of them returned. The interaction ray, the throw impulse on a grabbed body, the lever / door / wheel / slide interaction axes and the sound listener all read the camera through those, and `iLuxEnemy::IsSeenByPlayer`, `cLuxPlayerSanity::UpdateCheckEnemySeen` and `cLuxProp_Object::UpdateInsanityVision` test against `cCamera::GetFrustum`, so head tracking was moving monster behaviour, sanity drain and what the player could grab. Injection is now confined to the `GetViewMatrix` call `cCamera::GetFrustum` makes from inside `cScene::Render`, and the camera's cached view and frustum are marked dirty on the way out, so every gameplay read rebuilds them clean
- Suppress head tracking outside gameplay. Amnesia keeps the map rendering behind the main menu Escape opens, behind the inventory and the journal, and through a load screen, so the render-phase gate the camera hook already had said nothing about whether the player was playing - the view swung around under every one of them. The mod now reads `cLuxInputHandler::mState` off `gpBase` each render frame and injects only while it is `eLuxInputState_Game`, which covers the main menu, the inventory, the journal, load screens, the credits, the pre-menu and the debug menu in one test
- Take head roll as the tracker sends it. Yaw is still negated at the engine boundary, but roll was negated too, matching the rest of the fleet, and in game that leaned the view away from the head

- Removed recentring from the mod: the `Home` hotkey, the `Ctrl+Shift+T` chord, the `[Hotkeys] RecenterKey` setting and the handler behind them are gone. Every tracker app centres itself, so a mod-side centre was a second centre in series with the tracker's and the two drifted apart. The mod now applies the tracker pose as absolute; centre it in your tracker app.
- Replace the single `[Sensitivity] Smoothing` key with `LocalSmoothing` (default 0.0) and `RemoteSmoothing` (default 0.15), selected per connection from the packet source address
- Remove the `[Position] Smoothing` key: position now uses the same connection-selected value as rotation
- Remove the hidden 0.15 baseline smoothing floor, so local trackers get zero-latency tracking by default
- Rename the log file from `AmnesiaHeadTracking.log` to `HeadTracking.log`, matching `HeadTracking.ini` beside it. `uninstall.cmd` removes the old name too

### Added

- Centre the game window on its monitor's work area at startup. In windowed mode HPL2 leaves the position to SDL, and across launches it came up both centred on the display and with the title bar 26 px above the top of the screen, where the window cannot be dragged back down. A window that already fills the work area, so borderless or fullscreen, is left where it is
- Move the crosshair onto the point the interaction ray lands on. Amnesia draws its crosshair at a fixed screen centre, and it is the interaction marker - its shape follows what the ray is on and pressing use acts on that - so once the head turns or leans away from the aim, screen centre stops marking it. The crosshair is now projected through the same basis the camera hook wrote into the view matrix, at the live depth of the game's own `cLuxMapHelper::GetClosestEntity` cast: no fixed convergence distance, no smoothing, no stale depth. With nothing hit, or with a player state that is not casting, the clean aim direction is projected instead; when the head turns past the aim entirely the crosshair is hidden rather than clamped to a screen edge
- Ship the full licence text of every bundled component in both release ZIPs. The Nexus ZIP previously shipped `wininet.dll` (Ultimate ASI Loader, MIT) and an `.asi` with MinHook and the Hacker Disassembler Engine (both BSD-2-Clause) compiled in, carrying no notice at all, which those licences require of a binary distribution
- Credit Frictional Games' GPLv3 HPL2 source release in `THIRD-PARTY-NOTICES.md` as the source of the camera member names and conventions, and state explicitly that no HPL2 code is copied, adapted, linked, or redistributed here
- Name the Hacker Disassembler Engine (Vyacheslav Patkov) as a distinct copyright holder in `THIRD-PARTY-NOTICES.md`; it is compiled into every shipped `.asi` and was previously unattributed
- Record MinHook's vendored provenance and our local modifications to it in `extern/minhook/README.md`, so the copy is not mistaken for stock upstream
- Log a one-shot line the first time the `GetViewMatrix` hook runs and a one-shot line the first time tracker data arrives, so `HeadTracking.log` alone distinguishes a hook that never fired from a tracker that never sent

- Initial scaffold from cameraunlock-core templates (C++ ASI mod, x86 / HPL2).
