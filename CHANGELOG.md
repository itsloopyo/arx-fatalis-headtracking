# Changelog

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
