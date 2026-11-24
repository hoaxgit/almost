# COMPONENT_INVENTORY.md
## Project Almost - Component Inventory

**Studio:** Almost Studios  
**Inherited codebase:** Project Leonida  

## 1. RUNTIME CORE (runtime/)

### pe_loader.cpp / pe_loader.h
- **Maturity:** Fully implemented
- **Lines:** ~1200+
- **PE/COFF image mapping** for manually loaded Xbox executables
- TLS management, import binding, relocation protection, unwind table registration
- Cross-platform: designed for clang-cl on Linux with xwin SDK, also Windows native
- **Reusable:** Core PE loading pattern, generic across API layers

### src/ (runtime/src/)
- **active_input.h** - Input state tracking
- **arena_size.h** - Memory arena size calculations
- **base_reservation.h** - Host base address reservation (0x10000000000)
- **binary_occlusion_spirv.h** - SPIR-V binary occlusion data
- **buffer_va_index.h** - Vulkan buffer VA index tracking
- **cached_pso.h** - Cached pipeline state objects
- **code_hooks.h** - Code hook insertion points
- **command_list.h** - Command list interface
- **conservative_raster.h** - Conservative rasterization
- **depth24_convert_spv.h / depth24_convert.comp** - Depth conversion shaders
- **depth24.h** - Depth24 format definition
- **descriptor_table.h** - Descriptor table management
- **frame_pacer.h** - Frame pacing timing
- **gfx10_descriptors.h** - GFX10 descriptor constants
- **gfx10_tiling.h** - GFX10 tiling format
- **indirect_dispatch_count_spv.h / indirect_dispatch_count.comp** - Indirect dispatch count
- **main.cpp** - Entry point, stage dispatch
- **mods.h** - Mod system integration
- **occlusion_sum_spirv.h / occlusion_sum.comp** - Occlusion sum compute
- **pe_loader.cpp** - See above
- **predicate_reduce_spv.h / predicate_reduce.comp** - Predicate reduction
- **predicated_copy_spv.h / predicated_copy.comp** - Predicated copy compute
- **present_compose_spv.h / present_compose.comp** - Present compose shader
- **present_compose.inc** - Present compose include
- **queue_families.h** - Queue family selection
- **queue_holds.h** - Queue hold tracking
- **resource_detile.h / resource_tiling.h** - Resource tiling/detiling
- **retire_tickets.h** - Retire ticket management
- **root_signature.cpp / root_signature.h** - Root signature management
- **shader_library.h** - Translated shader library lookup
- **spirv_f16_halves.h** - FP16 half handling
- **spirv_stores.h** - SPIR-V store operations
- **spirv_words.h** - SPIR-V word constants
- **statistics_sum_spirv.h / statistics_sum.comp** - Statistics sum compute
- **subgroup_size.h** - Subgroup size queries
- **tls_host.cpp** - TLS host helper DLL ( minimal, only TLS anchor)
- **uav_clear.h** - UAV clear compute
- **vulkan_backend.h** - Vulkan backend interface
- **vulkan_bindless.h** - Bindless descriptor extension
- **vulkan_depth24.h** - Vulkan depth24 format
- **vulkan_device_filter.h** - Vulkan device filter
- **vulkan_indirect_dispatch_count.h** - indirect dispatch count
- **vulkan_memory.h** - Vulkan memory management
- **vulkan_predicate.h** - Predicate support
- **vulkan_predicated_copy.h** - Predicated copy
- **vulkan_present.h** - Present integration
- **vulkan_present_compose.h** - Present compose
- **vulkan_replay.h** - Replay support
- **vulkan_scratch.h** - Scratch memory management
- **vulkan13_features.h** - Vulkan 1.3 features
- **xdxr_build_spv.h / xdxr_build.comp / xdxr_build.h / xdxr_build.inc / xdxr_descs.h** - XDXR build shaders

**Maturity:** Fully implemented and tested (CTest suite with 100+ test executables)  
**Reusable:** Vulkan backend patterns, shader library lookup, root signature model  
**Windows-specific:** D3D12 types, Windows API calls  
**Xbox-specific:** Some D3D12 XS forwarding logic

### cmake/
- CMakeLists.txt - Main build configuration (300+ lines)
- clang-cl-xwin.cmake - Cross-compile toolchain configuration
- **Maturity:** Fully implemented
**Reusable:** CMake project templates for Windows/Linux cross-builds

### cmake/clang-cl-xwin.cmake
- **Maturity:** Fully implemented
- Cross-compile toolchain for clang-cl targeting Windows from Linux
- **Xbox-specific:** xwin SDK integration

## 2. SHIMS (runtime/shims/)

### registry.cpp / registry.h
- **Maturity:** Fully implemented
- Import thunk classification (HResult vs Other)
- Unimplemented import policy (log/fail/stop via LEONIDA_UNIMPLEMENTED)
- Common stub generation for unimplemented imports (32-byte x86-64 thunks)
- Module lookup and bundled module loading
- **Reusable:** Import thunk pattern, policy engine
**Xbox-specific:** D3D12 XBox API classification, XGameRuntime thunks

### file_system.cpp / file_system.h
- **Maturity:** Fully implemented
- GAME:/ drive mapping to game directory with overlay
- Console path parsing (GAME:, T:, C: drives)
- FindFirstFile/FindNextFile merging of overlay + package
- CreateFile*, DeleteFile*, CreateDirectory*, RemoveDirectory*, MoveFile*, CopyFile*
- Marker-based deletion and copy-up staging
- **Reusable:** Console path-to-overlay mapping pattern
**Xbox-specific:** GDK console filesystem semantics

### d3d12_xs.cpp / d3d12_xs.h
- **Maturity:** Fully implemented
- Xbox D3D12 imports forwarded to Windows D3D12 exports
- Root signature flag clearing (Xbox high byte deserialization)
- D3D12XboxCreateDevice wrapper with Xbox-ABI proxy (slot 62 → CreateComputePipelineState, slot 76 → CreateCommittedResource)
- D3D12SerializeVersionedRootSignature with Xbox flag clearing
- D3D12CreateRootSignatureDeserializer with RTS0 blob flag clearing
- **Xbox-specific:** Xbox ABI → Windows ABI translation
**Windows-specific:** d3d12.dll API calls, root signature serialization

### d3d12_xs_device.cpp / d3d12_xs_device.h
- **Maturity:** Fully implemented
- Xbox-ABI proxy over PC D3D12 device
- Overrides selected slots (Compute Pipeline State, Committed Resource creation)
- Rest of vtable follows PC device
**Xbox-specific:** Xbox ABI proxy mechanism

### d3d12_xs_rootsig.cpp / d3d12_xs_rootsig.h
- **Maturity:** Fully implemented
- Root signature descriptor clearing (Xbox flags)
- Version 1.2 → 1.1 downgrade when host serializer only supports 1.1
- **Xbox-specific:** Xbox root signature flag word

### xgameruntime.cpp / xgameruntime.h
- **Maturity:** Fully implemented
- XGameRuntimeInitialize/XGameRuntimeUninitialize reference counting
- XSystemImpl: device type, console ID, app device ID, analytics info
- XPackageImpl: package identifier, mount path, mount management
- XPackageIsPackagedProcess detection
- XNetworkingImpl: networking queries (offline: always no connectivity)
- XPersistentLocalStorageImpl: 2 GiB persistent storage under overlay
- XErrorImpl: error report/callback/options
- XTaskQueue impl: create, dispatch, submit, register/unregister monitor
- XAsync: begin/schedule/complete/get status/result
- XUserImpl: user queries (offline answers)
- XStoreImpl: store queries (offline answers)
- XGameImpl: game queries (offline answers)
- XGameSaveImpl: gamesave queries (offline answers)
- XSystemAnalyticsImpl: analytics (offline placeholder values)
- **Xbox-specific:** All 161+ GDK runtime thunks
**Windows-specific:** Windows API calls where GDK is Xbox-only

### xgameruntime_user.cpp
- XUser user query implementation (offline)
- XGame user query implementation (offline)
- XStore user query implementation (offline)
- XGameSave user query implementation (offline)

### xgameruntime_gamesave.cpp
- XGameSave gamesave-specific implementations

### xgameruntime_store.cpp
- XStore store-specific implementations

### xgameruntime_offline.cpp
- Offline-specific runtime implementations

### registry.cpp (shims/ - see runtime/shims/registry.cpp above, also referenced here)
- Included by multiple shim files for module lookup and thunk generation

### gameinput.cpp / gameinput.h
- **Maturity:** Conditional (LEONIDA_SDL3 option)
- GameInput shim on SDL3
- DualSense trigger effects and lightbar support
- Joystick, HIDAPI, haptic, sensor and power code
- **Build option:** LEONIDA_SDL3 (ON by default when SDL3 available)
**Cross-platform:** SDL3-based, works on Windows/Linux/macOS

### process.cpp / process_shim.h
- **Maturity:** Fully implemented
- Process command line: GetCommandLineW must parse back to __wargv
- **Reusable:** Command line parsing shim pattern

### xtaskqueue/
- **Maturity:** Fully implemented (vendored third_party)
- XTaskQueue/XAsync from microsoft/libHttpClient (MIT-licensed)
- Task queue, thread pool, async primitives
- **Reusable:** Task queue pattern for offline GDK runtime services

### shims.h
- **Maturity:** Fully implemented
- Common shim interfaces and function table definitions
- BundledModuleOps structure for runtime module loading
- ReturnClass enum (HResult, Other)
- UnimplementedPolicy enum (Log, Fail, Stop)
- **Reusable:** Shim base interfaces

### startup_trace.h
- **Maturity:** Fully implemented
- JSON-line trace event emission
- Enabled via trace system, bounded to 256 file open failures
- **Reusable:** Tracing infrastructure

## 3. TOOLS (tools/)

### rdna2spirv.py
- **Maturity:** Fully implemented
- Main RDNA2 → SPIR-V translator
- Shard-based parallel translation (--jobs, --shard)
- --wave64-emulate on/off/auto mode
- Fallback flags: --no-float-controls, --bary-fallback, --texel-offset-fallback, --mesh-compute, --f16-interp-fallback, --pc-tail-ubo
- --no-asm flag: keep only .spv modules
- --merge: merge shard outputs into index.json
- Translator source digest tracking (TRANSLATION_SOURCES)
- **Corpus:** 26,503 shaders
- **Reusable:** RDNA2 translation engine, shard management

### shader_carve.py
- Shaders.rpf carving into DXBC containers and index.json
- **Reusable:** RPF9 archive carving

### fssm_unpack.py
- FSSM native code unpacking
- **Reusable:** FSSM unpacking

### shader_disasm.py
- DXBC disassembly with llvm-mc (gfx1013)
- Listing tracking and verification
- **Reusable:** Shader disassembly

### fspx_unpack.py
- amd_fidelityfx_upscaler_xbox.dll compute program unpacking
- **Reusable:** FSPX unpacking

### rpf9dump/
- **Maturity:** Fully implemented (vendored, .NET 8 needed)
- RPF9 archive exploration and extraction
- Model (OBJ), texture (DDS), audio (AWC→OGG/WAV) export
- BCPack decoding tables
- **Dependencies:** .NET 8 SDK, git submodule rpf9-explorer
**Xbox-specific:** RPF9 archive format (Xbox Series)

### bcpack-rs/
- **Maturity:** Fully implemented (vendored, Rust)
- rANS decoder for BCPack compressed assets
- Tables and decoding logic
- **Reusable:** BCPack decoding

### check_no_game_data.py
- **Maturity:** Fully implemented
- Checks file first bytes to detect game data
- **Hard rule:** No game data in commits

### dev.py
- Development helper scripts

### first_look.ps1
- Full report set for the executable

### merge_check.py
- Merge check utilities

### run_semantic_tests.py
- **Maturity:** Fully implemented
- Translator regression suite runner
- One fixture per core, no LLVM/SPIR-V tools needed
- **Usage:** `python -I scripts/run_semantic_tests.py tests/semantic`

### run_gpu_tests.py / run_wine_tests.py
- GPU and Wine test runners

### check_apt_key.py
- APT key check utility

## 4. TESTS (tests/)

### test_*.py (486+ files)
- **Maturity:** Mix of fully implemented, partially implemented, experimental
- Categories:
  - **Shader translation:** 457+ semantic test files (alu, fma, cmp, logic, mem, pack, cvt, valu, vcmp, wave, saveexec, dpp, etc.)
  - **BVH:** test_bvh_*.py (geometry, shaders, index, packing, rehydrate, etc.)
  - **Audio:** test_awc_*.py, test_bink_*.py, test_audio_*.py
  - **BCpack:** test_bcpack_*.py, test_bc_feature_*.py
  - **CPU ref:** test_cpu_ref.py, test_cpu_ref.py
  - **PE loader:** test_pe_*.py, test_loader_*.py, test_tls_*.py
  - **D3D12/XS:** test_d3d12_*.py, test_root_sig_*.py, test_xs_*.py
  - **Translators:** test_rdna2spirv_*.py, test_fspx_*.py
  - **Graphics:** test_gfx10_*.py, test_descriptor_*.py
  - **File mode:** test_file_mode.py
  - **misc:** test_text_write_line_endings.py, test_i18n_index.py, etc.
- **Test types:** Python stdlib (no game data needed), some require LLVM/SPIR-V tools
- **Reusable:** Individual test fixtures and rules are standalone

### semantic/ (translator regression suite)
- **Maturity:** Fully implemented
- Hand-written .asm fixtures for each translator rule
- test_*.py per rule category (algebra, control flow, saveexec, DPP, etc.)
- support.py, regenerate_fixtures.py
- **Usage:** `python -m unittest discover -s tests/semantic`
- **Corpus:** Algebra, control flow, saveexec, DPP, bitwise, logical, comparison, transcendental, FP16, FP32, FP64, pack, unpack, wave64, wave32, saveexec b32, carve, FSPX, BVH

## 5. SCRIPTS (scripts/)

### make_game.py
- **Maturity:** Fully implemented (291-line Prompt.md, main build orchestrator)
- Python 3.12+ required, runs with `python -I` (standard library only)
- Stages: check, shaders, assets, runtime, pipelines
- **Reusable:** Build orchestration pattern, preflight checking

### build_runtime.ps1 / build_llvm_mc.ps1 / carve_all.ps1
- PowerShell build helpers

### run_gpu_tests.py / run_wine_tests.py
- GPU and Wine test runners

### check_no_game_data.py
- **Hard rule:** No game data in commits

### dev.py
- Development helpers

### first_look.ps1
- Full report set for the executable

### merge_check.py
- Merge check utilities

## 6. THIRD_PARTY (third_party/)

### rpf9-explorer/
- **Maturity:** Initialized submodule (needs git submodule update --init)
- C# tool for RPF9 archive exploration
- Core library, Tools (IndexBuilder, BinkPlayer), GUI app
- **Dependency:** git submodule, required for assets stage

### bcpack-rs/
- **Maturity:** Vendored Rust rANS decoder
- BCPack calibration tools
- src/tables.rs, src/rans.rs, src/lib.rs

### xtaskqueue/
- **Maturity:** Vendored C++ task queue library
- Microsoft's MIT-licensed implementation from libHttpClient
- Task, AsyncLib, ThreadPool_win32, WaitTimer_win32

### xma/
- **Maturity:** LGPLv2.1 audio format (NOTICE.md + COPYING.LGPLv2.1)
- LGPL licensed audio format handler

## 7. VIEWER (viewer/)

### index.html / serve.py
- Map viewer local server

### studio.html
- Studio viewer

## 8. GAME (game/)

### xbox-1.0.2.1.json
- **Maturity:** Fully implemented
- Per-file size and SHA-256 manifest of Xbox package
- 439 lines, 200+ files with hashes
- **No game content:** hashes only

## 9. ASSETS (assets/)

### logo.png
- Project logo

## 10. MODS (mods/)

### first_person/mod.json / mandy_prison_intro/mod.json
- Mod definition files, loaded in name order by gta6rt

## 11. ROOT FILES

### AGENTS.md
- Agent coordination and claiming rules

### CONTRIBUTING.md
- Contribution guidelines (hard rules: no game data, no decryption, fix don't disable)

### global.json
- .NET global.json for tooling version setup

### Build.bat
- Windows batch build entry point

### LICENSE
- Project license