#!/bin/bash
# The Legend of Zelda: Ocarina of Time (x86-64 Linux). ROMS: the drive with the game data; the dump's romfs is under it.
ROMS=${ROMS:-/mnt/Data}
DUMP=${DUMP:-$ROMS/hookshot-dump}
cd "$(dirname "$0")/work/sa/build" || exit 1
exec ./hookshot ../image/image.bin --romfs "$DUMP/romfs" --user "${USER_DIR:-$(dirname "$0")/work/user}" "$@"
