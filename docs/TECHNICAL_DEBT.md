# TECHNICAL_DEBT.md
## Project Almost - Technical Debt

**Studio:** Almost Studios  
**Inherited codebase:** Project Leonida  

## HIGH-Priority Debt

### 1. Cross-Build Toolchain Complexity
- **Area:** `runtime/CMakeLists.txt` + `scripts/build_llvm_mc.ps1`
- **Issue:** clang-cl + xwin SDK cross-compile from Linux is fragile; requires specific Windows SDK version (10.0.26100+), xwin splat configuration, and VC/Tools/MSVC availability
- **Impact:** Builds fail silently on misconfigured PATH; new contributors cannot build Windows runtime on Linux
- **Evidence:** `cross_build_missing()` lists 8+ required components (cmake, clang-cl suffixes, xwin splat, Visual C++ tools)
- **Fix needed:** Automated environment validation, Docker-based build isolates

### 2. D3D12 → Vulkan Translation Gaps
- **Area:** `runtime/shims/d3d12_xs.cpp`, `runtime/src/vulkan_*`
- **Issue:** Xbox root signature flag word (high byte 0x03000000) not supported by Windows d3d12.dll 1.1 serializer; version 1.2 descriptions rewritten to 1.1 but static sampler flags cause E_INVALIDARG
- **Impact:** Some Xbox root signatures cannot be forwarded to host d3d12.dll; workarounds lose descriptor flag information
- **Evidence:** `d3d12_xs.cpp` has explicit logging for "version 1.2 with static sampler flags, and the host d3d12.dll serializes only up to 1.1"; `root_sig_census.py` and `root_spill_census.py` census tools measure descriptor layouts
- **Fix needed:** Dynamic root signature flag clearing, or Vulkan serializer that accepts Xbox flag word

### 3. Shader Translation Refusals
- **Area:** `tools/rdna2spirv.py`, translator rule set
- **Issue:** Some shaders refused by translator (measured: count over 26,503 corpus); refused shaders cause pipeline creation failure in BVH builder
- **Impact:** Port lacks shaders that were refused; `verify_translation_completeness()` reports gap between carved count and translated index
- **Evidence:** `shader_stage_manifest.py` / `shader_stage_resume.py` track per-stage progress; `test_rdna2spirv_*.py` fixtures test individual rules; some opcodes have no translator rule
- **Fix needed:** New translator rules via fixture addition in `tests/semantic/`, not skip lists

### 4. TLS Management for Manually Mapped Modules
- **Area:** `runtime/src/pe_loader.cpp` (g_thread_tls_slots, g_tls_hosts, clone management)
- **Issue:** Complex per-thread TLS slot installation; running threads before load(), clone cleanup, generation tracking
- **Impact:** Mapped module TLS corruption when threads exist before load(); clone sweep race conditions
- **Evidence:** `install_on_existing_threads()` handles threads that predated the mapped module; `sweep_stale_tls_clones()` cleans up at exit; `kMaxMappedTlsModules = 16`, `kMaxModuleTlsSlots = 64`
- **Fix needed:** Simplified TLS model, better thread quiescence guarantees

### 5. File System Overlay Correctness
- **Area:** `runtime/shims/file_system.cpp` (console path → overlay mapping)
- **Issue:** Marker-based deletion and copy-up staging has edge cases: half-copied files, marker collision, deleted folder visibility
- **Impact:** Title may see inconsistent filesystem state; deleted files may reappear; folder removal may leave markers
- **Evidence:** `write_target()` copy-up staging with `s_copies` counter; `delete_file_w()` marker logic; `remove_directory_w()` package folder hiding
- **Fix needed:** More robust overlay state machine, automated regression tests

### 6. Unimplemented Import Policy State Management
- **Area:** `runtime/shims/registry.cpp` (g_policy, g_seen, g_called mutex)
- **Issue:** Global unimplemented policy is process-wide and decided once on first use; no per-import override
- **Impact:** Cannot have some imports log and others stop; policy applies globally
- **Evidence:** `g_policy` atomic, initialized once via `unimplemented_policy()` which reads `LEONIDA_UNIMPLEMENTED` env var
- **Fix needed:** Per-import policy or better policy override mechanism

### 7. Vulkan GPU Info Probing Reliability
- **Area:** `make_game.py` (wave64_settings, runtime_gpu_info)
- **Issue:** `tools/vk_subgroup_info.py` subprocess may fail; vulkaninfo may not be available; timeout 120s
- **Impact:** Wave64 emulation mode may be wrong (on/off/auto), causing shader translation thrashing across runs
- **Evidence:** `runtime_gpu_info()` returns {} on error; `wave64_settings()` falls back to emulation on when size is None; `runtime_gpu_per_vertex()` may return None
- **Fix needed:** Cache probing results, improve error handling, add fallback GPU profiles

### 8. Persistent Local Storage Quota Enforcement
- **Area:** `runtime/shims/xgameruntime.cpp` (kPersistentQuotaBytes = 2 GiB)
- **Issue:** Fixed 2 GiB quota from MicrosoftGame.Config measurement; actual quota depends on host volume free space
- **Impact:** Persistent storage may fail on smaller drives; `pls_get_space_info()` caps by host `std::filesystem::space()`
- **Evidence:** `kPersistentQuotaBytes = 2048ull * 1024 * 1024` measured from package 1.0.2.1; `pls_get_space_info()` available_free_bytes capped by host availability
- **Fix needed:** Configurable quota, better quota exhaustion handling

### 9. XGameRuntime Class Completeness
- **Area:** `runtime/shims/xgameruntime.cpp` (measured thunks vs documented GDK classes)
- **Issue:** 161+ runtime thunks measured; some classes (XPersistentLocalStorageImpl3 MountForPackage, XGameImpl, XPackageImpl slots) return E_NOTIMPL; first boot shows which slots are needed
- **Impact:** Titles requiring unimplemented slots fail at boot; LEONIDA_UNIMPLEMENTED policy controls behavior
- **Evidence:** `query_served()` chains through xgameruntime_user.cpp, xgameruntime_gamesave.cpp, xgameruntime_store.cpp, xgameruntime_offline.cpp; `class_served()` validates Release on success
- **Fix needed:** Implement missing slots, or document which titles need which slots

### 10. GameInput Shim SDL3 Dependency
- **Area:** `runtime/shims/gameinput.cpp`, `runtime/CMakeLists.txt` (LEONIDA_SDL3 option)
- **Issue:** GameInput shim built only when SDL3 available; fetch or build from source; Windows GameInput.dll not used
- **Impact:** Controller support fails on SDL3-unavailable platforms; LEONIDA_SDL3=OFF builds without shim
- **Evidence:** CMake option LEONIDA_SDL3 (ON by default when SDL3 find_package succeeds); FetchContent download of SDL3 3.4.20
- **Fix needed:** Pre-built SDL3 static library, or shim that uses host GameInput.dll

## MEDIUM-Priority Debt

### 11. Root Signature Downgrade Logic
- **Area:** `runtime/shims/d3d12_xs.cpp` (serialize_root_signature, downgrade_root_signature_1_2_to_1_1)
- **Issue:** Version 1.2 → 1.1 downgrade loses static sampler flag information; when host serializes only up to 1.1 and description has static sampler flags, refuses with E_INVALIDARG
- **Impact:** Some Xbox root signatures cannot be forwarded; descriptor flags silently dropped
- **Evidence:** `d3d12_xs.cpp` lines 70-81: downgrade path with sampler list building; measured on real hardware

### 12. BVH Builder Shared Memory Cap
- **Area:** `make_game.py` (BVH_MAX_SHARED_MEMORY = 32768)
- **Issue:** Fixed 32 KiB shared memory cap for BVH builder programs; some devices may have different limits
- **Impact:** BVH builder may fail on devices with lower shared memory; the cap was measured on gfx1013

### 13. FSPX Translation Flag Persistence
- **Area:** `make_game.py` (fspx_settings, translator_flags.txt, wave64_emulate.txt)
- **Issue:** Translator forward-dependent flags (wave64 mode, fallbacks, sources digest) must match exactly for reuse
- **Impact:** Translator source change forces full re-translation of FSPX programs

### 14. SHARD Plan Incrementality
- **Area:** `make_game.py` translate stage shard management
- **Issue:** Shard plan (jobs, mode, fallbacks, translator digest) must match exactly for resume; partial runs leave orphan parts
- **Impact:** Stopped shader translation requires full re-translate if plan changes

### 15. Error Code Coverage in Shim Tests
- **Area:** `runtime/test/` (100+ test executables)
- **Issue:** Many tests require Windows D3D12 library or game executable; Linux/portable tests limited
- **Impact:** CI test coverage limited on non-Windows platforms
- **Evidence:** CMake `if(WIN32 AND LEONIDA_BUILD_WINDOWS_RUNTIME)` guards; many tests skip without d3d12.lib or DXGI

### 16. LEONIDA_DEVICE_TYPE Override Limitations
- **Area:** `runtime/shims/xgameruntime.cpp` (XSystemGetDeviceType)
- **Issue:** Environment variable LEONIDA_DEVICE_TYPE overrides device type but no validation of range beyond 0-8
- **Impact:** Can force invalid device types; no per-title device type configuration

### 17. Package Mount Path Resolution
- **Area:** `runtime/shims/xgameruntime.cpp` (XPackageMount resolve_package_path)
- **Issue:** Content packages mapped via LEONIDA_CONTENT_PACKAGES env var format "id1=path1;id2=path2"
- **Impact:** No installed content packages by default; mount returns E_NOT_FOUND

### 18. XAsync Provider Gaps
- **Area:** `runtime/shims/xgameruntime.cpp` (XAsyncBegin not provided, async completion)
- **Issue:** XAsyncBeginAlloc (slot 8) always returns E_NOTIMPL; XAsync completion relies on XAsyncRun with kNoNetwork for networking queries
- **Impact:** Titles using XAsyncBegin directly fail; workaround returns E_NOTIMPL

### 19. Persistent Storage Path Hardening
- **Area:** `runtime/shims/xgameruntime.cpp` (persistent_storage_dir)
- **Issue:** Persistent storage folder `<overlay>/.gta6rt-persistent` may not exist if overlay missing
- **Impact:** pls_get_path_size/get_space_info return ERROR_PATH_NOT_FOUND when no overlay

### 20. XSystemAnalyticsInfo Hardcoded Values
- **Area:** `runtime/shims/xgameruntime.cpp` (XSystemGetAnalyticsInfo)
- **Issue:** OS version 10.0.22000, hosting OS same, family "Xbox", form "Xbox Series X/S" hardcoded
- **Impact:** Cannot reflect actual host OS; form override via LEONIDA_ANALYTICS_FORM env var only

## LOW-Priority Debt

### 21. Command List Test SDL3 Dependency
- **Area:** `runtime/CMakeLists.txt` (gameinput_test, gameinput_probe)
- **Issue:** Tests require SDL3 gamepad; fails without installed SDL3 or fetch
- **Impact:** test_ gameinput_* skips on SDL3-unavailable platforms

### 22. XPath Console Parsing Edge Cases
- **Area:** `runtime/shims/file_system.cpp` (is_separator, console_drive, parse_console)
- **Issue:** Path parsing may not cover all title spelling variations (drive letters, separator styles)
- **Impact:** Some GAME: paths refused or mis-routed

### 23. TLS Clone Sweep Timing
- **Area:** `runtime/src/pe_loader.cpp` (sweep_stale_tls_clones_internal, atexit handler)
- **Issue:** Clone cleanup at process exit may not cover all exit paths (TerminateProcess, exception exits)
- **Impact:** Stale TLS helper DLLs left behind after abnormal termination

### 24. PE Loader Relocation Exhaustiveness
- **Area:** `runtime/src/pe_loader.cpp` (relocation processing)
- **Issue:** Limited relocation type support; may not handle all Xbox PE relocation entries
- **Impact:** Some executables fail to map at non-preferred base

### 25. Sparse Test Coverage for Edge Cases
- **Area:** `tests/` (486+ files, but not all categories covered equally)
- **Issue:** Some translator rules have no fixture; some shader ops untested
- **Impact:** New translator rules may have undiscovered bugs
- **Evidence:** `test_valu_untested_ops.py` tracks untested operations

### 26. Build Dependency Pinning
- **Area:** `global.json`, `tests/requirements.txt`, third_party submodules
- **Issue:** Toolchain versions (LLVM 21+, .NET 8, Vulkan SDK) may drift; submodule pins may break
- **Impact:** Build failures when tools update; `check_no_game_data.py` catches game data but not tool version mismatches
- **Fix needed:** CI matrix with pinned tool versions, Docker images with fixed toolchain

### 27. Documentation Drift
- **Area:** `docs/` (75+ files, Prompt.md Section 8 requirements)
- **Issue:** New architecture decisions may not be documented; existing docs may become outdated
- **Impact:** Team members cannot understand system from docs alone
- **Fix needed:** Documentation created alongside code changes, Prompt.md Section 8 compliance

### 28. No CI Configuration in Repo
- **Area:** Root level - no GitHub Actions, no CI scripts committed
- **Issue:** ProjectAlmost has no automated test integration in repo
- **Impact:** No pull request validation without external CI setup
- **Fix needed:** Add CI configuration for translator tests, runtime tests on Windows/Linux

### 29. Windows-Only Runtime Build
- **Area:** `runtime/CMakeLists.txt` (LEONIDA_BUILD_WINDOWS_RUNTIME guard)
- **Issue:** Runtime, shim tests, and D3D12 tests only built on Windows (WIN32)
- **Impact:** Linux contributors cannot build or test Windows runtime features
- **Fix needed:** Portable test fixtures, Linux-compatible shim test implementations

### 30. SDL3 GameInput Fetch URL Hardening
- **Area:** `runtime/CMakeLists.txt` (FetchContent SDL3 download)
- **Issue:** Hardcoded URL and SHA256 for SDL3 3.4.20 download; may break if GitHub changes
- **Impact:** GameInput shim build fails if fetch fails
- **Fix needed:** Mirror SDL3 archive, or use system SDL3 when available