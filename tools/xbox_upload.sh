#!/bin/bash
# Installs FOLD on the real Xbox over FTP: build/xbox/FOLD (tools/build_xbox.sh) -> E:\Games\FOLD, which
# Evolution X lists under "Play Games from Hard Drive" as FOLD. Run by the owner, who types the FTP login (or has it
# in ~/.netrc as "machine <ip> login .. password .."); it is never stored. XBOX_IP=... for the console's address.
# A demo.txt from a test build is left out (it would play instead of the pad on every start).
set -e
cd "$(dirname "$0")/.."
XBOX=${XBOX_IP:-192.168.1.116}
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
for f in default.xbe assets/* levels/*; do
	echo "-> $f"; curl -sS "${AUTH[@]}" --ftp-create-dirs -T "$f" "$URL/$f"
done
echo "done: on the Xbox, Play Games from Hard Drive -> FOLD"
