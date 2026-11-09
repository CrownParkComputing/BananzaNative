# BananzaNative — Donkey Kong Bananza (and Mario Kart World) on x86-64 Linux

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

## Android shader library without .NET

The Android build (`android/bananative`, moceure's BananaNative) loads its shaders from one library per game, `dmsh_vk.lib`,
which upstream builds with Ryujinx's C# translator on .NET. This tree writes the same file from Eden's C++ recompiler:

    # after the Linux build and the shader seed (nx2/shader_census.py → work/sa/build/hookshot_shaders.bin)
    cd work/sa/build
    HOS_HEADLESS=1 RX_VK_DMSH_GAME=040074A01BF12000 ./hookshot ../image/image.bin --romfs /mnt/Data/bananza-dump/romfs \
        --user ../../user --write-dmsh /mnt/Data/bananza-eden-lib        # → v-<chain>/dmsh_vk.lib + eden_report.txt
    tools/dmsh_check.sh                                                   # builds BananaNative's reader on Linux (once)
    tools/dmsh_check /mnt/Data/bananza-eden-lib/v-*/dmsh_vk.lib --val     # reader + spirv-val over every module
    android/bananative/scripts/pack_android.py <game> --shader-lib /mnt/Data/bananza-eden-lib/v-*/dmsh_vk.lib   # pack it, no .NET

`RX_VK_DMSH_RAW=1` keeps the modules uncompressed (zstd otherwise). What the library carries and what is still open is in
`docs/VULKAN_SHARED_PLAN.md`.

## Mario Kart World too

The same tree runs Mario Kart World (`0400C3F00006E000`, v917504 — the build Hookshot's runtime was first written for):
extract its `.nup` to `/mnt/Data/mkw-dump`, lift and build it in a second checkout (the lifted code is per game:
`OOT_WORK`/`work/dump.txt` pick the dump), prepare its shaders the same way, and start it with `run-mkw.sh`. That script
sets `RX_VK_CAPTURE_WAIT=0`: the capture write-back wait (an Ocarina of Time exposure fix) waits in MKW for a submission
held behind the presenting thread, so every wait ran to its 250 ms timeout — 4 fps; off, a race runs at 50-60 fps.
