# Plan

Owner's decisions (2026-10-20): original Xbox (nxdk, like the MH1 port); a real-3D game + level editor on the TV, with
a VMX window that shows the 64x64 version and can playtest it, and later the real VMX in the controller's memory-unit
slot; levels as files every version reads; grid only (no 45-degree joins, no walking round a rim onto a back); this
repo is public. VMX hardware tests run on an M5Stack CoreS3 with the Faces gamepad (ESP32-S3, like the VMX).

## Steps

1. **C core** (`core/`): level files, the surface (rooms joined by turning about each edge), walking and pushing,
   buttons and room moves (flip, turn, shift, hinge), undo. Plain C99, no platform code, no allocation, small memory
   (it has to fit an ESP32-S3). Tests replay every level's solution; a parity test checks the C core against the
   JavaScript core move by move.
2. **PC build** (SDL2): the 3D view, the editor, the VMX window. Driven by scripts and screenshots in testing.
3. **Original Xbox build** (nxdk) from the same code; xemu headless, then the real console over the network loader.
4. **VMX link**: the Xbox sends the level and the pad to a VMX in the controller slot (ESP32-S3 USB device), which
   runs the same C core. First on the CoreS3 test rig. Check `XMU GADGET`'s board plan (memory-unit storage or not)
   before starting.

## Level files

See `docs/FORMAT.md`. The levels in `levels/` were exported from the JavaScript version
(`tools/export_levels.mjs`).
