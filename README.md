# FOLD

Sokoban on the outside of a 3D shape made of squares (working name). Every square of the shape is a cell (a room),
and its doorways lead over the edges into the next cell, however the two meet. Buttons make cells flip, turn, shift
or hinge. You never see the shape while you play. Clearing a level reveals it.

**Words:** *level* = the whole level; *cell* = one room square; *tile* = one element inside a cell (wall, floor, box,
target, button, player). In the core's code a cell is a "square" (with two sides, "faces") and a tile is an array
"cell" (`face * 64 + y * 8 + x`).

FOLD started as a 64x64 1-bit game for the VMX, a handheld shaped like an original Xbox memory unit. This repo is
where it grows into:

- **a portable C core:** rules, rooms, doorways, room moves and the level format, with no platform code;
- **an original Xbox game + level editor** (nxdk): real 3D, plus a VMX window that shows the 64x64 version; the
  level you build can be played on a VMX plugged into the controller;
- **a PC build** (SDL) of the same game and editor;
- **the VMX build** (ESP32-S3; tested on an M5Stack CoreS3 with the Faces gamepad).

The original JavaScript version lives in the microgames collection (`ui/games/foldrooms_core.js`); `docs/history/`
keeps snapshots of it. See `docs/PLAN.md`.

## Status

- `core/`: the C core (C99, no allocation, ~60 KB of state). `make test` replays all 15 levels and undoes every
  move back to the start; `make parity` checks it against the JavaScript core move by move.
- `levels/`: the 15 levels as `.fold` files (`docs/FORMAT.md`), exported from the JavaScript version.
- Tiles: wall, floor, box, target, button, player, plus ice, one-way floor, pit, teleporter pads, pressure plate,
  gate (worked by buttons / plates), key and lock, player-only grate (`docs/FORMAT.md`; rules tested in
  `tests/tiles.c`, small test levels in `tests/levels/`). The JavaScript web version only has the original tiles.
- Signals and logic: inputs (buttons, plates, sensors, targets, keys, all targets filled) send signals, logic pieces
  combine them, outputs follow them (cell moves, gates, brick-look wall gates, teleporters, one-ways, lights). Timers
  (a tick every n moves; on after n moves, for step limits) and delay pieces (n moves late) make timed puzzles. Talking to a tile
  says what it is and does, worked out from the wiring (never which cell), unless the level gives it its own text.
- `app/`: the level editor + playtest + VMX window, on a small software renderer (640x480, the same picture on PC
  and the original Xbox). `make app` builds `build/fold` (SDL2 window) and `build/fold_headless` (scripted input,
  screenshots; used for testing). Run `build/fold levels`. Pad or keyboard (WASD stick, arrows D-pad, Z A, X B, C X,
  V Y, Q L, E R, Esc START, F right stick click).
- Next: the original Xbox build of the app (nxdk, its SDL2), real-3D play, then the VMX link. See `docs/PLAN.md`.

## Editor controls

Level view: left stick moves the cursor over the working plane; A places a cell (facing you), B removes it; the D-pad
folds the working plane 90 degrees about the cursor's edge, wrapping away from you round the shape, and the view turns
to face the new plane; L / R turn the view 90 degrees, the right stick turns and tilts it (BACK + right stick zooms);
X opens the cell on the side facing you; Y opens the cell's settings (its move, the button that triggers it, its back
side); START opens the menu (playtest, VMX window, save, load, new level); clicking the right stick pops the VMX up in the
middle of the screen (the cell under the cursor) or hides it.

Cell editor: left stick moves the tile cursor (off an edge it carries on into the next cell); the D-pad jumps to the
next cell; A places the tile (L / R pick), B clears to floor; a doorway on the outer ring opens in the cell beyond too;
Y opens the tile's settings (the signal it sends or is worked by, how a gate / teleporter starts, what it will say);
X goes back to the level view. START > WIRING: the logic pieces (and / or / xor / not / latch / count) and the
all-targets input.

Playtest (and the game): move with the D-pad or stick; A talks to the tile you face (or stand on); B undoes, hold B
restarts; START goes back.
