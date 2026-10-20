#!/bin/sh
# Builds FOLD for the original Xbox: build/xbox/default.xbe, build/xbox/FOLD/ (XBE + assets + levels) and
# build/xbox/fold.iso; --loader builds the network loader instead (build/xbox/loader/default.xbe). Needs nxdk (~/xboxdev, see ~/xboxdev/env.sh). make can't take spaces in paths, so this
# builds through a symlink, ~/xboxdev/links/fold -> this repo.
set -e
R=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$HOME/xboxdev/links"; ln -sfn "$R" "$HOME/xboxdev/links/fold"
. "$HOME/xboxdev/env.sh"
F=$HOME/xboxdev/links/fold
if [ "$1" = --loader ]; then shift; cd "$F/xbox/loader" && make F="$F" "$@"; exit; fi
if [ "$1" = --testlaunch ]; then shift; cd "$F/xbox/testlaunch" && make F="$F" "$@"; exit; fi
cd "$F/xbox" && make F="$F" package "$@"
