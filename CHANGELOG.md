# Changelog

## [Unreleased]

### Changed

- Settings move to `CameraUnlock.ini`, beside `arx.exe` (`EN\CameraUnlock.ini` in the Xbox Game Pass copy). Earlier versions of the mod kept these settings in `ArxFatalisHeadTracking.ini`, in the same folder. The first time this version starts and finds no `CameraUnlock.ini`, it reads your settings from `ArxFatalisHeadTracking.ini` and writes them into `CameraUnlock.ini`. It never changes `ArxFatalisHeadTracking.ini`, and does not read it again while `CameraUnlock.ini` exists.
- A setting that the defaults the README shows set to `default` is written as `default` when you never changed it from the default earlier versions used, because `ArxFatalisHeadTracking.ini` does not hold it or holds that default. It then follows `Defaults.ini`, so it takes the value `Defaults.ini` gives it, or the built-in value where `Defaults.ini` gives none, which can differ from the default earlier versions used. A setting you changed is written with the value imported for it, or as `default` where that value equals its default at that start.
- `RotationEnabled` and `PositionEnabled` are one setting here, the tracking mode, so both are written as `default` or neither is.
- Comments, and keys the mod never read, are not carried over. Nor are these, where your old file had them:
  - A sensitivity or axis inversion you changed from its default. Set these in your tracker instead.
  - `MoveCrosshair=0`. The crosshair always follows your aim now.
- An older version of the mod reads `ArxFatalisHeadTracking.ini` and never reads `CameraUnlock.ini`, so a setting you change after updating is not in `ArxFatalisHeadTracking.ini`.
- Deleting only `CameraUnlock.ini` makes the next start read `ArxFatalisHeadTracking.ini` again. To go back to the defaults, replace everything in `CameraUnlock.ini` with the defaults the README shows. Every setting they set to `default` then follows `Defaults.ini`.
- Hotkeys are written as key names, and each hotkey lists every key that triggers it, the Ctrl+Shift chord included: `ToggleKey=End, Ctrl+Shift+Y`. The cycle hotkey keeps `Ctrl+Shift+J`, because `G` is Arx's drink-mana-potion key: `CycleTrackingModeKey=PageUp, Ctrl+Shift+J`. Other mods in this series use `Ctrl+Shift+G`, so in this mod the row keeps these keys and does not follow `Defaults.ini`.
- Several settings have the fleet's names: `[Network] Port` is `UdpPort`, and `CollisionRadius` is `CollisionMargin`, still in Arx units from 2 to 200. The one vertical lean limit is now two, `PositionLimitY` upward and `PositionLimitYDown` downward, and an old `PositionLimitY` is imported into both.
- The tracking mode the mode hotkey picks is saved in `CameraUnlock.ini` and comes back at the next start.
- Uninstalling leaves `CameraUnlock.ini` and `ArxFatalisHeadTracking.ini` in place, so a reinstall keeps your settings. Earlier versions removed `ArxFatalisHeadTracking.ini`.

### Added

- A setting set to `default` in `CameraUnlock.ini` takes its value from `Defaults.ini`, which every head tracking mod that keeps its settings in `CameraUnlock.ini` reads. Head tracking mods that keep their settings in another file do not read it, and neither do earlier versions of this mod. Writing a value in place of `default` changes that setting for this game only. When the mod saves a setting that a hotkey changed in game, it writes the new value in place of `default`, so that setting no longer follows `Defaults.ini` in this game until you set it to `default` again.
- `Defaults.ini` is `%AppData%\CameraUnlock\Defaults.ini` on Windows; `$XDG_CONFIG_HOME/CameraUnlock/Defaults.ini` on Linux, or `~/.config/CameraUnlock/Defaults.ini` where `XDG_CONFIG_HOME` is not set, under Wine and Proton too; and `~/Library/Application Support/CameraUnlock/Defaults.ini` on macOS. The mod's log, where it writes one, names the file it read.
- When the mod starts and finds no `Defaults.ini`, it creates one holding the built-in values, unless Windows runs the game as a packaged app. The mod never changes `Defaults.ini` after that.

### Fixed

- Leaning no longer puts the view inside doors, chests, furniture or other characters. The lean used to stop only at the level's own walls and floors.
- Leaning past the edge of a doorway or pillar holds the view the same distance off the edge as off a flat wall, instead of letting it slide right up to it.

### Removed

- `MoveCrosshair`. The cursor and the crosshair always follow your aim.
- The sensitivity and axis inversion settings. Set these in your tracker app instead.
- With these settings at their shipped defaults the camera moves as it did before.

## [0.0.0] - 2026-09-08

### Added

- Added head tracking for Arx Fatalis. Your head moves the view; the mouse
  still turns your character and still decides where arrows, spells and sword
  swings go.
- Added support for the Microsoft Store copy alongside the Steam one. They are
  different builds of 1.21, and the mod recognises both and still stays dormant
  on anything else. The Microsoft Store version keeps a separate copy of the
  game per language, so see the README if you play in something other than
  English.
- Added 6DOF, so leaning moves the eye, with separate limits for each direction
  and a smaller budget backwards so pulling away does not put the view inside
  your own body.
- Added redrawing of the cursor and the crosshair on the world point your
  character is aimed at, with the cursor picking up whatever is under it there,
  so what the mark is on is what you hit and what you interact with, at any
  distance.
- Added a clamp that holds a lean against the level, so pushing your head into
  a wall stops at the wall instead of putting the view through it.
- Added zoom compensation, so head movement keeps the same effect on the
  picture while a bow is drawn or Magic Sight is up, both of which narrow the
  game's field of view.
- Added `FieldOfView`, which sets the vertical field of view that Arx itself
  gives no way to change. The world, the sprites and the aim mark all move
  together with it, drawing a bow still narrows the view by the same
  proportion, and head movement is scaled so it covers the same amount of
  screen at any setting.
- Added a gameplay gate, so tracking stands down outside ordinary first-person
  play: menus, the book and character sheet, conversations, cutscenes, and any
  time the camera leaves the player.
- Added hotkeys: `End` or `Ctrl+Shift+Y` toggles tracking, and `Page Up` or
  `Ctrl+Shift+J` cycles 6DOF, rotation only and position only.
- Added window centring, so windowed play starts with the game window centred
  on the monitor it opened on. Arx leaves the placement to Windows, which puts
  a 1024x768 window near the top-left corner. A window that is already centred,
  and a fullscreen one, are left where they are.
