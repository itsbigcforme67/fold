#!/bin/bash
# xemu_loader_test.sh: FOLD's network loader (xbox/loader, docs/XBOX.md) in xemu, hidden, on a SCRATCH hard disk
# (build/xbox/loader_xemu/hdd.img). The disk gets E:\Games\FOLD\default.xbe (the loader) and loader.txt
# (10.0.2.2:PORT, the laptop as xemu's NAT network sees it), as tools/xbox_upload.sh leaves the real Xbox. A test
# DVD (xbox/testlaunch) starts the loader from the disk as the dashboard would. tools/xbox_serve.py serves the
# current build (tools/build_xbox.sh first).
#   1. laptop up, nothing on the disk: the loader fetches /version and /pack, writes the build, starts FOLD
#   2. laptop up again: the build is up to date, no /pack, FOLD starts
# Uses the mh1-xbox repo's tools/xbox_fatx.py (formats / reads the disk image) and its owner's BIOS files, read only.
set -e
cd "$(dirname "$0")/.."
T=$PWD/build/xbox/loader_xemu
MH="$HOME/claude projects/MH XBOX/mh1-xbox"
fatx() { python3 "$MH/tools/xbox_fatx.py" "$@"; }
PORT=8770
. tools/emu_lib.sh
tools/build_xbox.sh --loader >/dev/null; tools/build_xbox.sh --testlaunch >/dev/null
[ -f build/xbox/FOLD/default.xbe ] || { echo "no build: tools/build_xbox.sh"; exit 1; }
rm -rf "$T"; mkdir -p "$T"
truncate -s 8G "$T/hdd.img"; fatx format "$T/hdd.img" >/dev/null
printf '10.0.2.2:%s\n' $PORT > "$T/loader.txt"
fatx put "$T/hdd.img" build/xbox/loader/default.xbe Games/FOLD/default.xbe >/dev/null
fatx put "$T/hdd.img" "$T/loader.txt" Games/FOLD/loader.txt >/dev/null
SPID=
unserve() { [ -z "$SPID" ] || { kill "$SPID" 2>/dev/null || :; wait "$SPID" 2>/dev/null || :; SPID=; }; }
emu_traps unserve
python3 tools/xbox_serve.py $PORT > "$T/serve.log" 2>&1 & SPID=$!; sleep 1
boot() {                                    # boot RUN SECONDS
    printf 'E:\\Games\\FOLD\\default.xbe\n' > "$T/launch.txt"; fatx put "$T/hdd.img" "$T/launch.txt" launch.txt >/dev/null
    tools/run_xemu.sh --xbe build/xbox/testlaunch/default.xbe --net --hdd "$T/hdd.img" --shot "$T/run$1.png" --after "$2" >/dev/null 2>&1 & wait $! || :
    cp build/xbox/xemu/serial.txt "$T/serial_$1.txt"
}
FAILS=0
check() { if eval "$2"; then echo "PASS: $1"; else echo "FAIL: $1"; FAILS=$((FAILS + 1)); fi; }
boot 1 "25 45"
check "1: loader asked for the version" "grep -q 'GET /version' '$T/serve.log'"
check "1: loader downloaded the build" "grep -q 'GET /pack' '$T/serve.log'"
check "1: FOLD started (its serial line)" "grep -q 'FOLD: started' '$T/serial_1.txt'"
want=$(stat -c %s build/xbox/FOLD/default.xbe)
check "1: fold.xbe on the disk is the build" "fatx ls '$T/hdd.img' Games/FOLD | awk '\$1 == \"fold.xbe\" { print \$2 }' | grep -qx $want"
check "1: the loader is still default.xbe" "fatx ls '$T/hdd.img' Games/FOLD | awk '\$1 == \"default.xbe\" { print \$2 }' | grep -qx $(stat -c %s build/xbox/loader/default.xbe)"
n1=$(grep -c 'GET /pack' "$T/serve.log")
boot 2 "25 40"
check "2: up to date, no second download" "[ \$(grep -c 'GET /pack' '$T/serve.log') = $n1 ]"
check "2: FOLD started again" "grep -q 'FOLD: started' '$T/serial_2.txt'"
unserve
echo "screens, serial logs, server log: $T"
[ $FAILS = 0 ] && echo "all checks passed" || { echo "$FAILS checks failed"; exit 1; }
