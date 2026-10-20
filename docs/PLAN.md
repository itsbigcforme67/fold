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

## In progress (2026-10-20): live 3D control icons in the level view

Owner: the level view's control pictures (bottom-corner brushed-metal panels) should be REAL 3D, drawn live, moving
and exaggerating their point when a control is used (stick tilts / cursor slides; D-pad tips, ribbon arrows spin, the
planes turn; face buttons sink and glow; camera turns with the right stick). Curved arrows are 3D ribbons wrapping
round the D-pad / right stick, YELLOW ON BOTH SIDES (the red in the owner's sketch was only to show which side is the
back). Use the Duke controller models from the VMX film (XMU GADGET: tools/duke/, anim/duke/align_me.blend): the D-pad,
both sticks, the face buttons. Plan: export those objects with Blender headless (blender -b) to a small mesh asset
(triangles + flat colours) the C renderer loads; app/icons3d.c already has the panel scenes, ribbon arrows, the
grid/cursor, crossing planes and film camera built in code, and gfx has gfx_clip / gfx_zclear_rect / view offset.
Still to do: the export tool, a mesh loader, swap the stand-in models for the Duke ones, wire icons_update /
icons_* into app.c's level view (replacing level_controls' flat drawings), screenshot, commit.
