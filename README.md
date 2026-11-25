# Project Almost

A research compatibility layer that translates Xbox Series X|S (Scarlett) games to run on Windows x64. It is a static recompilation approach: Xbox game executables and DLLs are mapped and instrumented so they can operate through Windows APIs and a Vulkan backend.

## What this project does

- **Maps Xbox PE executables** on Windows — PE/COFF images loaded at high base addresses (0x10000000000+)
- **Forwards Xbox-specific imports** — D3D12 XS shims, XGameRuntime classes, DXGI, XAsync, XUser, XStore, XPackage, and more
- **Translates shaders** — RDNA2 DXBC → SPIR-V for graphics and compute pipelines
- **Provides overlay filesystem** — GAME:/, T:/, and C: drive mappings with marker-based deletion and copy-up staging
- **Runs a test suite** — 486+ unit tests covering shader translation, BVH, audio, BCpack, CPU reference, D3D12/XS, and PE loader

It does **not** distribute, provide, or host GTA VI game files, executables, assets, keys, or licenses. Users independently own a legitimate copy of GTA VI and prepare game data from their own Xbox Series installation.

## Project status

One row per subsystem. ✅ = implemented and covered by automated tests · ⚠️ = partially working with known gaps · ❌ = not yet achieved.

| What | Status |
|---|---|
| **PE loading** | ✅ Xbox PE/COFF images map at console base addresses (0x10000000000+) with import resolution, TLS and module diagnostics; loader and PE-loader tests run without game files. |
| **CPU execution** | ⚠️ Game code runs natively on x86-64 through the mapped executable and import thunks (no CPU emulator); execution of the retail game's first frames is not yet reached (M3). |
| **System shims** | ⚠️ Import forwarding, the overlay filesystem (GAME:/T:/C:) and offline XGameRuntime classes are implemented and tested; some IXPackageImpl/IXNetworkingImpl2 slots still return E_NOTIMPL until a first boot identifies them. |
| **Shader translation** | ✅ RDNA2 → SPIR-V translator passes the 447-file / 5,957-test regression suite over the 26,503-shader corpus with zero failures; some instructions still have no rule. |
| **Graphics backend** | ⚠️ Vulkan 1.3 device with bindless GFX10 descriptors, PresentX presentation and per-feature fallbacks; PortWatch boot gates M0–M2 are in progress, M3–M7 not yet achieved. GPU test results are recorded per GPU and driver. |
| **Audio** | ⚠️ XMA2 decoder (Rust crate, C ABI) and AWC Opus/WAV export are implemented with unit tests; Bink/XAudio playback is exercised by synthetic tests only — no game audio verified. |
| **Input** | ✅ GameInput v0 shim over SDL3: device enumeration, rumble, haptics, motion, adaptive triggers, keyboard-as-pad; covered by unit tests (requires the SDL3-built shim). |
| **Game boot / playability** | ❌ No game verified reaching a first frame, menu or gameplay. Passing the earlier gates means the layer translates a defined API and shader set for synthetic tests — it does not mean the game runs. |

Boot progress is also tracked against the [PortWatch](https://github.com/realdody/americas-recomp) milestone gates (M0–M7, defined in docs/d3d12_vulkan.md §6): M0–M2 in progress, M3–M7 not yet achieved.

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
- **Project status** — compact per-subsystem status table (PE loading, CPU execution, shims, shader translation, graphics, audio, input, boot/playability)
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