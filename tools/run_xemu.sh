#!/bin/sh
# Boots build/xbox/fold.iso (tools/build_xbox.sh) in xemu, hidden on its own Xvfb display: nothing shows on the
# owner's screen. Needs the owner's MCPX ROM and BIOS dumps, which are never in this repo; by default they are read
# from the mh1-xbox repo's xbox_files/ (override with XBOX_FILES=dir holding mcpx.bin and bios.bin). xemu's own
# config and a blank scratch disk image live in build/xbox/xemu/.
#   tools/run_xemu.sh --shot out.png --after "20 30"   screenshots at those seconds, then quit
#   --record out.mp4   also record the display until the last screenshot
#   --show             on the desktop instead (by hand only)
# The XBE's log lines (if any) come out of the emulated serial port: build/xbox/xemu/serial.txt.
set -e
cd "$(dirname "$0")/.."
XF=${XBOX_FILES:-"$HOME/claude projects/MH XBOX/mh1-xbox/xbox_files"}
XEMU=${XEMU:-$HOME/xboxdev/xemu/xemu-0.8.136-x86_64.AppImage}
SHOT=""; AFTER=30; SHOW=; REC=
while [ $# -gt 0 ]; do
    case "$1" in
    --shot) SHOT=$2; shift 2 ;; --after) AFTER=$2; shift 2 ;; --record) REC=$2; shift 2 ;;
    --show) SHOW=1; shift ;; *) echo "unknown option $1"; exit 2 ;;
    esac
done
for f in mcpx.bin bios.bin; do [ -f "$XF/$f" ] || { echo "missing $XF/$f (the owner's own dumps)"; exit 1; }; done
ISO=$PWD/build/xbox/fold.iso; [ -f "$ISO" ] || { echo "no $ISO: run tools/build_xbox.sh"; exit 1; }
W=$PWD/build/xbox/xemu; mkdir -p "$W/home/xemu/xemu"
[ -f "$W/hdd.img" ] || truncate -s 8G "$W/hdd.img"
cat > "$W/home/xemu/xemu/xemu.toml" <<TOML
[general]
show_welcome = false
[sys.files]
bootrom_path = '$XF/mcpx.bin'
flashrom_path = '$XF/bios.bin'
eeprom_path = '$W/eeprom.bin'
hdd_path = '$W/hdd.img'
dvd_path = '$ISO'
TOML
export XDG_DATA_HOME="$W/home"
DBG="$W/serial.txt"; : > "$DBG"
set -- -device lpc47m157 -serial "file:$DBG"
[ -n "$SHOW" ] || set -- "$@" -audio none
XVFB=${XVFB:-$HOME/xboxdev/xvfb/root/usr/bin/Xvfb}
. tools/emu_lib.sh
PID=; XPID=; RPID=
stop() { [ -n "$RPID" ] && kill -INT $RPID 2>/dev/null && emu_sleep 2; emu_stop $PID $RPID $XPID; }
emu_traps stop
if [ -z "$SHOW" ]; then
    DNUM=$(( 70 + $$ % 20 ))
    "$XVFB" :$DNUM -screen 0 1280x960x24 -nolisten tcp >/dev/null 2>&1 &
    XPID=$!; export DISPLAY=:$DNUM; emu_sleep 2
fi
if [ -z "$SHOT" ]; then "$XEMU" "$@" & PID=$!; wait $PID; exit; fi
"$XEMU" "$@" >"$W/xemu.log" 2>&1 &
PID=$!
if [ -n "$REC" ]; then
    ffmpeg -v error -y -f x11grab -framerate 15 -video_size 1280x960 -i "$DISPLAY" -vf scale=640:480 \
        -c:v libx264 -preset veryfast -crf 24 -pix_fmt yuv420p "$REC" </dev/null & RPID=$!
fi
T=0
for A in $AFTER; do
    emu_sleep $(( A - T )); T=$A
    OUT=$SHOT; [ "$A" = "$AFTER" ] || OUT="${SHOT%.png}_${A}s.png"
    ffmpeg -v error -y -f x11grab -i "$DISPLAY" -frames:v 1 "$OUT" </dev/null; echo "screenshot: $OUT"
done
