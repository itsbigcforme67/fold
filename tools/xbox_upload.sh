#!/bin/bash
# Installs FOLD on the real Xbox over FTP: build/xbox/FOLD (tools/build_xbox.sh) -> E:\Games\FOLD, which
# Evolution X lists under "Play Games from Hard Drive" as FOLD. Run by the owner, who types the FTP login (or has it
# in ~/.netrc as "machine <ip> login .. password .."); it is never stored. The console is found by trying the
# addresses it has had (192.168.1.115, .116) for an open FTP port; XBOX_IP=... to name it.
# A demo.txt from a test build is left out (it would play instead of the pad on every start).
set -e
cd "$(dirname "$0")/.."
XBOX=${XBOX_IP:-}
if [ -z "$XBOX" ]; then
	for ip in 192.168.1.115 192.168.1.116; do
		if timeout 2 bash -c "</dev/tcp/$ip/21" 2>/dev/null; then XBOX=$ip; break; fi
	done
	[ -n "$XBOX" ] || { echo "no Xbox FTP server at 192.168.1.115 or .116: is it on (Evolution X running)? XBOX_IP=... to name it"; exit 1; }
fi
echo "Xbox at $XBOX"
SRC=build/xbox/FOLD
[ -f $SRC/default.xbe ] || { echo "no $SRC: run tools/build_xbox.sh"; exit 1; }
if grep -q "machine $XBOX" ~/.netrc 2>/dev/null; then
	AUTH=(--netrc)
else
	read -rp "Xbox FTP login: " u
	read -rsp "Xbox FTP password: " p; echo
	NETRC=$(mktemp); trap 'rm -f "$NETRC"' EXIT; chmod 600 "$NETRC"
	printf 'machine %s login %s password %s\n' "$XBOX" "$u" "$p" > "$NETRC"
	AUTH=(--netrc-file "$NETRC")
fi
AUTH+=(--ftp-port -)     # Evolution X's FTP server answers 502 to EPSV / PASV: active mode only
URL=ftp://$XBOX/${XBOX_DIR:-E/Games/FOLD}
cd $SRC
# Evolution X's FTP sometimes stops taking data mid-file (the laptop's send queue sits full): a file that makes
# no progress for 15 s is given up and sent again, up to 4 tries
put() {
	for try in 1 2 3 4; do
		curl -sS "${AUTH[@]}" --ftp-create-dirs --speed-limit 1 --speed-time 15 -T "$1" "$URL/$1" && return 0
		echo "   stalled, again ($try)"; sleep 2
	done
	echo "   could not copy $1"; return 1
}
for f in default.xbe assets/* levels/*; do echo "-> $f"; put "$f"; done
echo "done: on the Xbox, Play Games from Hard Drive -> FOLD"
