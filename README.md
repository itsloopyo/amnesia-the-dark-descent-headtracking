# Head Tracking for Amnesia: The Dark Descent

![Amnesia: The Dark Descent running with this mod](https://raw.githubusercontent.com/itsloopyo/amnesia-the-dark-descent-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Amnesia: The Dark Descent that moves the view with your head while your mouse or controller keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - head tracking moves the view; the mouse still controls where you turn and what you reach for
- **6DOF tracking** - yaw, pitch and roll plus positional lean, peek and duck
- **Works with any OpenTrack-compatible source** - webcam, phone app, or anything else that sends the OpenTrack UDP protocol

## Requirements

- [Amnesia: The Dark Descent](https://store.steampowered.com/app/57300/), on Steam or GOG.
- A tracking source that sends the OpenTrack UDP protocol on port 4242: a webcam through [OpenTrack](https://github.com/opentrack/opentrack/releases), a phone app, or a VR headset.
- Windows 10 or 11, 64-bit. The game is a 32-bit executable and the mod is built to match it.

## Installation

1. Download the latest `AmnesiaHeadTracking-vX.Y.Z-installer.zip` from [Releases](https://github.com/itsloopyo/amnesia-the-dark-descent-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`.
4. Configure OpenTrack to output UDP to `127.0.0.1:4242`.
5. Launch the game.

The installer detects Steam and GOG installs on its own. If it cannot find
yours, point it at the folder holding `Amnesia.exe` in either of these ways:

```powershell
# Set the path in the environment
$env:AMNESIA_TDD_PATH = "D:\Games\Amnesia The Dark Descent"
.\install.cmd

# Or pass it straight to the installer
.\install.cmd "D:\Games\Amnesia The Dark Descent"
```

### Manual Installation

To place the files by hand, extract `AmnesiaHeadTracking-vX.Y.Z-nexus.zip` into
the game folder, next to `Amnesia.exe`. It contains three files:

- `wininet.dll` - the ASI loader. Amnesia imports `wininet.dll`, so that is the
  slot the loader goes into. If another mod already installed a loader under
  that name, keep the one you have.
- `AmnesiaHeadTracking.asi` - the mod.
- `HeadTracking.ini` - the config file. The mod writes one with defaults if it
  is missing.

## Setting Up OpenTrack

The mod listens for pose data on UDP port `4242`, on every network interface.
One datagram is six little-endian 64-bit floats in the order
`x, y, z, yaw, pitch, roll`: position in centimeters, rotation in degrees, 48
bytes in total. Anything that sends that to that port drives the view.
OpenTrack's **UDP over network** output sends exactly this.

1. Install [OpenTrack](https://github.com/opentrack/opentrack/releases).
2. Pick a tracker under **Input**, using the notes below.
3. Set **Output** to **UDP over network**, host `127.0.0.1`, port `4242`.
4. Press **Start**. Tracking and the game can start in either order.

Centering is done in your tracker: OpenTrack's **Center** bind, the CENTER
button in a phone app, or SteamVR's reset.

### VR Headset Setup

1. Connect the headset to the PC over Air Link, Virtual Desktop, or a link cable.
2. Start SteamVR and confirm the headset is tracking.
3. In OpenTrack, set **Input** to the SteamVR tracker.
4. Leave **Output** on **UDP over network**, host `127.0.0.1`, port `4242`.

### Webcam Setup

OpenTrack ships a `neuralnet tracker` input that reads a plain webcam, with no
markers to wear and no IR hardware to buy. Select it under **Input**, pick your
camera in its settings, and use the output settings above. How well it tracks
depends on your camera and your lighting, so try it before buying anything.

### Phone App Setup

A phone app can reach the mod directly, with no OpenTrack on the PC, if the app
sends the OpenTrack UDP datagram described above. Not every phone tracker does,
so check yours for an OpenTrack or UDP output option first. Point it at this
PC's IP address (run `ipconfig` to find it) on port `4242`.

What decides direct-send against going through OpenTrack is how much filtering
the app does before the packet leaves the phone. The mod's smoothing is sized to
take the edge off a clean signal rather than to rescue a noisy one, so a raw or
lightly filtered feed sent direct will jitter. The test takes a moment: send
direct, hold your head still, and watch the view. If it drifts or shakes, point
the app at OpenTrack's **UDP over network** *input* on another port, say `5252`,
and let OpenTrack's filters and curves clean the feed up before its output
forwards to `127.0.0.1:4242`. Route it through OpenTrack anyway if you want its
curve mapping.

I made [Headcam](https://headcam.app) so decent tracking was free for anybody
with a phone already in their pocket. It filters on-device, so it can send
direct. Any app that filters enough noise works exactly the same way.

Anything arriving from outside `127.0.0.0/8` counts as a remote connection and
is smoothed with `RemoteSmoothing` rather than `LocalSmoothing`. That includes a
tracker on this very PC that sends to the machine's own LAN address, because the
mod reads the source address and not the machine.

## Controls

| Action | Nav key | Chord |
|--------|---------|-------|
| Toggle tracking | `End` | `Ctrl+Shift+Y` |
| Cycle mode (6DOF / rotation / position) | `Page Up` | `Ctrl+Shift+G` |

The two columns are the same actions bound twice. Use whichever your keyboard
has: the nav-cluster keys, or the chords on a tenkeyless or laptop layout
without them.

## Configuration

`HeadTracking.ini` sits next to `Amnesia.exe`. Delete it to reset to defaults.

```ini
[Network]
; UDP port for OpenTrack data (default: 4242)
UDPPort=4242

[Sensitivity]
; Rotation sensitivity multipliers (1.0 = 1:1)
YawMultiplier=1.0
PitchMultiplier=1.0
RollMultiplier=1.0
; Smoothing is chosen per connection and covers rotation and position.
; LocalSmoothing applies when the tracker runs on this machine (loopback).
; 0.0 = no smoothing, 1.0 = heavy.
LocalSmoothing=0.0
; RemoteSmoothing applies when the tracker is a remote device on the network.
RemoteSmoothing=0.15

[Position]
; Enable/disable position tracking (6DOF)
Enabled=true
; Position tracking sensitivity (0.1-5.0, higher = more movement)
SensitivityX=1.0
SensitivityY=1.0
SensitivityZ=1.0
; Position limits in meters (how far the camera can move)
LimitX=0.30
LimitY=0.20
LimitZ=0.40
; Backward lean limit (prevents the camera clipping behind the player)
LimitZBack=0.10
; Invert position axes, for a tracker whose axis runs the other way.
InvertX=false
InvertY=false
; InvertZ is for a tracker that sends depth backwards, not for a lean that
; feels reversed. It is applied before the LimitZ / LimitZBack clamp, so
; turning it on also swaps the travel budgets to 0.10m forward and 0.40m back.
InvertZ=false

[Hotkeys]
; Virtual key codes (hex). Every action also has a Ctrl+Shift+<letter>
; chord fallback (see Controls above): Toggle=Y, Position=G.
ToggleKey=0x23       ; End      - Enable/disable head tracking
TrackingModeKey=0x21 ; Page Up  - Cycle tracking mode (6DOF / rotation / position)

[General]
; Auto-enable tracking on game start
AutoEnable=true
```

## Troubleshooting

**Mod not loading**

- Look for `HeadTracking.log` next to `Amnesia.exe`. If it is not there at all,
  the loader never ran: confirm `wininet.dll` is in the same folder as
  `Amnesia.exe`.
- The log records whether the camera hook installed. If it reports that a
  signature was not found, the mod stays dormant and the game runs unmodified.
- If the game will not start, remove `wininet.dll` to rule the mod out.

**No tracking response**

- Confirm OpenTrack's **Output** is **UDP over network**, host `127.0.0.1`,
  port `4242`, and that you pressed **Start**.
- Press `End`, or `Ctrl+Shift+Y`, in case tracking was toggled off.
- Tracking only applies during gameplay. The main menu, the inventory, the
  journal and load screens all keep drawing the map behind them, and the view
  stays put in every one of them.
- The log records the first pose that arrives, so it answers whether any tracker
  data reached the mod at all.

**Jittery or unstable tracking**

- Raise `RemoteSmoothing` for a phone or another device on the network, or
  `LocalSmoothing` for a tracker running on this PC.
- A raw phone feed sent direct will jitter. Route it through OpenTrack so its
  filters and curves clean it up first.
- For a webcam, add light. The neuralnet tracker gets noisy in a dim room.

**Wrong rotation axis**

- Correct the axis in your tracker rather than here. OpenTrack's **Mapping**
  page inverts any axis, and doing it there keeps one profile behaving the same
  way across every game.
- `InvertX`, `InvertY` and `InvertZ` in the config cover head position only, for
  a tracker whose lean or depth axis runs backwards.

## Updating

Download the new release and run `install.cmd` again. Your config is preserved.

## Uninstalling

Run `uninstall.cmd`. This removes the mod files. The ASI loader is removed only
if this mod's installer put it there. Use `uninstall.cmd /force` to remove it
anyway.

## Building from Source

Requires Visual Studio 2022 with the C++ x86 toolset, CMake, and
[pixi](https://pixi.sh).

```powershell
git clone --recursive https://github.com/itsloopyo/amnesia-the-dark-descent-headtracking
cd amnesia-the-dark-descent-headtracking
pixi run update-deps     # vendor the x86 ASI loader
pixi run build-release
pixi run package         # build both release ZIPs
```

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details. Bundled third-party components
keep their own licenses, reproduced in full in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md), which ships in both release
ZIPs.

## Credits

- [Frictional Games](https://frictionalgames.com) - Amnesia: The Dark Descent and the HPL2 engine.
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) by ThirteenAG - loads the mod into the game.
- [OpenTrack](https://github.com/opentrack/opentrack) - the tracking protocol this mod speaks.
- [MinHook](https://github.com/TsudaKageyu/minhook) by Tsuda Kageyu, with the Hacker Disassembler Engine by Vyacheslav Patkov - function hooking.
- CameraUnlock Core - the shared head-tracking library behind these mods.

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Frictional Games.
Use at your own risk.
