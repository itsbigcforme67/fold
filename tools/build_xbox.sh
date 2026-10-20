#!/bin/sh
# Builds FOLD for the original Xbox: build/xbox/default.xbe, build/xbox/FOLD/ (XBE + assets + levels) and
# build/xbox/fold.iso. Needs nxdk (~/xboxdev, see ~/xboxdev/env.sh). make can't take spaces in paths, so this
# builds through a symlink, ~/xboxdev/links/fold -> this repo.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$HOME/xboxdev/links"; ln -sfn "$R" "$HOME/xboxdev/links/fold"
. "$HOME/xboxdev/env.sh"
F=$HOME/xboxdev/links/fold
cd "$F/xbox" && make F="$F" package "$@"
