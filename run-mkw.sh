#!/bin/bash
# Mario Kart World (x86-64 Linux). ROMS: the drive with the game data; the dump's romfs is under it.
ROMS=${ROMS:-/mnt/Data}
DUMP=${DUMP:-$ROMS/mkw-dump}
# The capture write-back wait (an OoT exposure-meter fix) waits for a submission held behind the presenting thread in MKW:
# every wait ran to its 250 ms timeout, 4 fps. Off here: 45-60 fps in a race.
export RX_VK_CAPTURE_WAIT=${RX_VK_CAPTURE_WAIT:-0}
cd "$(dirname "$0")/work/sa/build" || exit 1
exec ./hookshot ../image/image.bin --romfs "$DUMP/romfs" --user "$HOME/MKW-recomp/work/user" "$@"
