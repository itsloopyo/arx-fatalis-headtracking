# Arx Fatalis Head Tracking

![Arx Fatalis running with this mod](https://raw.githubusercontent.com/itsloopyo/arx-fatalis-headtracking/main/assets/readme-clip.gif)

An unofficial head tracking mod for Arx Fatalis that moves the view with your head while your mouse keeps aiming, driven by a webcam, phone, or any OpenTrack compatible tracker, with no VR headset required.

## Features

- **Decoupled look and aim** - your head moves the view while the mouse still controls where you aim
- **6DOF tracking** - yaw, pitch and roll plus positional lean, peek and duck
- **Works with any OpenTrack compatible tracker** - free options available for PC, iOS and Android
- **Cursor sits on the aim point** - the mark you select with follows the shot, not your head

## Requirements

- [Arx Fatalis](https://store.steampowered.com/app/1700/Arx_Fatalis/) on Steam, or the
  Microsoft Store copy. The mod identifies the executable by its PE header and engages on
  the Steam and Microsoft Store builds of 1.21. On any other copy it stays dormant, leaves
  the game untouched, and says which way it differs in the log.
- A tracking source that sends the OpenTrack UDP protocol: [OpenTrack](https://github.com/opentrack/opentrack) with a webcam or VR headset, or a phone app that speaks the protocol itself.
- Windows 10 or 11. The mod loads into the game's 32-bit executable.

## Installation

1. Download `ArxFatalisHeadTracking-v<version>-installer.zip` from the [Releases page](https://github.com/itsloopyo/arx-fatalis-headtracking/releases).
2. Extract it anywhere.
3. Double-click `install.cmd`.
4. Configure OpenTrack to output UDP to `127.0.0.1:4242`.
5. Launch the game.

If the installer cannot find your copy of the game, point it at the install folder yourself. Either set the environment variable:

```powershell
$env:ARX_FATALIS_PATH = "D:\Games\Arx Fatalis"
.\install.cmd
```

or pass the folder as an argument:

```powershell
.\install.cmd "D:\Games\Arx Fatalis"
```

### The Microsoft Store copy holds one game per language

The Microsoft Store build installs a complete copy of the game for each of its six
languages, in `DE`, `EN`, `ES`, `FR`, `IT` and `RU` folders under
`Arx Fatalis (PC)\Content`, and its launcher starts the one matching the language you
play in. `install.cmd` deploys into `EN`.

If you play in one of the other five, copy `dinput.dll` and
`ArxFatalisHeadTracking.asi` out of the `EN` folder into the folder for the language you
play in. Without that the game starts normally and no head tracking appears, because the
loader is sitting beside an executable you are not running.

### Mod managers do not deploy this mod

Install it with `install.cmd`. Vortex and Mod Organizer 2 each deploy into one
fixed folder inside a game, and this mod's files have to sit in the game's root
directory next to `arx.exe` - the loader only looks there. A manager would put
them somewhere the loader never reads, report a successful install, and load
nothing.

### Manual Installation

Two files out of the installer ZIP go into the folder that contains `arx.exe`.
The rest of the ZIP is the installer, the licences and these documents:

- `vendor/ultimate-asi-loader/dinput8.dll` goes in **renamed to `dinput.dll`**.
  That is the import slot the game already has, and it is what loads the `.asi`.
  The file keeps its upstream name in the ZIP; the name it is copied to is what
  matters.
- `plugins/ArxFatalisHeadTracking.asi` goes in as
  `ArxFatalisHeadTracking.asi`.

The mod writes `ArxFatalisHeadTracking.ini` and `ArxFatalisHeadTracking.log`
beside them the first time it runs.

## Setting Up OpenTrack

Set **Output** to `UDP over network`, with the address `127.0.0.1` and port
`4242`. Any input OpenTrack supports works: a webcam through neuralnet or
AruCo, a VR headset, or a hardware tracker.

Centre your head in your tracker rather than in the game, with OpenTrack's
**Center** bind, SteamVR's reset, or the CENTER button in a phone app.

### VR Headset Setup

1. Connect the headset to your PC over Air Link, Virtual Desktop or a link cable.
2. Start SteamVR and confirm the headset is tracking.
3. In OpenTrack, set **Input** to the SteamVR tracker.
4. Leave **Output** on `UDP over network` at `127.0.0.1`, port `4242`.

### Webcam Setup

In OpenTrack, set **Input** to the neuralnet tracker and pick your webcam. It
reads your face directly, so there are no markers to wear and no IR hardware to
buy. Leave **Output** on `UDP over network` at `127.0.0.1`, port `4242`.

### Phone App Setup

A phone app is usable here if it sends the OpenTrack UDP protocol itself, or
ships a PC-side companion that does. Plenty of phone trackers speak something
else entirely, so check yours against that first.

For an app that does send it, what decides the wiring is how much filtering the
app does on the phone. An app that filters on-device can point straight at your
PC's LAN address on port `4242`, with UDP 4242 open in your firewall. A raw or
lightly filtered feed sent direct will jitter, because this mod's smoothing is
sized to take the edge off a clean signal rather than to rescue a noisy one, and
that app wants routing through OpenTrack so its filters and curves can clean the
feed up first. The test is quick: send direct, hold your head still, and if the
view drifts or shakes, route it through OpenTrack instead.

I made [Headcam](https://headcam.app) so decent tracking was free for anybody
with a phone already in their pocket. It filters on-device, so it can send
direct. Any app that filters enough noise works exactly the same way.

Smoothing is chosen per connection, from the address the packets arrive from.
Only loopback - `127.0.0.1` - counts as local. A phone on WiFi is a remote
connection and gets `RemoteSmoothing`, and so does a tracker running on this
same PC that sends to the machine's LAN address, because the mod sees a
transport rather than a machine.

## Controls

Two equivalent binding sets - use whichever your keyboard has:

| Action              | Nav-cluster | Chord           |
|---------------------|-------------|-----------------|
| Toggle tracking     | `End`       | `Ctrl+Shift+Y`  |
| Cycle tracking mode | `Page Up`   | `Ctrl+Shift+J`  |

`Page Up` / `Ctrl+Shift+J` cycles tracking mode:

1. Normal head-tracked gameplay
2. Positional tracking disabled, rotational tracking enabled
3. Rotational tracking disabled, positional tracking enabled
4. Back to normal

Both sets collide with something Arx already uses, so pick whichever you mind
less. Arx binds `End`, `Page Up` and `Page Down` to centre view, look up and
look down, so pressing one of those does both things at once. It also binds
`Ctrl` to magic mode and `Shift` to stealth mode, so holding either chord puts
you into both for as long as you hold it. The cycle chord is `Ctrl+Shift+J`
rather than the `Ctrl+Shift+G` other mods use because `G` is Arx's drink-mana-
potion key, and `J` is the next key in the same cluster that Arx leaves alone.
The nav-cluster keys can be changed: the mod's in `ArxFatalisHeadTracking.ini`,
the game's in Options.

There is no recenter key. Centring is done in your tracker - OpenTrack's Center
bind, or the CENTER button in a phone app - so there is only one centre to get
right.

## Configuration

`ArxFatalisHeadTracking.ini` is written next to `arx.exe` on first run, with
each section commented. Edit it with the game closed.

```ini
[Network]
; UDP port to listen on. 4242 is what OpenTrack sends to by default.
Port=4242

[General]
; Whether tracking is live as soon as the game starts.
EnableOnStartup=1
; Move the game's cursor onto the point you are aiming at. Turning this off
; leaves the cursor where the game puts it, which is wherever your head is
; pointed rather than where your character is.
MoveCrosshair=1
; Vertical field of view in degrees. Arx has no setting of its own and renders
; 75.95 degrees vertically, which widens horizontally on a wide monitor. 0
; leaves the game's own alone. 40 to 110 can be set, and the view still narrows
; when you draw a bow either way.
FieldOfView=0.0
; Write a line a second to ArxFatalisHeadTracking.log naming the camera the shot
; leaves from, the camera the frame is drawn through and the point the cursor is
; placed on. Only useful for reporting a problem.
Diagnostics=0

[Sensitivity]
; Shape the pose in your tracker, not here, so one profile behaves the same in
; every game. These stay at 1.0 unless you have a reason.
YawSensitivity=1.0
PitchSensitivity=1.0
RollSensitivity=1.0

[Inversion]
; Fix a mirrored axis in your tracker where you can, so it is right in every
; game at once.
InvertYaw=0
InvertPitch=0
InvertRoll=0

[Smoothing]
; Which of these applies is decided per connection, by the address the packets
; arrive from. Only loopback (127.0.0.1) counts as local: a tracker running on
; this same PC but sending to the machine's LAN address is treated as remote.
; Both cover rotation and position. 0 is no smoothing at all.
LocalSmoothing=0.0
RemoteSmoothing=0.15

[Position]
; Positional (6DOF) tracking - leaning. Limits are in metres.
PositionEnabled=1
; As above: shape the pose in your tracker. X is side to side, Y is up and
; down, Z is forward and back.
PositionSensitivityX=1.0
PositionSensitivityY=1.0
PositionSensitivityZ=1.0
; How far the view may move from where the game put it, in metres.
PositionLimitX=0.30
PositionLimitY=0.20
; Forward gets more room than backward so pulling back does not put the view
; inside your own body.
PositionLimitZ=0.40
PositionLimitZBack=0.10

; Stop a lean pushing the view through a wall. The trace uses the game's own
; level collision. CollisionRadius is how far off a surface the eye is held, in
; Arx units (1 unit = 1 cm), and must stay above the engine's 1-unit near clip
; or the wall is culled and you see through it anyway.
CollisionEnabled=1
CollisionRadius=18.0
CollisionReleaseSmoothing=0.9

[Hotkeys]
; Virtual key codes. Every action also has a Ctrl+Shift chord for keyboards
; with no navigation cluster.
;  End      / Ctrl+Shift+Y : tracking on or off
;  Page Up  / Ctrl+Shift+J : cycle 6DOF -> rotation only -> position only
;
; Both sets collide with something Arx already uses; pick whichever you
; mind less. End, Page Up and Page Down are centre view, look up and look
; down. Ctrl is magic mode and Shift is stealth mode, so holding a chord
; enters both for as long as you hold it.
; There is no recenter key. Centre your head in the tracker.
ToggleKey=0x23
CycleTrackingModeKey=0x21
```

### Field of view

Arx has no field of view setting of its own, and renders a fixed 75.95 degrees
vertically - about 92 degrees across on a 4:3 screen and about 108 on a 16:9
one, since the width is what stretches on a wider monitor.

`FieldOfView` changes it. The number is **vertical** degrees, between 40 and
110, and `0` leaves the game exactly as it was. It is applied to the engine's
own projection, so the world, the sprites, the lighting halos and the aim mark
all move together, and the game's own zoom still happens on top: drawing a bow
narrows the view by the same proportion at any setting.

Head tracking is scaled to match. A narrow view magnifies everything on screen,
head movement included, so without that the same head turn would sweep further
across the frame the moment the game zoomed. Whatever you set here becomes the
new normal, and turning your head moves the view by the same amount on screen at
40 degrees as at 110.

## Troubleshooting

**Mod not loading.** Check `ArxFatalisHeadTracking.log` next to `arx.exe`. If
the file does not exist the loader never ran: confirm `dinput.dll` is in the
same folder as `arx.exe`.

**The log says the game is newer than the mod knows about.** The mod matches the
executable by its PE header and stays completely dormant on a build it has not
been tested against, rather than hooking against addresses that have moved.
Check the releases page for an update.

**No tracking response.** Confirm OpenTrack's output is `UDP over network` at
`127.0.0.1:4242`, and that the log shows packets arriving. If the log says it
could not bind the UDP port, another game - or a second copy of this one - is
still running and holding it. The log line carries the error the OS returned.
The mod retries every half second and starts listening within about half a
second of whatever was holding the port closing, so closing the other game is
the whole fix and there is no need to restart Arx.

**The view does not move but the log says packets are arriving.** Head tracking
is suppressed outside ordinary first-person play: in menus, while the book or
the character sheet is open, during cutscenes and conversations, and any time
the camera leaves the player. Close the book, or leave the conversation.

**Jittery or unstable tracking.** Raise `RemoteSmoothing` if the tracker reaches
the game over the network, or `LocalSmoothing` if it runs on this PC and sends
to `127.0.0.1`. A phone sending a raw feed direct is the usual cause: route it
through OpenTrack so its filters clean the signal up before it arrives. Poor
lighting on a webcam produces the same shake, and is fixed at the camera.

**Wrong rotation axis: left and right are swapped.** Set `InvertYaw=1`. If
leaning is mirrored, check your tracker's own axis settings first - the tracker
owns the shape of the pose, and fixing it there fixes it in every game at once.

**Yaw feels wrong when I am looking up or down at a steep angle.** Turning your
head always turns the view about the vertical, whichever way the camera is
pitched, so looking at the floor and turning your head pans across it.

**The crosshair sits away from the middle of the screen.** That is the mod
working. It marks where your character is aiming, which stops being the middle
of the screen the moment your head moves.

**Everything looks too zoomed in, or too wide.** Set `FieldOfView` in
`ArxFatalisHeadTracking.ini`; see above.

**The game window moved when I launched.** By design, and only when you play
windowed: once the game has finished placing its window, the mod centres it on
the work area of the monitor it opened on. A window that is already centred, and
a fullscreen one, are left where they are.

## Updating

Download the new release and run `install.cmd` again. It overwrites the mod and
the loader and leaves `ArxFatalisHeadTracking.ini` alone, so your settings
survive.

## Uninstalling

Run `uninstall.cmd`. It removes the mod's files from the game folder. The ASI
loader (`dinput.dll`) is only removed if the installer put it there; use
`uninstall.cmd /force` to remove it anyway.

## Building from Source

```powershell
git clone --recursive https://github.com/itsloopyo/arx-fatalis-headtracking.git
cd arx-fatalis-headtracking
pixi run package
```

Needs Visual Studio with the C++ workload and CMake. The build is x86 and needs
nothing from a game install, so it works on a machine that does not own the
game.

## Community & Support

- [Discord](https://discord.com/invite/dxyZdyFNT9) - setup help, bug reports, and new-release announcements
- [Lopari](https://lopari.app) - free Windows launcher with one-click install and launch of head-tracking mods
- [Headcam](https://headcam.app) - free app that turns your phone into a head tracker

## License

MIT License - see [LICENSE](LICENSE) for details.

## Credits

- Arx Fatalis is by [Arkane Studios](https://www.arkane-studios.com/).
- [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) by
  ThirteenAG.
- [OpenTrack](https://github.com/opentrack/opentrack).

See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) for the full list.

## Disclaimer

This mod is not affiliated with, endorsed by, or supported by Arkane Studios or
ZeniMax. Use at your own risk. Requires a legitimately purchased copy of the
game.
