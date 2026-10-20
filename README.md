# FOLD

Sokoban on the outside of a 3D shape made of squares (working name). Every square of the shape is a room, and its
doorways lead over the edges into the next square, however the two squares meet. Buttons make rooms flip, turn,
shift or hinge. You never see the shape while you play. Clearing a level reveals it.

FOLD started as a 64x64 1-bit game for the VMX, a handheld shaped like an original Xbox memory unit. This repo is
where it grows into:

- **a portable C core:** rules, rooms, doorways, room moves and the level format, with no platform code;
- **an original Xbox game + level editor** (nxdk): real 3D, plus a VMX window that shows the 64x64 version; the
  level you build can be played on a VMX plugged into the controller;
- **a PC build** (SDL) of the same game and editor;
- **the VMX build** (ESP32-S3; tested on an M5Stack CoreS3 with the Faces gamepad).

The original JavaScript version lives in the microgames collection (`ui/games/foldrooms_core.js`); `docs/history/`
keeps snapshots of it. See `docs/PLAN.md`.
