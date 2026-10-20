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

## Install on the real Xbox

    tools/xbox_upload.sh        # FTP to E:\Games\FOLD; the owner types the login (XBOX_IP=..., default 192.168.1.116)

Then Play Games from Hard Drive -> FOLD. Levels are read from and saved to `E:\Games\FOLD\levels`.

## Controls (Duke)

Left stick, D-pad, A B X Y, Start, Back as on the PC; L and R are the triggers or White / Black; clicking the right
stick shows the VMX window. Holding Back + Start shows the frame rate in the corner.

## Things found on the way

- nxdk's C library mishandles `sscanf("%*s ...")` (it stores into the next argument) and `ftell` on a disc file is
  unreliable: `app/padscript.c` avoids both. An access violation writing to 0x74696177 ("wait") was the clue.
- Frame time (PC, level view, `bench` in the headless runner): 6.25 ms -> 1.86 ms after caching the background
  gradient and the metal panels, integer colour mixing, a span rasterizer (stepped edge functions, one divide a
  pixel), the control icons redrawn only while used (or every other frame for the two with idle animation), and the
  shadow blur limited to where the shadow is.
- xemu's speed is not the console's: real frame rates need the hardware.
