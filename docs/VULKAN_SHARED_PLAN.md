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

## Step 1 — results (2026-11-09)

`hookshot --write-dmsh <dir>` (the writer at the end of `gpu/vk/vk_shader.cpp`, `dmsh_write_library`) over Bananza's seed
(22,549 stages), then `tools/dmsh_check` (BananaNative's `vka_shlib.c` reader compiled on Linux, `tools/dmsh_check.sh`)
with `spirv-val --target-env vulkan1.3 --uniform-buffer-standard-layout`:

| | first writer | writer v4 (current) |
|---|---|---|
| translated and written | 21,496 stages (8,920 vertex, 12,430 fragment, 146 compute) | same, 66 s |
| library size | 884 MB raw SPIR-V | **249 MB** (one zstd frame per module, `DMSH_VK_CODEC_ZSTD`) |
| refused by the file reader | 0 | 0 |
| `spirv-val` failures | 0 | **0** |
| bindings the replay cannot fill | 279 unknown + 47 `NOSLOT` | **0** |
| not written | 911 stages Hookshot's parser rejects (geometry / tessellation, which it never draws) | same |

What v2–v4 added, all on the Eden side (no change to BananaNative's replay or ABI):
- **Compute handles and pointers by the program's c0 layout** (`compute_c0_layout`): textures/images from the per-shader
  handle tables, storage buffers from the ssbo table or the uniform-pointer table (`UBO_PTR`), push-constant reads moved to the
  push bank (`DRVCB` reg 24).
- **Raw global memory through the phone's pool table** (`Profile::gmem_map`): the 47 compute programs that read memory through
  pointers (Hookshot's 3-region table at c0+0x880 on Linux) now declare `gmem_map` / `gmem_log` as their last storage buffers;
  Eden's global-memory fallbacks search the table by the 2 MiB chunk hint, then a branchless bisection, and load/store/atomic
  through a physical storage buffer pointer at address + delta, the sink taking misses; stores set their entry's bit in the log.
  48 programs use it, 31 store through it.
- **Specialization constants** (`Profile::dmsh_spec`): every vertex module carries SpecId 0 (`DMSH_VK_SPEC_GL_DEPTH`, the
  depth conversion under a select) and is flagged `F_GL_DEPTH`; every fragment module writing colour 0 carries SpecId 1
  (`DMSH_VK_SPEC_ALPHA_FUNC`) with a `SUPPORT` uniform buffer as its last binding (word 0 = the reference) and is flagged
  `F_ALPHA_TEST`. Sirit gained `SpecConstantFalse` / `SpecConstant`.
- **64 lanes**: emitted with `max_subgroup_size 64` and `warp_size_potentially_larger_than_guest`, header `subgroup_size 64`,
  so the replay pins fragment and compute to 64 on Adreno instead of rewriting every stage to one-lane subgroups.

Capabilities the modules declare (census over all 21,496): float controls (SignedZeroInfNanPreserve on all, DenormFlushToZero
on 10,552), Int16 5,095, Int8 2,996, Int64 136, Float16 110, DrawParameters 3,343, GroupNonUniform* 235–238,
PhysicalStorageBufferAddresses 48, DemoteToHelperInvocation 5,770, DepthReplacing 123. `vka_dev.c` enables every feature the
physical device reports (Vulkan 1.1–1.3 feature structs passed back as queried), so the open question is only whether the
Adreno reports them — float-controls properties and `uniformBufferStandardLayout` in particular.

Still open before a phone plays it (in order):
1. On the phone: `DM_VK_SHLIB=<this dmsh_vk.lib>` with `DM_VK_SHLIB_CHECK=1`, draws compared with the Ryujinx library. No
   phone is attached to this machine; the library is at `/mnt/Data/bananza-eden-lib/v4/v-0035e16073bf/dmsh_vk.lib`.
2. Module variants the Ryujinx library has and this one does not: `UINT` / `UINT_MIXED` colour outputs (fragment shaders drawn
   to integer attachments: Eden bakes the output type from the render target at run time, the library needs the per-entry
   variant), `DUAL` (dual-source blend, from the game's `dual_want.tsv`), `SHUFFLE_LDS` (Turnip without arbitrary shuffles).
3. Fragment-depth writes (123 `DepthReplacing` modules) keep the translated convention; the vertex stage's GL-depth constant
   does not reach them.
4. Geometry and tessellation stages (911): Hookshot's parser does not decode them; the Android replay draws them.
5. Step 2 of the plan: `scripts/pack_android.py --translator eden` calling this writer, and `bananative_env.py` without .NET.
