# COMPATIBILITY_DESIGN.md
## Project Almost - Compatibility Design

**Studio:** Almost Studios  
**Inherited codebase:** Project Leonida  
**Target platform:** Xbox Series X|S (Scarlett) to Windows x64  

## Fundamental Design Principles

### 1. Do NOT Assume API Interchangeability
- **Rule:** Do not assume that Xbox and Windows implementations of the same API are interchangeable.
- **Evidence:** Measured differences in D3D12 root signature flag word (Xbox sets high byte 0x03000000, Windows serializer rejects it)
- **Implementation:** D3D12 XS shims (`runtime/shims/d3d12_xs.cpp`) explicitly clear Xbox flag bits before forwarding; some descriptions rewritten as 1.1 when host only supports 1.1
- **XGameRuntime:** Each import classified via `classify()` in `registry.cpp`; return class (HResult/Other) determines thunk behavior
- **XAudio2:** Console name `xaudio2_9.dll` vs PC name; measured export shapes differ

### 2. Do NOT Assume CPU Instruction Compatibility Guarantees Executable Compatibility
- **Rule:** CPU instruction set compatibility does not guarantee executable compatibility.
- **Evidence:** PE/COFF format differences (Xbox uses custom relocation types, different TLS layout); Xbox executables mapped at base 0x10000000000 vs Windows default 0x1400000000; TLS index assignment differs (OS loader vs manual mapping)
- **Implementation:** `pe_loader.cpp` handles relocation processing, TLS directory parsing, per-module TLS slot management, import binding through shim registry
- **Base Reservation:** `GTA6RT_HOST_BASE /BASE:0x10000000000` keeps loader's own image off default x64 EXE base
- **Relocation Processing:** `require_range()` validates RVAs stay within image; not all Xbox relocation types may be handled

### 3. Do NOT Implement DRM Circumvention, Authentication Bypasses, or Unauthorized Access
- **Rule:** No DRM circumvention, authentication bypasses, or unauthorized access to proprietary services.
- **Evidence:** Project reads plain files from user's game folder; no XVD/XVC container decryption; no Rockstar Services access
- **Hard Rule:** `scripts/check_no_game_data.py` reads first bytes to detect game data in commits
- **No Keys:** Build never decrypts or works around encryption; only reads plain files on disk
- **Archives:** RPF9 archives read via `rpf9-explorer` submodule (C# tool), not reverse-engineered decryption

### 4. Fix, Don't Disable
- **Rule:** A shader the translator rejects gets a rule, not a skip list.
- **Evidence:** `verify_translation_completeness()` fails if index count < carved count; translator fixturing in `tests/semantic/` adds rules
- **Count over corpus:** 26,503 shaders - rules validated across entire corpus, not samples
- **FSPX programs:** `fspx_key_clash()` refuses index whose modules differ for same kind-6/kind-0 bytes
- **BVH builder:** One failed program gives up whole constructor; translator rules required

### 5. Count Over the Corpus
- **Rule:** A rule that holds for 400 shaders can fail on the other 26,000+.
- **Evidence:** `shader_stage_manifest.py` tracks counts; `verify_translation_completeness()` compares carved vs translated
- **Shader categories:** dxbc (main shaders), fspx (FidelityFX), bvh (XboxBvhBuilder compute programs)
- **Translator rules:** Each `.asm` fixture in `tests/semantic/` tests one rule; `regenerate_fixtures.py` regenerates from measured behavior

### 6. No Silent Interface Changes
- **Rule:** Do not silently change public interfaces.
- **Evidence:** `shims.h` defines stable ABI (BundledModuleOps, ReturnClass, UnimplementedPolicy); function signatures in shim .cpp files match measured guest ABI
- **Registry table:** `table()` map uses "dll!name" keys; `classify()` must match import strings exactly
- **Policy changes:** `LEONIDA_UNIMPLEMENTED` env var change is explicit; `g_policy.compare_exchange_strong()` is atomic write

### 7. Evidence Over Speculation
- **Rule:** Prefer evidence over speculation; distinguish officially documented behavior from verified implementation behavior from research hypotheses from unknowns.
- **Structure:** Each compatibility concern documented with:
  - Officially documented behavior (Microsoft GDK docs, learn.microsoft.com)
  - Verified implementation behavior (measured in this repo, disassembled code)
  - Research hypotheses (unproven, marked as such)
  - Unknowns requiring further investigation (marked as debt)

### 8. Platform API Compatibility Map

#### D3D12 (Xbox → Windows)
| Xbox Import | Windows Forwarding | Status |
|---|---|---|
| D3D12SerializeVersionedRootSignature | Forwarded with Xbox flag clearing | Fully implemented |
| D3D12SerializeRootSignature (legacy) | Forwarded with Xbox flag clearing | Fully implemented |
| D3D12CreateRootSignatureDeserializer | Forwarded with RTS0 blob flag clearing | Fully implemented |
| D3D12XboxCreateDevice | Wrapper with Xbox-ABI proxy (slot 62/76) | Fully implemented |
| D3D12ConfigureVirtualMemoryX | No-op S_OK | Fully implemented |
| D3D12CreateDevice | Host D3D12CreateDevice (feature level 12.0) | Fully implemented |

#### DXGI (Xbox → Windows)
| Xbox Import | Windows Forwarding | Status |
|---|---|---|
| CreateDXGIFactory | Host | Fully implemented |
| CreateDXGIFactory1 | Host | Fully implemented |
| CreateDXGIFactory2 | Host | Fully implemented |
| DXGIGetDebugInterface1 | Host | Fully implemented |
| DXGIDeclareAdapterRemovalSupport | Host | Fully implemented |

#### XGameRuntime (Xbox → Offline Shim)
| Class | Functions | Status |
|---|---|---|
| XGameRuntimeInitialize/Uninitialize | Ref counting, offline init | Fully implemented |
| XSystemImpl | Device type, console ID, analytics | Fully implemented |
| XPackageImpl | Package ID, mount path, mount management | Fully implemented |
| XNetworkingImpl | Network queries (always offline: no connectivity) | Fully implemented |
| XPersistentLocalStorageImpl | 2 GiB persistent storage | Fully implemented |
| XErrorImpl | Report, callback, options | Fully implemented |
| XTaskQueueImpl | Create, dispatch, submit, monitor | Fully implemented |
| XAsync | Begin/schedule/complete/get status/result | Partially (BeginAlloc E_NOTIMPL) |
| XUserImpl | User queries (offline placeholders) | Fully implemented |
| XStoreImpl | Store queries (offline placeholders) | Fully implemented |
| XGameImpl | Game queries (offline placeholders) | Fully implemented |
| XGameSaveImpl | Gamesave queries (offline placeholders) | Fully implemented |

#### GameInput (Xbox → Windows/SDL3)
| Function | Implementation | Status |
|---|---|---|
| GameInputCreate | SDL3 shim or host thunk | Conditional (LEONIDA_SDL3) |
| Controller mappings | DualSense trigger effects, lightbar | SDL3-dependent |

#### Audio (XMA2 → WAV/OGG)
| Format | Conversion | Status |
|---|---|---|
| XMA2 (AWC) | rpf9dump BCPack decoding → WAV via ffmpeg or OGG via opus | Fully implemented |

#### Filesystem (GAME:/ → Overlay)
| Console Path | Windows Equivalent | Status |
|---|---|---|
| GAME:\path | <game-dir>\overlay\path (with marker logic) | Fully implemented |
| T:\path | <game-dir>\.gta6rt-temp\path (scratch) | Fully implemented |
| C:\path | Host path (unchanged) | Fully implemented |
| GAME: itself | Game directory root | Fully implemented |

#### Input/Controller
| Feature | Implementation | Status |
|---|---|---|
| Button/axis mapping | Standard gamepad | Fully implemented |
| DualSense trigger effects | via SDL3 haptic | Conditional |
| DualSense lightbar | via SDL3 | Conditional |
| Motion/Sensor | Not implemented | Future work |

### 9. Compatibility Design Notes (from Prompt.md Section 226-227)
- Do not assume Xbox and Windows implementations of the same API are interchangeable
- Do not assume CPU instruction compatibility guarantees executable compatibility
- Do not implement DRM circumvention, authentication bypasses, or unauthorized access to proprietary services

### 10. Implementation Maturity Classification
All subsystems classified per Prompt.md Section 5:

| Subsystem | Classification |
|---|---|
| PE Loader core | Generic and reusable, fully implemented |
| Import thunk registry | Generic and reusable, fully implemented |
| File system shims | Xbox-specific, fully implemented |
| D3D12 XS shims | Xbox-specific, fully implemented |
| XGameRuntime shims | Xbox-specific, fully implemented |
| Shader translation | Generic, partially implemented (some rules missing) |
| Wave64 emulation | Experimental (GPU-dependent auto-detection) |
| BVH builder | Xbox-specific, partially implemented (shared memory cap) |
| Persistent storage | Generic, partially implemented (2 GiB fixed quota) |
| GameInput shim | Experimental (SDL3-dependent) |
| TLS management | Experimental (complex per-thread logic) |
| Vulkan backend | Generic, fully implemented (where hardware available) |
| Root signature forwarding | Xbox-specific, partially implemented (flag clearing gaps) |

## Compatibility Risk Assessment

### High Risk Areas
1. **D3D12 Root Signature Flags** - Xbox flag word may not survive deserialization on Windows 10 d3d12.dll
2. **Shader Translation Refusals** - Some DXBC instructions have no translator rule; port lacks those shaders
3. **TLS Corruption** - Running threads before module load may corrupt TLS blocks
4. **Filesystem Inconsistency** - Overlay/cache state may diverge from expected after interrupted builds

### Medium Risk Areas
1. **XGameRuntime Slot Gaps** - Some IXPackageImpl/IXNetworkingImpl2 slots return E_NOTIMPL
2. **BVH Builder Shared Memory** - Fixed 32 KiB cap may exceed device limits
3. **Wave64 Emulation Mode** - Auto-detection may pick wrong mode for given GPU
4. **Persistent Storage Quota** - Fixed 2 GiB may not suit all host volumes

### Low Risk Areas
1. **Import Thunk Policy** - log/fail/stop policy is well-defined and tested
2. **DXGI Forwarding** - All CreateDXGIFactory variants forward transparently
3. **D3D12CreateDevice** - Feature level 12.0 floor is sufficiently liberal
4. **Basic GAME: Path Parsing** - Most common path patterns handled correctly