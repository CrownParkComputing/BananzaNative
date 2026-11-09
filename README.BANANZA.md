# BananzaNative — Donkey Kong Bananza on x86-64 Linux

A fork of [Hookshot](https://github.com/NotRexed/Hookshot) (NotRexed's static recompilation of Switch 2 games to native
PC code) that runs **Donkey Kong Bananza** (`040074A01BF12000`, update v458752) natively on x86-64 Linux with Vulkan.
Nothing from the game is in this repository: you need your own dump (`.nup`).

Status (2026-11-09): boots, plays the opening and the mining tunnel at 60 fps (dips in heavy terrain scenes), guide
movies play, saves work. See `host/nn_streamplay.c`, `host/nn_video.c` (the `dkb` paths), `host/nn_os_sync.c`
(`BroadcastConditionVariable`), `host/hos_thread.c` (`SdkAllocateTlsSlot`) for what the game needed beyond Hookshot.

## Build (Linux)

    sudo pacman -S clang lld sdl2 vulkan-headers libx11 freetype2 ffmpeg   # or the Debian equivalents in docs
    pip install --user capstone lz4 numpy zstandard
    # headers for the Vulkan back end, in the CPM layout nx2/build.py reads
    scripts/linux/deps.sh                      # or: fetch Vulkan-Headers, SPIRV-Headers, VMA into build-linux/deps/cpm/<name>/<tag>
    # the dump: exefs + romfs out of your .nup (the update's romfs is the whole, patched tree)
    python3 nx2/nup.py "/path/Donkey Kong Bananza [040074A01BF12000].nup" --title 458752 --extract /mnt/Data/bananza-dump
    echo /mnt/Data/bananza-dump > work/dump.txt; mkdir -p work/image
    export RX_CPM=$PWD/build-linux/deps/cpm RX_MARCH=native
    python3 nx2/lift.py --standalone --write
    python3 nx2/build.py --standalone --jobs 8
    python3 nx2/shader_census.py /mnt/Data/bananza-dump/romfs --out work/sa/build/hookshot_shaders.bin
    (cd work/sa/build && ./hookshot --prepare-shaders)      # once, ~15 min: no shader stalls in play
    ROMS=/mnt/Data ./run.sh

## Keeping up with upstream

`upstream` is NotRexed/Hookshot. `.upstream-sha` records the upstream commit this tree was last merged with.

    scripts/upstream-check.sh          # fetches upstream, lists what is new, and says whether a merge is clean
    scripts/upstream-check.sh --merge  # merges upstream/main into the current branch (our changes stay; conflicts stop it)

`.github/workflows/upstream-watch.yml` runs daily and opens (or updates) an issue titled "Upstream Hookshot has new
commits" when upstream moved past `.upstream-sha`. After a merge and a successful rebuild, `scripts/upstream-check.sh
--mark` records the new upstream commit.

Licence: GPL-3.0-or-later, as Hookshot.
