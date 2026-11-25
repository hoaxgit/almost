# Project Almost

A research compatibility layer that translates Xbox Series X|S (Scarlett) games to run on Windows x64. It is a static recompilation approach: Xbox game executables and DLLs are mapped and instrumented so they can operate through Windows APIs and a Vulkan backend.

## What this project does

- **Maps Xbox PE executables** on Windows — PE/COFF images loaded at high base addresses (0x10000000000+)
- **Forwards Xbox-specific imports** — D3D12 XS shims, XGameRuntime classes, DXGI, XAsync, XUser, XStore, XPackage, and more
- **Translates shaders** — RDNA2 DXBC → SPIR-V for graphics and compute pipelines
- **Provides overlay filesystem** — GAME:/, T:/, and C: drive mappings with marker-based deletion and copy-up staging
- **Runs a test suite** — 486+ unit tests covering shader translation, BVH, audio, BCpack, CPU reference, D3D12/XS, and PE loader

It does **not** distribute, provide, or host GTA VI game files, executables, assets, keys, or licenses. Users independently own a legitimate copy of GTA VI and prepare game data from their own Xbox Series installation.

## Current progress (PortWatch milestones)

The project uses [PortWatch](https://github.com/realdody/americas-recomp) milestone definitions to track what has been implemented, tested, and verified. Each milestone is a capability gate with a synthetic test; passing a milestone does not mean the game is playable — it means a defined set of Xbox APIs and shaders operate correctly through the layer.

| Milestone | Scope | Status | What it proves |
|---|---|---|---|
| **M0** | `D3D12XboxCreateDevice` and the measured device overrides, one direct queue, allocator/list, fence, fake DXGI factory/adapter/output, `ALLOW_DISPLAY` back buffers, frame events + `PresentX`, clear + present | **In progress** | COM interception, the known Xbox device entry point and ABI overrides, presentation, frame loop, clean shutdown. A window shows a stable clear colour for 60 frames, no Vulkan validation errors, every D3D12 call traced. |
| **M1** | Root signature with root constants (mask `0x03000000` flag byte on a copy), one translated VS+PS, PSO, vertex buffer in the arena, draw, push constants `s0..s31` + heap base | **In progress** | PSO translation, shader hash lookup, vertex input, root-constant packing, heap base, present. The triangle matches a CPU/GPU reference; error 0 comparison passes. |
| **M1.5** | The same, with real translator output: hand-written RDNA2 VS+PS, PSO found by FSSM hash, vertex fetch through a V# loaded from the user-data table, a root constant, a table in the arena | **In progress** | The push-constant, vertex-fetch and code-base contracts on translated code. Three draws; every pixel off the triangle's edge exactly matches in a float target, validation clean on supported GPUs. |
| **M2** | Descriptor heap (heap slots 9/10), `CreateShaderResourceView`, sampler, root descriptor table, hash-table insert, bindless sampling | **In progress** | The full D3D12 descriptor path and the shader's T#/S# contract. A textured quad matches a synthetic 8×8 gradient or a known texture with error 0; the hash table has the expected entries. |
| **M3** | Depth/stencil, multiple render targets, resource barriers, render passes, occlusion queries, fences, 2–3 frames in flight; run the game's first frames | **Not yet achieved** | Real frame structure and the first game code. The game reaches its first rendered frame (loading screen/menu) or stops on a single unsupported call with a clean report. |
| **M4** | Compute PSOs, UAVs, storage images, indirect dispatch, copy queues, async streaming | **Not yet achieved** | Asset loading and GPU compute. The world's streaming path runs without device loss; compute results match a CPU/reference check. |
| **M5** | Mesh shaders, `ExecuteIndirect`, bindless-table pressure, multi-threaded command lists, pipeline cache | **Not yet achieved** | World rendering. A stable open-world frame at any frame rate; then profile and optimise. |
| **M6** | Ray tracing/XDXR: standard AS builds or the shipped `XboxBvhBuilder` compute path; BVH node format; the builder's 51 console FSSM programs | **Not yet achieved** | RT shadows and any RT effects. RT output matches a reference; the builder's shaders run through the FSSM translator or a substitute; no unknown XDXR call remains. |
| **M7** | HDR, VRS, `PresentX` planes (2-plane HDR + GameDVR compose, `ImmediateThresholdPercent`), resize, alt-tab, `SuspendX`/`ResumeX`, frame pacing and `GetFrameStatisticsX` | **Not yet achieved** | Presentation polish. HDR and resize work on the target display; no validation errors over a long run. |

**Important**: Passing these milestones means the layer correctly translates Xbox APIs and shaders for synthetic tests. It does **not** mean the game reaches a first frame, menu, or gameplay — that requires the game's own root signatures, PSOs, and descriptors to operate correctly, which is a much higher bar.

## What works (implemented and tested)

The following subsystems have been implemented in Project Almost's own C++20 code, tested across the 26,503-shader corpus, and verified via the translator regression suite. They are **independent clean-room implementations** — no third-party source code is copied.

- **PE/COFF image mapping** for manually loaded Xbox executables at high base addresses
- **Import thunk classification** (HResult vs. Other) with `ALMOST_UNIMPLEMENTED=log|fail|stop` policy
- **GAME:/ drive mapping** to game directory with overlay support, marker-based deletion, copy-up staging
- **RDNA2 → SPIR-V translation** via `tools/rdna2spirv.py`; shard-based parallelization (--jobs, --shard)
- **FSPX (FidelityFX) compute program** unpacking and translation
- **BVH builder programs** (51 compute programs from XboxBvhBuilder.dll → SPIR-V)
- **DXGI import forwarding** (CreateDXGIFactory*, DXGIGetDebugInterface1, DXGIDeclareAdapterRemovalSupport)
- **D3D12 XS forwarding** (D3D12SerializeVersionedRootSignature, D3D12XboxCreateDevice with Xbox-ABI proxy)
- **XGameRuntime offline classes** (XSystem, XPackage, XNetworking, XPersistentLocalStorage, XError, XTaskQueue, XAsync, XUser, XStore, XGame, XGameSave)
- **File system console path parsing** (GAME:, T:, C: drives; FindFirstFile/FindNextFile merging)
- **Persistent local storage** under overlay (configurable quota from MicrosoftGame.Config; derived from `<PersistentLocalStorage>` and `<TitleId>` in config, falls back to 2 GiB)
- **Import status reporting** — `compute_import_status()` / `report_import_status()` with per-call classification (HResult/Other) and call counts
- **PE module diagnostics** — `report_module_diagnostics()` reporting DLL characteristics, subsystem names, TLS callback RVAs, import binding counts
- **BCPack C++ decoder** — lane-array rANS decoder with block rebuilders for BC1, BC3, BC4, BC5, BC6H, BC7 (clean-room port of the Python reference)
- **Crash diagnostics** — `crash_capture_enabled()` (`ALMOST_CRASH` env var), memory watchpoints via `ALMOST_WATCHPOINTS` env var (comma-separated `address:size` pairs)
- **486+ unit/test files** covering shader translation, BVH, audio, BCpack, CPU ref, D3D12/XS, PE loader
- **Translator regression suite** (`python -I scripts/run_semantic_tests.py tests/semantic`)

## What's being worked on

These subsystems are under active development. Gaps are tracked per the [CONTRIBUTING.md](CONTRIBUTING.md) audit cycle.

- **Shader translation**: Some DXBC instructions have no translator rule; corpus coverage ~100% with some refused shaders. New rules are added hand-written `.asm` fixtures in `tests/semantic/fixtures/`, validated against the full 26,503 shader corpus.
- **Wave64 emulation**: Auto-detection via vulkaninfo; may pick wrong mode for a given GPU.
- **BVH builder**: Fixed 32 KiB shared memory cap; may exceed device limits on some hardware.
- **Persistent storage**: Configurable quota from MicrosoftGame.Config; may not suit all host volumes.
- **Root signature flag clearing**: Version 1.2 → 1.1 downgrade loses static sampler flag information when the host serializer only supports 1.1.
- **TLS management**: Complex per-thread slot installation; running threads before `load()` may corrupt mapped module TLS blocks.
- **GameInput shim**: Conditional on SDL3 availability; fallback to host GameInput.dll when shim not built.

## Known limitations (no exaggeration)

- **No games verified as running** — this is a research compatibility layer; success varies by title
- **Shader translation refusals** — some DXBC instructions produce no SPIR-V module; the port lacks those shaders
- **D3D12 root signature flag word** — Xbox high byte (0x03000000) not supported by Windows d3d12.dll 1.1 serializer
- **TLS corruption** — running threads before `load()` may corrupt mapped module TLS blocks
- **Filesystem inconsistency** — overlay/cache state may diverge after interrupted builds
- **XGameRuntime slot gaps** — some IXPackageImpl/IXNetworkingImpl2 slots return E_NOTIMPL; first boot shows which are needed
- **No online services** — no Xbox Live, Rockstar services, or DRM circumvention
- **No container decryption** — tools read plain files from user's game folder; no XVD/XVC decryption
- **Windows-only runtime build** — CMake `if(WIN32 AND ALMOST_BUILD_WINDOWS_RUNTIME)` guards many tests
- **Toolchain dependencies** — LLVM 21+ with AMDGPU target, .NET 8 SDK, Vulkan SDK, Visual Studio 2022 C++ tools

## What is prohibited (strictly enforced)

- **No game data ever committed** to the repository, commits, issues, pull requests, or test fixtures — archives, executables, shaders, textures, audio, or logs quoting them.
- **No decryption or protection workarounds** — if a format is encrypted, document it and stop; project tools read only plain files from the user's own disk.
- **No Xbox Live or Rockstar services** — no online authentication, no DRM circumvention, no unauthorized access to proprietary services.
- **No keys, decryption keys, or protection bypasses** — project reads plain files from the user's own legally obtained copy.
- **No Zettrox or Victory branding** — Project Almost uses `ALMOST_*` env var prefixes, `rt::shims`, `rt::pe`, and its own namespace naming. No third-party project names appear in source code, APIs, filenames, env vars, or documentation.

## Structure

- **About Project Almost** — what this project is, what it aims to achieve, what progress has been made, what still needs to happen
- **Current progress** — PortWatch milestone status (M0–M7), what each proves, what is achieved vs. in progress vs. not yet achieved
- **What works** — implemented and tested subsystems, distinguished from tested functionality vs. independently verified game compatibility
- **What's being worked on** — active development items with known gaps
- **Known limitations** — honest accounting of what is not yet working, with no exaggeration
- **Technical overview** — architecture, subsystems, how they fit together
- **Building and running** — prerequisites, configure, build, test instructions
- **Contributing** — how to claim work, submit increments, write tests, follow the audit cycle
- **Project rules** — prohibitions, branding, accuracy, no game data, AI-assisted disclosure

## Building and running (verified from repository)

```powershell
# 1. Clone and initialize submodules
git clone https://github.com/hoaxgit/almost.git
cd almost
git submodule update --init --recursive

# 2. Install prerequisites
#   - Python 3.12+ from python.org
#   - Visual Studio 2022 with "Desktop development with C++"
#   - Windows SDK (10.0.26100+ for root signature 1.2)
#   - .NET 8 SDK from dotnet.microsoft.com
#   - Vulkan SDK from vulkan.lunarg.com
#   - LLVM 21+ with AMDGPU target (scripts/build_llvm_mc.ps1 builds native distribution)

# 3. Configure and build runtime (Windows)
mkdir runtime/build
cd runtime/build
cmake .. -DALMOST_BUILD_WINDOWS_RUNTIME=ON
cmake --build . --config RelWithDebInfo

# 4. Run translator test suite (cross-platform, no game data)
python -I scripts/run_semantic_tests.py tests/semantic

# 5. Build full project (interactive)
python -I make_game.py
# Follow the TUI prompts: --jobs, stage overlap, --no-tui for plain output

# 6. Check your copy
python -I make_game.py --check
```

**Preflight checks** (`make_game.py --check`): validates game folder file names/sizes against `game/xbox-1.0.2.1.json`, checks for required tools (llvm-mc, spirv-as, spirv-val, dotnet SDK, Visual C++ tools).

## Contributing

1. **Read the coordination issue** — claim work before starting
2. **Present findings and WAIT for explicit approval** — audit findings → approval → implementation
3. **Work in small, reviewable increments** — one increment at a time, test before/after
4. **Write automated tests** — new functionality requires tests
5. **Preserve existing behavior** — Fix, don't disable; translator rules, not skip lists
6. **Count over the corpus** — 26,503 shaders; rules validated across entire corpus, not samples
7. **Classify subsystems** — §5: generic/reusable/Xbox-specific/Windows-specific/game-specific/experimental/partially implemented/deprecated
8. **Document architectural decisions** — all new docs identify Project/Studio/Inherited codebase
9. **No game data in commits** — `python -I scripts/check_no_game_data.py <path>` checks first bytes
10. **AI-assisted changes**: disclose in PR, test on GPU, report counts before/after

## Declaration for AI-assisted changes

When AI-assisted, PR descriptions must state:
- What was tested
- On which GPU and driver
- Translated and `spirv-val`-valid counts before and after

## Legal Notice

**Project Almost** is an independent software compatibility project. It is not affiliated with, endorsed by, sponsored by, or associated with Rockstar Games or Take-Two Interactive.

**Project Almost** does not distribute, include, host, or provide GTA VI game files, executables, assets, source code, or other copyrighted game content. Users must obtain any required game software and content through legitimate channels.

All game-related trademarks and copyrights belong to their respective owners. Project Almost is provided for compatibility research and software development purposes only.

---

*Badge accuracy verified from repository dependencies; no metrics claimed.*