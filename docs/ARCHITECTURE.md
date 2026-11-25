# ARCHITECTURE.md
## Project Almost - Xbox Series X|S to Windows x64 Compatibility Layer

**Studio:** Almost Studios  
**Inherited codebase:** Project Almost  
**Target platform:** Xbox Series X|S (Scarlett) to Windows x64  
**Primary research approach:** Static recompilation

## CORE

### Runtime Management
- `runtime/src/pe_loader.cpp` - PE/COFF image loader for manually mapped Xbox executables
- Handles module loading, TLS initialization, import binding, relocation protection, and unwind table registration
- `runtime/shims/registry.cpp` - Import thunk registry and unimplemented import policy
- Supports `ALMOST_UNIMPLEMENTED=log|fail|stop` environment variable policy
- `runtime/shims/file_system.cpp` - GAME:/ drive-to-overlay path mapping and console path shims
- Configurable via `configure(game_dir, overlay_dir)`

### Configuration
- Project-wide configuration through environment variables (`ALMOST_*`)
- Game directory and overlay directory setup via `rt::fs::configure()`
- Persistent local storage quota (2 GiB default per MicrosoftGame.Config)

### Logging & Diagnostics
- `runtime/src/startup_trace.h` - JSON-line trace events for boot sequence
- `scripts/run_gpu_tests.py` / `scripts/run_wine_tests.py` - GPU and Wine test runners
- Test output integrates with ALMOST TUI progress board

## EXECUTION

### Executable Loader
- Maps Xbox executables at arbitrary base addresses (GTA6RT_HOST_BASE = 0x10000000000)
- Per-module TLS slot management (max 16 mapped modules, 64 TLS slots each)
- TLS clone mechanism for per-process thread-local data
- Import resolution through shim registry with unimplemented import thunks
- Dynamic module loading with dependency ordering (init_order_)

### CPU Execution Environment
- Static recompilation infrastructure (make_game.py shaders stage)
- RDNA2 → SPIR-V translation via `tools/rdna2spirv.py`
- Translator fallback flags: `--no-float-controls`, `--bary-fallback`, `--texel-offset-fallback`, `--mesh-compute`, `--f16-interp-fallback`, `--pc-tail-ubo`
- Wave64 emulation control (`--wave64-emulate on/off/auto`)
- Translator source digest tracking (TRANSLATION_SOURCES constant list)

### Static Recompilation Engine
- `tools/rdna2spirv.py` - Main translator with shard-based parallelization
- Shader carving (`shader_carve.py`), FSSM unpacking (`fssm_unpack.py`), disassembly (`shader_disasm.py`)
- BVH builder programs (`XboxBvhBuilder.dll` → SPIR-V via `pe_shader_containers.py`)
- FSPX (FidelityFX) compute program translation (`fspx_unpack.py`, `rdna2spirv.py`)
- Translation metadata: `index.json` per shader category (dxbc, fspx, bvh)
- Verification: `verify_translation_completeness()` ensures all carved shaders have SPIR-V modules

### Intermediate Representation
- SPIR-V binary modules (.spv) with optional text assembly (.spvasm)
- Per-module translation sources digest (`translation_sources_digest()`)
- Translator flags (`translator_flags.txt`) and wave64 mode (`wave64_emulate.txt`)
- Shard plan (`shards.json`) for incremental translation resume

### Native Code Generation
- Not applicable: this project uses static recompilation to SPIR-V, not native x64 code generation
- Vulkan pipeline creation (`vk_pipeline_check`) for shader validation
- GPU pipeline audit and conformance testing

## MEMORY

### Guest Memory Model
- PE/COFF image mapping at high base addresses (0x10000000000)
- Relocation processing and section protection
- TLS directory parsing and per-thread block management
- Base reservation at 0x10000000000 to avoid conflict with default x64 EXE base

### Memory Mappings
- `pe_loader.cpp` maps images with `VirtualAlloc` at preferred or relocated base
- Section protection handling (execute/read/write combinations)
- Relocation table processing for non-preferred base addresses

### Allocation Management
- TLS block allocation via `VirtualAlloc(MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)`
- Per-thread TLS slot installation and removal
- Clone management for mapped TLS helper DLLs

### Synchronization
- SRWLOCK-protected TLS global state (`g_tls_lock`, `g_lookup_mutex`, `g_enum_mutex`)
- Thread event handling under Windows loader lock
- Clone sweep and stale TLS management at process cleanup

## GRAPHICS

### Graphics Abstraction
- Vulkan backend (`runtime/vulkan_*`) for shader pipeline creation
- D3D12 → Vulkan translation layer (primary research target)
- D3D12 XS shims (`runtime/shims/d3d12_xs.cpp`) forwarding Xbox imports to Windows D3D12

### Shader Translation
- RDNA2 DXBC → SPIR-V translation (26,503 shader corpus)
- Translator rule set in `tools/rdna2spirv.py` with per-instruction semantics
- GFX1013 disassembly target for shader carving
- wave64 emulation/auto based on GPU subgroup size detection
- Translator fallback flags for edge cases

### Resource Management
- BVH builder module translation (51 compute programs from XboxBvhBuilder.dll)
- Descriptor table and root signature forwarding
- SPIR-V validation with `spirv-val`

### Graphics Backends
- Vulkan: primary target for D3D12 translation
- D3D12: Windows native, with XS shim forwarding
- Cross-validation between D3D12 and Vulkan outputs

### GPU Synchronization
- Pipeline cache creation and validation (`pipelines` stage)
- Present compose and swap chain management
- Command list recording and submission

## COMPATIBILITY

### Platform API Interfaces
- **D3D12:** `D3D12CreateDevice`, `D3D12Serialize*`, `D3D12XboxCreateDevice`, `D3D12XboxCreateSyntheticDevice`
- **DXGI:** `CreateDXGIFactory*`, `DXGIGetDebugInterface1`, `DXGIDeclareAdapterRemovalSupport`
- **XGameRuntime:** `XGameRuntimeInitialize`, `QueryApiImpl`, class querying for Threading, Error, Networking, PersistentStorage, Package, System, Analytics
- **GameInput:** `GameInputCreate` via SDL3 shim (optional, ALMOST_SDL3)
- **XAudio2:** `XAudio2Create` (xaudio2_9.dll / xaudio2_9redist.dll)
- **DStorage:** `DStorageGetFactory` (dstorage.dll / dstorage_xs.dll)

### Filesystem
- `GAME:/` prefix mapping to game directory with overlay support
- Console path parsing: `GAME:\data\x.sps`, `T:\` scratch folder, `C:\` host paths
- Overlay directory writes (default: `<game-dir>\overlay`)
- Deletion markers (` .gta6rt-deleted/`), copy-up staging, folder removal markers
- FindFirstFile/FindNextFile merging of overlay + package listings

### Audio
- XMA2 audio decoder integration (BCPack-based)
- AWC → WAV/OGG conversion via `rpf9dump` and `awc_opus.py`
- Audio manifest and cue sheet processing

### Input
- GameInput shim on SDL3 (optional, ON by default)
- Controller mapping: DualSense trigger effects and lightbar
- Offline query implementation for all XInput-compatible functions

### Threading
- XTaskQueue/XAsync from vendored `third_party/xtaskqueue` (microsof/libHttpClient, MIT-licensed)
- Threading primitives: SRWLOCK, condition variables, atomic operations
- Per-thread TLS slot management

### Runtime Services
- XSystem device type, console ID, app device ID
- XPackage mount path resolution and mount handle management
- XPersistentLocalStorage: persistent folder under overlay (2 GiB quota)
- XErrorReport/XErrorSetCallback/XErrorSetOptions
- XPackageIsPackagedProcess detection

## INTEGRATION

### Game-Specific Configurations
- MicrosoftGame.Config version matching (1.0.2.1)
- ALMOST_DEVICE_TYPE override for XSystemGetDeviceType()
- ALMOST_PACKAGE_ID for XPackageGetCurrentProcessPackageIdentifier()
- ALMOST_CONTENT_PACKAGES for installed content mapping
- ALMOST_PACKAGED override for XPackageIsPackagedProcess()
- ALMOST_ANALYTICS_FORM for XSystemGetAnalyticsInfo() form override

### Compatibility Profiles
- Per-GPU wave64 emulation settings (auto detects max subgroup size via vulkaninfo)
- Translator fallback flag profiles (recorded in translator_flags.txt)
- Unimplemented policy profiles (log/fail/stop)

### Testing
- 486+ unit/test files in `tests/` covering:
  - Shader translation (semantic tests, SPIR-V interp)
  - BVH geometry and shader stages
  - Audio (AWC/opus/WAV)
  - BCpack encoding/decoding
  - CPU reference validation
  - D3D12/XS import forwarding
  - PE loader bounds and TLS
  - Root signature contract tests
- `python -I scripts/run_semantic_tests.py tests/semantic` - Translator regression suite
- `python -m pytest tests/` - Standard library unit tests

### Debugging
- `--trace-d3d12` flag for D3D12XboxCreateDevice parameter logging
- Startup trace events for file open failures and import calls
- `ALMOST_UNIMPLEMENTED` policy control
- GPU info probing (`tools/vk_subgroup_info.py`)

## Compatibility Design Notes

- Do NOT assume Xbox and Windows API implementations are interchangeable
- Do NOT assume CPU instruction compatibility guarantees executable compatibility
- Do NOT implement DRM circumvention, authentication bypasses, or unauthorized access
- Fix, don't disable: rejected shaders get translator rules, not skip lists
- Count over the corpus: rules validated across all 26,503 shaders
- No game data ever committed; all tools work on user's own copy