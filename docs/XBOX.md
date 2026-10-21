# FOLD on the original Xbox

The same editor and game as the PC build (`app/`), built with nxdk. The software renderer's 640x480 picture is copied
to the framebuffer each frame (`app/main_xbox.c`); no GPU code yet.

## Build

    tools/build_xbox.sh                 # -> build/xbox/FOLD/ (default.xbe + assets/ + levels/) and build/xbox/fold.iso
    tools/build_xbox.sh DEMO=<script>   # also puts a pad script in as D:\demo.txt (played instead of the pad at start)

Needs nxdk and its clang/lld in `~/xboxdev` (`~/xboxdev/env.sh`, set up for the mh1-xbox port). make can't take the
space in this repo's path, so the script builds through a symlink, `~/xboxdev/links/fold`. Never `make clean` in
`xbox/`: nxdk's clean also deletes its shared libraries.

## Run in xemu (hidden)

    tools/run_xemu.sh --shot build/shots/x.png --after "20 40"     # screenshots at 20 s and 40 s
    tools/run_xemu.sh --record out.mp4 --shot x.png --after 275     # and a recording

xemu runs on its own Xvfb display, nothing on screen. It uses the owner's MCPX ROM and BIOS dumps from the mh1-xbox
repo's `xbox_files/` (never copied here; `XBOX_FILES=dir` for another place) and a blank scratch disk in
`build/xbox/xemu/`. The XBE logs to COM1 (`FOLD: started`, `FOLD: <n> fps` each second), which xemu writes to
`build/xbox/xemu/serial.txt`; a retail Xbox has nothing there and gets nothing.

## Install on the real Xbox (network loader)

Evolution X's FTP drops files bigger than about half a megabyte at random points (curl 426; FOLD's 1.2 MB XBE
failed four times in a row), as the mh1-xbox port found. So only a small loader goes over FTP, once:

    tools/xbox_upload.sh        # the owner types the FTP login; finds the console at .115 / .116 (XBOX_IP=...)

It puts `E:\Games\FOLD\default.xbe` (the loader, `xbox/loader/main.c`, 0.5 MB) and `loader.txt` (this laptop's
address, port 8770) on the Xbox. Then, with

    tools/xbox_serve.py         # on the laptop: serves build/xbox/FOLD (re-read on every request)

running, each start of FOLD from Play Games from Hard Drive asks the laptop for the build's version and, when it
changed, downloads the whole build in one request (`/pack`: fold.xbe, assets, levels; 2.2 MB), writes it next to
itself and starts `fold.xbe`. Two connections a start at most. Without the laptop it starts the copy on the disk.
Levels saved on the Xbox (new names) are kept. `tools/xemu_loader_test.sh` checks all of this in xemu on a scratch
disk (uses the mh1-xbox repo's `tools/xbox_fatx.py`); the HTTP client and the test DVD are copied from mh1-xbox.

## Controls (Duke)

Level view: the D-pad moves the cursor along the working plane; the left stick rolls the plane over the cursor cell's
edge, round the shape, and the camera rolls with it (the new cell is seen face up, four rolls one way come back).
White / Black raise the cursor a layer towards you / drop it one away. The right stick orbits freely, over the top and
underneath (up / down inverted); Back + right stick zooms; the triggers (L / R) turn the view a quarter turn; clicking
the left stick swings round to the other side of the cell. A and B work on the side you are looking at: A places a
cell, or a room on an empty side (the checkerboard); B clears a back room to nothing, or removes the cell (from its
front or an empty back). Y opens the side's room in the cell editor (an empty side: "A adds one"), X the cell's
settings, Start the menu, clicking the right stick shows the VMX window. A compass (top left) shows the world's axes.
Holding Back + Start shows the frame rate in the corner.

Cell editor: opens turned the way the cell lies on screen in the level view. The D-pad moves the cursor: past the
right edge onto the tile palette (there the D-pad picks, A takes the tile, B or left past the first column goes
back), past the other edges into the next cell. The left stick jumps a whole cell; White / Black go to the room a
layer above / below; the right stick, left / right, turns the room a quarter turn. A places, B makes floor, X the
tile's settings (X or B closes them), Y back to the level. Under the palette, the whole level turns slowly, the room
being edited outlined in yellow.

Cell settings (X): the cell's move in play (flip, turn, shift, hinge) and its signal, and layout tools for the side
being edited: mirror left / right, mirror top / bottom, turn a quarter, copy, paste (as the room lies on screen;
one-way arrows turn with it, edge doorways are matched up with the next cells again).

On the PC: keys as before, plus - / = zoom, G the left stick click; a modern pad's shoulder buttons zoom.

## Things found on the way

- nxdk's C library mishandles `sscanf("%*s ...")` (it stores into the next argument) and `ftell` on a disc file is
  unreliable: `app/padscript.c` avoids both. An access violation writing to 0x74696177 ("wait") was the clue.
- Frame time (PC, level view, `bench` in the headless runner): 6.25 ms -> 1.86 ms after caching the background
  gradient and the metal panels, integer colour mixing, a span rasterizer (stepped edge functions, one divide a
  pixel), the control icons redrawn only while used (or every other frame for the two with idle animation), and the
  shadow blur limited to where the shadow is.
- xemu's speed is not the console's: real frame rates need the hardware.
- Saving crashed the real Xbox: app_save had a 77 KB local copy of the level, and nxdk gives the main thread a 64 KB
  stack (the PC gives 8 MB). It is static now and the XBE asks for a 256 KB stack (`-stack:0x40000`). Checked in xemu
  on a writable scratch disk: the save lands in E:\Games\FOLD\levels and the game carries on.
