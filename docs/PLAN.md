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

## Done (2026-10-20): live 3D control icons in the level view

The level view's corner panels (brushed metal) hold real 3D controls drawn live by app/icons3d.c, moving and playing
up their point when used: the stick tilts and the cursor slides on its grid; the D-pad tips, the one of its four curling arrows
(owner's sketch: they wrap round it like a ball) that way pops, and the planes turn; face buttons sink and glow; the camera turns with the right stick. Ribbon arrows are yellow on
both sides. The stick, D-pad and face-button models are the Duke parts from the VMX film (`XMU GADGET/anim/duke/
part_*.stl`), turned into assets/duke.mesh by tools/make_duke.py (Blender headless: weld, decimate to ~500-1800
triangles, icon frame). They are drawn with smooth per-corner light and a shine (gfx_tri_lit). Without the file the
icons fall back to simple built-in shapes.
