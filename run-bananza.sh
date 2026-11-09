#!/bin/bash
# Donkey Kong Bananza (x86-64 Linux). ROMS: the drive with the game data; the dump (exefs + romfs extracted from the .nup) is under it.
ROMS=${ROMS:-/mnt/Data}
DUMP=${DUMP:-$ROMS/bananza-dump}
cd "$(dirname "$0")/work/sa/build" || exit 1
exec ./hookshot ../image/image.bin --romfs "$DUMP/romfs" --user "$HOME/Bananza-recomp/work/user" "$@"
