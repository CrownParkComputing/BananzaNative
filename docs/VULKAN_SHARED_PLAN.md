# One Vulkan path for Linux (x86-64) and Android (ARM64), without .NET

Where the two codebases in this repository stand, what differs, and the order of work to share one renderer and one
shader translator between the x86-64 recompiled build (Hookshot: `host/`, `gpu/vk/`) and the ARM64 native build
(moceure's BananaNative: `android/bananative/dm/`). Written 2026-11-09 from both trees as they are; numbers are
line counts and measurements from this machine.

## What each side is today

| | Linux x86-64 (this tree) | Android ARM64 (`android/bananative`) |
|---|---|---|
| CPU | the game's ARM64 lifted to C by a64-kit, compiled with clang (`nx2/`) | the game's own ARM64 code, rewritten in place (`dm/tools/patch_nso.py`) |
| OS | `host/*.c`: Horizon HLE on a Win32 shim over POSIX (`host/posix/`) | `dm/host/*.c`: Horizon HLE on Bionic (`plat_android.c`) |
| NVN | `host/nvn_native.c` (3,008 lines) records the game's NVN2 calls into an op stream | `dm/gfx/nvn/` + `dm/gfx/hle_nv.c` (3,845 lines) do the same |
| Replay | `gpu/vk/` (26,080 lines C++): NVN op stream → Vulkan 1.3, timeline semaphores, guest memory imported (host pointer / dma-buf), mesh shaders, cooperative matrices | `dm/gfx/vk_android/` (20,364 lines C): the same job for Adreno (Qualcomm and Turnip), push descriptors, dma-buf aliasing, 64-lane subgroups |
| Shaders | **Eden's C++ shader recompiler at run time** (`gpu/vk/eden/shader_recompiler`, `vk_shader.cpp translate()`): Ampere → Maxwell lowering → SPIR-V, per draw state, with a seed pre-translation (`--prepare-shaders`) and a program cache | **Ryujinx's C# translator offline** (`dm/third_party/ryujinx`, run by `vk_android_build.py` on the .NET 10 runtime): one library `dmsh_vk.lib` per game, loaded by `vka_shlib.c` |
| Status | OoT, Bananza, MKW run (this repository) | Bananza, MKW, Pokopia run on Snapdragon phones; OoT experimental |

Both replays are solving the same problem and have each fixed things the other has not (Bananza's terrain pointer
stores and GOB order on the Android side; the mirror/capture write-back model, conditional rendering and the OoT
lighting fixes on ours). Neither can replace the other wholesale: the Android one is written against Adreno's limits
(32 KiB shared memory, 64-lane subgroups, no arbitrary shuffle on Turnip), ours against desktop NVIDIA/AMD.

## The .NET dependency, precisely

It is **only the offline shader translator**. The Android runtime itself is C and Vulkan. The translator's output
contract is `dm/gfx/nvn/dmsh_abi_vk.h` (the `dmsh_vk.lib` format: entries keyed by the 8-byte NVN program key, one
SPIR-V module per variant — base, shuffle-through-LDS, … — plus **binding records** that tell the replay what each
descriptor is: `DMSH_VK_KIND_CBUF/C1/DRVCB/SBUF/SBUF_UBO/UBO_PTR/TEX/TBUF/IMG/SAMP*/TEX_UBO/TEX_C2A/TEX_POOL/GMEM_MAP`,
in fixed descriptor sets: set 0 = per-draw resources, 64 bindings per stage; set 1 = the texture (2^20) and sampler
(2^12) pools). Anything that writes that file to that contract replaces .NET.

Eden's recompiler already produces, for every stage, `Shader::Info` (constant buffers used, storage buffers, textures
with how their handles are found, images, shared memory, workgroup size) and SPIR-V. Our `vk_shader.cpp` turns that
into Vulkan descriptor layouts at run time; `vk_pipeline.cpp` binds them. So the mapping Eden-Info → `dmsh_vk_bind`
records is the core of the work, not a new translator.

## Plan

1. **Library writer on Eden** — `gpu/vk/tools/dmsh_vk_write.cpp` (new): a host-side program linking the existing
   `gpu/vk/eden` recompiler and `vk_shader.cpp`'s seed path (`seed_translate`, the `hookshot_shaders.bin` census of
   `nx2/shader_census.py`, which already walks Bananza's and MKW's archives) and writing `dmsh_vk.lib` per
   `dmsh_abi_vk.h`. Steps inside it:
   - Eden translation with Android device answers (`DMSH_VK_SUBGROUP_SIZE` 64, `DMSH_VK_SSBO_ALIGN` 64,
     `DMSH_VK_SHARED_MAX` 32 KiB spill) — these are `host_info()` fields in `vk_shader.cpp` today.
   - `Shader::Info` → binding records. Most kinds map directly (constant buffers → `CBUF`/`C1`/`DRVCB`, storage
     buffers → `SBUF`/`SBUF_UBO`/`UBO_PTR`, textures by handle source → `TEX`/`TEX_UBO`/`TEX_C2A`/`TEX_POOL`); the
     c0/c2 driver-bank layout is the same NVN2 one on both sides (`dmsh_abi.h` vs `driver_bank_word`).
   - Descriptor set numbering rewritten in the SPIR-V to the fixed set-0/set-1 shape (`vk_android_spv.py` does this
     in Python today; it is OpDecorate rewriting, ~200 lines in C++).
   - Variants: base; `SHUFFLE_LDS` for compute shaders with subgroup shuffles (Turnip); instanced and one-lane
     variants are made by the replay itself from the base module (`vka_spv_*`), so they are not the writer's.
   - Validation with `spirv-val` (SPIRV-Tools, no .NET) and the library's `--check` reader (`vka_shlib.c` compiles
     on the Mac already as a unit test; it compiles on Linux too).
   Acceptance: `DM_VK_SHLIB_CHECK=1` run of the existing Android replay against the Eden-built library on the phone:
   same draws and dispatches as the Ryujinx-built one, no refused modules. Bananza first (22,852 shaders, both
   translators have decoded them all), then MKW.
2. **Packer without .NET** — `scripts/pack_android.py` gains `--translator eden` (default once 1 passes): the code
   rewrite stays Python (Capstone), the shader step calls the writer. `bananative_env.py` stops installing .NET.
3. **Share the fixes, not the files** — keep the two replays, with a `docs/REPLAY_PARITY.md` ledger of the behaviours
   each has and the other needs (today's known deltas: Android's gmem pool search for terrain pointer stores and
   GOB order; ours: capture write-back model, `RX_VK_CAPTURE_WAIT` semantics, conditional rendering, cooperative
   matrices on desktop). Merging the replays into one source is not worth it: the Adreno constraints and the desktop
   extensions pull the code in different directions, and both move weekly upstream.
4. **Upstream tracking for both** — `scripts/upstream-check.sh` already tracks NotRexed/Hookshot; add moceure's
   `android` branch (`git subtree pull --prefix android/bananative … android --squash`) and a second recorded SHA,
   and the daily workflow reports both.
5. **Later**: the Linux host's game answers that the Android host has (`dm/host/hle_*.c`) are being ported case by
   case as games need them (done for Bananza on 2026-11-09: DLC, offline services, time, hid, the 1080p window);
   the reverse direction (our `BroadcastConditionVariable`, `SdkAllocateTlsSlot`, the movie-player buffer protocol)
   is worth offering upstream to moceure.

## What is deliberately not in the plan

- A Godot (or any engine) renderer: both ports replay the game's own GPU command stream — its shaders, render
  targets, indirect draws, GPU-written memory the CPU reads back. There is no scene to hand an engine, and an
  engine's rendering device hides what the replay depends on (guest memory import, mesh shaders, cooperative
  matrices, raw SPIR-V with the game's layouts, conditional rendering, timeline semaphores).
- Running the x86 recompiled C on ARM64: possible in principle (the lifted C is portable) but strictly worse than
  running the game's own ARM64 code natively, which the Android side already does.

## Step 1 — first result (2026-11-09)

`hookshot --write-dmsh <dir>` (the writer at the end of `gpu/vk/vk_shader.cpp`, `dmsh_write_library`) over Bananza's seed
(22,549 stages), then `tools/dmsh_check` (BananaNative's `vka_shlib.c` reader compiled on Linux, `tools/dmsh_check.sh`)
with `spirv-val --target-env vulkan1.3 --uniform-buffer-standard-layout`:

| | |
|---|---|
| translated and written | **21,496 stages** (8,920 vertex, 12,430 fragment, 146 compute) in 39 s, 884 MB of raw SPIR-V |
| refused by the file reader | 0 entries, 0 unreadable modules |
| `spirv-val` failures | **0** (21,001 without the uniform-buffer-standard-layout rule: Eden lays constant buffers out as `u32[]` with stride 4) |
| bindings | 147,242: cbuf 68,158, tex 70,501, sbuf 3,435, drvcb 2,594, c1 2,121, img 154, **unknown 279 in 129 entries** |
| not written | 911 stages Hookshot's parser rejects (geometry / tessellation, which it never draws); 0 with cooperative matrices |

What is left before a phone can play it (in order):
1. The Android device must enable what Eden's modules assume: `uniformBufferStandardLayout` (Vulkan 1.2), 8/16-bit storage and
   int8/16/64/float16 as the Adreno reports them (`vka_dev.c` asks for bufferDeviceAddress and update-after-bind today).
2. The 129 entries with an unknown binding: compute-stage texture/image handles (Eden reads them from c0's per-shader
   layout; the record needs the slot the host binds, as `dmsh_build.py` derives it) and handles Eden resolves at draw time.
3. Variants and specialization constants the replay expects of a library (`DMSH_VK_SPEC_GL_DEPTH`, the alpha test, UINT
   colour outputs, dual-source): today the base module only, depth convention left as translated.
4. Header `subgroup_size` 32: the replay pins fragment/compute to 32 lanes where the device allows and rewrites the vertex
   pipeline to one-lane subgroups otherwise; 64-lane emission (Eden `max_subgroup_size`) is the alternative.
5. zstd modules (`DMSH_VK_CODEC_ZSTD`) to bring 884 MB down to ~200.
6. On the phone: `DM_VK_SHLIB=<this dmsh_vk.lib>` with `DM_VK_SHLIB_CHECK=1`, draws compared with the Ryujinx library.
