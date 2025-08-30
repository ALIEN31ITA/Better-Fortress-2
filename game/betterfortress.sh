#!/usr/bin/env bash

gamedir=$(pwd)
script=$(readlink -f -- "$0")
pushd "$(dirname -- "$script")" > /dev/null

# Launch the game under the steam runtime
/home/$USER/.steam/bin32/steam-runtime/run.sh /home/$USER/.local/share/Steam/steamapps/common/Source\ SDK\ Base\ 2013\ Multiplayer/hl2.sh -game $gamedir/betterfortress "$@"

popd
