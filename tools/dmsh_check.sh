#!/bin/bash
# Builds tools/dmsh_check from BananaNative's reader and zstd decompressor (no Vulkan, no .NET)
cd "$(dirname "$0")/.."
B=android/bananative
clang -O1 -o tools/dmsh_check tools/dmsh_check.c $B/dm/gfx/vk_android/vka_shlib.c $B/dm/third_party/zstd/zstddeclib.c -I$B/dm/gfx/vk_android -I$B/dm/gfx/nvn -Wno-everything && echo built tools/dmsh_check
