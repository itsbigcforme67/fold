# Process helpers for tools/run_xemu.sh (copied from the mh1-xbox repo, same owner).
#
#
# Why: xemu and PCSX2 are AppImages. The AppImage process runs the emulator itself (its name shows as "AppRun", not
# "xemu"), and its FUSE helper detaches into its own session (name "xemu-0.8.13", the file name cut to 15 characters).
# So `pgrep -x xemu` never matched a running xemu, and killing only the wrapper script's PID (a foreground `sleep` in a
# plain sh delays its EXIT trap; SIGTERM without a trap skips it) left the emulator running: an orphaned xemu ran 1 h at
# 400% CPU on 16 Oct 2026 and its Xvfb stayed.
#
#   emu_running FILE        true while a process started from that AppImage (or another file of that name) runs.
#                           pgrep -f with the first letter in brackets, so the pattern never matches a command line that
#                           only quotes it (this shell's, a caller's `sh -c "..."`); the FUSE helper's mount directory
#                           (/tmp/.mount_<first 6 letters>...) counts too.
#   emu_tree PID            PID and all its descendants (deepest first)
#   emu_stop PID...         SIGTERM to each PID's whole tree, SIGKILL after 5 s to what is left (xemu sometimes ignores
#                           SIGTERM, 11 Oct 2026)
#   emu_sleep N             sleep N seconds so that a trapped signal (TERM, INT, HUP) is handled at once, not after it
#   emu_traps FUNC          run FUNC on exit, then stop every child process still running (with its tree); exit the same
#                           way on TERM, INT and HUP. A trap only runs once the foreground command has ended: start
#                           long commands (run_xemu.sh, run_pcsx2.sh) with `cmd & wait $!` so a kill acts at once.
#   xemu_running, pcsx2_running   emu_running for the xemu / PCSX2 AppImage the scripts use

emu_running() {
    _b=$(basename "$1")
    _f=$(printf %.1s "$_b")
    _m=$(printf %.6s "$_b")
    pgrep -f "[$_f]${_b#?}|[.]mount_[$_f]${_m#?}" >/dev/null
}

emu_tree() {
    for _c in $(pgrep -P "$1"); do emu_tree "$_c"; done
    echo "$1"
}

emu_stop() {
    _p=""
    for _x in "$@"; do [ -n "$_x" ] && _p="$_p $(emu_tree "$_x")"; done
    [ -n "$_p" ] || return 0
    kill $_p 2>/dev/null || :
    for _i in 1 2 3 4 5 6 7 8 9 10; do emu_alive $_p || return 0; sleep 0.5; done
    kill -9 $_p 2>/dev/null || :
}

# true while one of the PIDs runs (a zombie, ended but not yet reaped by its parent, counts as ended)
emu_alive() {
    for _x in "$@"; do
        case "$(ps -o stat= -p "$_x" 2>/dev/null)" in ""|Z*) ;; *) return 0 ;; esac
    done
    return 1
}

emu_sleep() {
    sleep "$1" &
    wait $! || :
}

emu_traps() {
    trap "$1; emu_stop \$(pgrep -P $$ 2>/dev/null)" EXIT
    trap 'exit 143' TERM
    trap 'exit 130' INT
    trap 'exit 129' HUP
}

# the emulators the scripts use (XEMU= / PCSX2= override, as in run_xemu.sh / run_pcsx2.sh)
xemu_running() { emu_running "${XEMU:-$HOME/xboxdev/xemu/xemu-0.8.136-x86_64.AppImage}"; }
pcsx2_running() { emu_running "${PCSX2:-$HOME/ps2dev/pcsx2/pcsx2-v2.8.2.AppImage}"; }
