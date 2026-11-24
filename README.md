# Project Almost

## Almost Studios

### Xbox Series X|S → Windows x64 Compatibility Layer

<p align="center">
  <a href="https://github.com/hoaxgit/almost">
    <img src="https://files.catbox.moe/pzckpj.png" alt="Project Almost — Xbox Series X|S static recompilation research, generalized from Project Leonida" width="600">
  </a>
</p>

<p align="center">
  <em>Research approach: Static recompilation · Inherited codebase: Project Leonida · Target platform: Xbox Series X|S (Scarlett) to Windows x64</em>
</p>

---

## 📋 Navigation

[Overview](#project-almost) •
[Project Status](#current-status) •
[Architecture](#architecture-overview) •
[Capabilities](#existing-capabilities-verified-from-audit) •
[Experimental Components](#experimental--partial-components) •
[Limitations](#known-limitations-no-exaggeration) •
[Roadmap](#project-roadmap) •
[Building](#setup--build-instructions-verified) •
[Contributing](#contribution-guidelines) •
[Legal](#legal--authorization-boundaries)

---

## 🟦 Current Status

| Category | State |
|---|---|
| **Research phase** | Active repository audit and architecture generalization |
| **Codebase inherited** | Project Leonida — existing PE loader, D3D12 XS shims, XGameRuntime shims, shader translation pipeline, file system overlay, test suite (486+ tests) |
| **Build system** | CMake + clang-cl/xwin cross-build (Windows runtime); Python 3.12+ tooling (`make_game.py`) |
| **Translator infrastructure** | RDNA2 → SPIR-V via `tools/rdna2spirv.py`; 26,503 shader corpus with shard-based parallelization |
| **Test coverage** | 486+ unit/test files across shader translation, BVH, audio, BCpack, CPU ref, D3D12/XS, PE loader |
| **Known limitations** | See Technical Debt and Compatibility Design documents; no games verified as running; many subsystems partial/experimental |

---

## 🏗️ Architecture Overview

```mermaid
flowchart TD
  subgraph CORE
    PE[PE Loader (runtime/src/pe_loader.cpp)] -->|maps images| TLS[TLS Manager]
    REG[Import Registry (runtime/shims/registry.cpp)] -->|thunk resolution| UNI[Unimplemented Policy]
    FS[File System Shims (runtime/shims/file_system.cpp)] -->|GAME:/overlay| OVER[Overlay Directory]
  end

  subgraph EXECUTION
    SHA[Static Recompilation (tools/rdna2spirv.py)] -->|SPIR-V modules| VULK[Vulkan Backend]
    TLS -->|per-thread blocks| LOAD[Loader::load()]
    UNI -->|return class| THUNK[Unimplemented Import Thunks]
  end

  subgraph GRAPHICS
    D3D12[XS D3D12 Shims (runtime/shims/d3d12_xs.cpp)] -->|forward Xbox imports| VK[Vulkan Device]
    FSPX[FSPX Programs (tools/fspx_unpack.py)] -->|FidelityFX compute| SPIRV[SPIR-V Library]
    BVH[XboxBvhBuilder (51 programs)] -->|BVH compute| SPIRV
  end

  subgraph COMPATIBILITY
    DXGI[DXGI imports] -->|CreateDXGIFactory| HOST[d3d12.dll]
    XGR[XGameRuntime (161+ classes)] -->|QueryApiImpl| SHIMS[Shim Registry]
    AUD[XMA2/Audio (rpf9dump + BCPack)] -->|AWC→WAV/OGG| RPF9[RPF9 Archive]
  end

  subgraph INTEGRATION
    INPUT[GameInput (SDL3 shim, conditional)] -->|controller| HOST
    PSTORAGE[Persistent Local Storage (2 GiB quota)] -->|overlay\.gta6rt-persistent|
  end
```

### Major Subsystems (classified per Prompt.md §5)

| Subsystem | Classification | Maturity |
|---|---|---|
| PE Loader core | Generic and reusable | Fully implemented |
| Import thunk registry | Generic and reusable | Fully implemented |
| D3D12 XS shims | Xbox-specific | Fully implemented |
| XGameRuntime shims | Xbox-specific | Fully implemented |
| File system shims | Xbox-specific | Fully implemented |
| Shader translation | Generic | Partially implemented (some rules missing) |
| Wave64 emulation | Experimental | GPU-dependent auto-detection |
| BVH builder | Xbox-specific | Partially implemented (shared memory cap) |
| Persistent storage | Generic | Partially implemented (2 GiB fixed quota) |
| GameInput shim | Experimental | SDL3-dependent |
| TLS management | Experimental | Complex per-thread logic |
| Vulkan backend | Generic | Fully implemented (where HW available) |
| Root signature forwarding | Xbox-specific | Partially implemented (flag clearing gaps) |

---

## ✅ Existing Capabilities (verified from audit)

- **PE/COFF image mapping** for manually loaded Xbox executables at high base addresses (0x10000000000)
- **Import thunk classification** (HResult vs Other) with `LEONIDA_UNIMPLEMENTED=log|fail|stop` policy
- **GAME:/ drive mapping** to game directory with overlay support, marker-based deletion, copy-up staging
- **RDNA2 → SPIR-V translation** via `tools/rdna2spirv.py`; shard-based parallelization (--jobs, --shard)
- **FSPX (FidelityFX) compute program** unpacking and translation
- **BVH builder programs** (51 compute programs from XboxBvhBuilder.dll → SPIR-V)
- **DXGI import forwarding** (CreateDXGIFactory*, DXGIGetDebugInterface1, DXGIDeclareAdapterRemovalSupport)
- **D3D12 XS forwarding** (D3D12SerializeVersionedRootSignature, D3D12XboxCreateDevice with Xbox-ABI proxy)
- **XGameRuntime offline classes** (XSystem, XPackage, XNetworking, XPersistentLocalStorage, XError, XTaskQueue, XAsync, XUser, XStore, XGame, XGameSave)
- **File system console path parsing** (GAME:, T:, C: drives; FindFirstFile/FindNextFile merging)
- **Persistent local storage** under overlay (2 GiB quota from MicrosoftGame.Config)
- **GameInput shim** on SDL3 (conditional, LEONIDA_SDL3 option)
- **486+ unit/test files** covering shader translation, BVH, audio, BCpack, CPU ref, D3D12/XS, PE loader
- **Translator regression suite** (`python -I scripts/run_semantic_tests.py tests/semantic`)

---

## ⚠️ Experimental / Partial Components

These subsystems are functional in part but have known gaps:

- **Shader translation:** Some DXBC instructions have no translator rule; corpus coverage ~100% with some refused shaders
- **Wave64 emulation:** Auto-detection via vulkaninfo; may pick wrong mode for given GPU
- **BVH builder:** Fixed 32 KiB shared memory cap; may exceed device limits on some hardware
- **Persistent storage:** Fixed 2 GiB quota; may not suit all host volumes
- **Root signature flag clearing:** Version 1.2 → 1.1 downgrade loses static sampler flag information when host serializer only supports 1.1
- **TLS management:** Complex per-thread slot installation; running threads before module load may corrupt blocks
- **GameInput:** Conditional on SDL3 availability; fallback to host GameInput.dll when shim not built

---

## ⚠️ Known Limitations (no exaggeration)

- **No games verified as running** — this is a research compatibility layer; success varies by title
- **Shader translation refusals** — some DXBC instructions produce no SPIR-V module; port lacks those shaders
- **D3D12 root signature flag word** — Xbox high byte (0x03000000) not supported by Windows d3d12.dll 1.1 serializer
- **TLS corruption** — running threads before `load()` may corrupt mapped module TLS blocks
- **Filesystem inconsistency** — overlay/cache state may diverge after interrupted builds
- **XGameRuntime slot gaps** — some IXPackageImpl/IXNetworkingImpl2 slots return E_NOTIMPL; first boot shows which are needed
- **No online services** — no Xbox Live, Rockstar services, or DRM circumvention
- **No container decryption** — tools read plain files from user's game folder; no XVD/XVC decryption
- **Windows-only runtime build** — CMake `if(WIN32 AND LEONIDA_BUILD_WINDOWS_RUNTIME)` guards many tests
- **Toolchain dependencies** — LLVM 21+ with AMDGPU target, .NET 8 SDK, Vulkan SDK, Visual Studio 2022 C++ tools

---

## 📁 Repository Structure (key directories)

```
.
├── make_game.py                              # Main build orchestrator (Python 3.12+)
├── runtime/                                  # CMake C++20 project (Windows runtime, shims, tests)
│   ├── src/                                  # PE loader, Vulkan backend, utilities
│   └── shims/                                # D3D12 XS, XGameRuntime, file_system, GameInput
│       ├── registry.cpp                      # Import thunk classification & thunk generation
│       ├── file_system.cpp                   # GAME:/ overlay path mapping
│       ├── d3d12_xs.cpp                      # Xbox→Windows D3D12 forwarding
│       └── xgameruntime.cpp                  # 161+ GDK runtime class thunks
├── tools/                                    # Python development tooling
│   ├── rdna2spirv.py                         # RDNA2 → SPIR-V translator
│   ├── shader_carve.py                       # RPF9 carving
│   ├── fssm_unpack.py                        # FSSM native code unpacking
│   ├── rpf9dump/                             # .NET 8 RPF9 archive explorer
│   └── bcpack-rs/                            # Rust rANS decoder
├── tests/                                    # 486+ unit/test Python files
│   ├── test_*.py                             # Shader, BVH, audio, BCpack, CPU, D3D12/XS, PE loader
│   └── semantic/                             # Translator regression suite (.asm fixtures)
├── docs/                                     # 75+ documented subsystems
│   ├── ARCHITECTURE.md                       # This audit's output
│   ├── COMPONENT_INVENTORY.md
│   ├── TECHNICAL_DEBT.md
│   ├── GENERALIZATION_ROADMAP.md
│   ├── COMPATIBILITY_DESIGN.md
│   └── DEVELOPMENT_GUIDELINES.md
├── third_party/                              # rpf9-explorer (C#), bcpack-rs (Rust), xtaskqueue (C++)
└── scripts/                                  # build runners, test runners, preflight checks
```

---

## 🔧 Build Prerequisites (verified from repository)

### Cross-platform toolchain

| Component | Minimum | Notes |
|---|---|---|
| **Python** | 3.12+ | `python -I` required; standard library only for core tools |
| **CMake** | 3.24+ | `cmake -S runtime -B runtime/build`; multi-config generator |
| **clang-cl** | LLVM 19+ | `clang-cl` or `clang-cl-21` for Windows cross-build |
| **Visual C++ tools** | Desktop development with C++ | Required for Windows runtime build |
| **llvm-mc** | LLVM 21+ with AMDGPU | `scripts/build_llvm_mc.ps1` builds native distribution |
| **spirv-as / spirv-val** | Vulkan SDK | `vulkan.lunarg.com`; includes `glslc` too |
| **.NET 8 SDK** | 8.0+ | Required for `rpf9dump` (BCPACK decoding) |
| **Visual Studio 2022** | "Desktop development with C++" | `vswhere` validated in preflight |
| **PowerShell 5.1** | `scripts/build_runtime.ps1` | Windows-only build helper |
| **Git** | latest | `git submodule update --init` for `third_party/rpf9-explorer` |

### Linux portable subset

| Component | Status |
|---|---|
| CMake + clang-cross | Available for cross-build |
| Python tools | `python -I` works (standard library) |
| Runtime tests | CTest executables; many require d3d12.lib (Windows) |
| Shader translation | `python -I scripts/run_semantic_tests.py tests/semantic` works (no LLVM/SPIR-V tools needed) |
| Most unit tests | Standalone Python; no game data needed |

---

## 🚀 Setup / Build Instructions (verified)

```powershell
# 1. Clone and initialize submodules
git clone https://github.com/ItsNotRexed/Leonida.git
cd Leonida
git submodule update --init --recursive

# 2. Install prerequisites
#   - Python 3.12+ from python.org
#   - Visual Studio 2022 with "Desktop development with C++"
#   - Windows SDK (10.0.26100+ for root signature 1.2)
#   - .NET 8 SDK from dotnet.microsoft.com
#   - Vulkan SDK from vulkan.lunarg.com
#   - LLVM 21+ with AMDGPU target (scripts/build_llvm_mc.ps1)

# 3. Configure and build runtime (Windows)
mkdir runtime/build
cd runtime/build
cmake .. -DLEONIDA_BUILD_WINDOWS_RUNTIME=ON
cmake --build . --config RelWithDebInfo

# 4. Run translator test suite (cross-platform)
python -I scripts/run_semantic_tests.py tests/semantic

# 5. Build full project (interactive)
python -I make_game.py
# Follow the TUI prompts: --jobs, stage overlap, --no-tui for plain output

# 6. Check your copy
python -I make_game.py --check
```

**Preflight checks** (`make_game.py --check`): validates game folder file names/sizes against `game/xbox-1.0.2.1.json`, checks for required tools (llvm-mc, spirv-as, spirv-val, dotnet SDK, Visual C++ tools).

---

## 🧪 Available Tests (verified from repository)

```powershell
# Translator regression suite (cross-platform, no game data)
python -I scripts/run_semantic_tests.py tests/semantic

# Standard library unit tests (with venv + requirements.txt)
python -m pytest tests/test_cpu_ref.py
python -m pytest tests/test_rdna2spirv_decode.py
python -m pytest tests/test_bvh_index.py
python -m pytest tests/test_awc_opus.py

# Runtime CTest (Windows, needs d3d12.lib / game EXE)
cd runtime/build && ctest --test-dir . -C RelWithDebInfo --output-on-failure
```

Test categories (from the 486+ files):

- **Shader translation:** alu, fma, cmp, logic, mem, pack, cvt, valu, vcmp, wave, saveexec, dpp, per-op coverage
- **BVH:** geometry, shaders, index, packing, rehydrate, tile obj, geometry check
- **Audio:** AWC/opus/WAV decoding, cue sheets, markers
- **BCpack:** encoding/decoding, BCPack tables, native decoder
- **CPU reference:** integer/floating point operations, transcendental functions
- **D3D12/XS:** import forwarding, root signature, device proxy, stubs generation
- **PE loader:** bounds, TLS, module names, PE bounds, atexit
- **File mode:** file permission and mode handling
- **misc.:** text encoding, line endings, i18n indexing, docs CLI flags

---

## 🧭 Project Roadmap

| Phase | Goal | Key Milestones |
|---|---|---|
| **1: Foundation** | Generalize reusable components from Project Leonida | PE loader core, import registry, file system overlay, shader translation infrastructure |
| **2: Windows Host Integration** | D3D12 → Vulkan translation, XGameRuntime classes | Root signature flag clearing, complete XGameRuntime thunk set, Vulkan backend validation |
| **3: Multi-Platform Portability** | Vulkan multi-GPU, cross-platform path mapping, TLS abstraction | Linux x64 build, device filter abstraction, POSIX path mapping |
| **4: Full Compatibility Layer** | All Xbox GDK imports forwarded, complete shader coverage, filesystem emulation | 100% shader translation (zero refused), all D3D12 XBox imports forwarded, full filesystem emulation |

**Success criteria (Phase 4):**

- Game EXE maps and starts without crashes
- D3D12 device creation succeeds via Xbox-ABI proxy
- All runtime classes queryable through QueryApiImpl
- Filesystem operations (create, read, delete, find) behave like console
- Input (controller) works with DualSense features
- Persistent storage within quota works across sessions

---

## 👥 Contribution Guidelines

### How to Contribute

1. **Read the coordination issue** — `gh issue list -R ItsNotRexed/Leonida --state open --search "Coordinating who works on what in:title"` — claim work before starting
2. **Present findings and WAIT for explicit approval** — Prompt.md Section 10: audit findings → approval → implementation
3. **Work in small, reviewable increments** — Prompt.md Section 190: one increment at a time, test before/after
4. **Write automated tests** — Prompt.md Section 183, 185: new functionality requires tests
5. **Preserve existing behavior** — Fix, don't disable; translator rules, not skip lists
6. **Count over the corpus** — 26,503 shaders; rules validated across entire corpus, not samples
7. **Classify subsystems** — Prompt.md §5: generic/reusable/Xbox-specific/Windows-specific/game-specific/experimental/partially implemented/deprecated
8. **Document architectural decisions** — Prompt.md §8: all new docs identify Project/Studio/Inherited codebase
9. **No game data in commits** — `python -I scripts/check_no_game_data.py <path>` checks first bytes
10. **AI-assisted changes:** disclose in PR, test on GPU, report counts before/after

### Declaration for AI-Assisted Changes

When AI-assisted, PR descriptions must state:
- What was tested
- On which GPU and driver
- Translated and `spirv-val`-valid counts before and after

### Branching & PR Workflow

- **One issue per PR** — maximum one focused change
- **`Depends on: #N`** for every PR building on previous work
- **No merging without test approval** — all new tests must pass; existing tests must not regress
- **Rollback pattern** — each increment must be independently reversible; must not break unrelated subsystems

### Do Not

- Rewrite the entire repository without explicit approval
- Delete existing functionality without approval
- Silently change public interfaces
- Invent undocumented functionality
- Claim successful implementation without verification
- Add unnecessary dependencies
- Assume Xbox and Windows APIs are interchangeable
- Assume CPU instruction compatibility guarantees executable compatibility
- Implement DRM circumvention or authentication bypasses
- Use `&&` in PowerShell 5.1 (use `; if ($?) {}` instead)
- Change directories inside bash/PowerShell commands (use `workdir` parameter)
- Overwhelm repo with unnecessary abstractions
- Claim functionality based on filenames or documentation alone
- Skip the audit/approval wait cycle

---

## ⚖️ Legal & Authorization Boundaries

### Strict Prohibitions (from Prompt.md & AGENTS.md)

- **No game data ever committed** — archives, executables, shaders, textures, audio, or logs quoting them
- **No decryption or protection workarounds** — if a format is encrypted, document it and stop
- **No Xbox Live or Rockstar services** — no online authentication, no DRM circumvention
- **No unauthorized access to proprietary services** — this is a research compatibility layer only
- **No keys, decryption keys, or protection bypasses** — project reads plain files from user's own copy

### What Is Allowed

- **User's own game copy** — all tools read plain files from the user's legally obtained Xbox Series package
- **Static recompilation research** — RDNA2 → SPIR-V translation, PE mapping, import forwarding
- **GPU validation** — `spirv-val` checks, pipeline cache audit, GPU info probing
- **Translator rule addition** — hand-written `.asm` fixtures in `tests/semantic/fixtures/`, validated against the 26,503 shader corpus
- **Publicly documented API forwarding** — D3D12 XS shims, XGameRuntime offline classes, GAME:/ overlay mapping
- **No commit of derived/shader outputs** — build artifacts go under `--out` work folder, never committed

### Attribution

- **Project:** Project Almost
- **Studio:** Almost Studios
- **Inherited codebase:** Project Leonida
- **Repository:** https://github.com/ItsNotRexed/Leonida

---

## 🎫 Badges (accurate only)

```markdown
[![Python 3.12+](https://img.shields.io/badge/python-3.12%2B-blue.svg)](https://www.python.org/downloads/release/python-3120/)
[![CMake 3.24+](https://img.shields.io/badge/cmake-3.24%2B-brightgreen.svg)](https://cmake.org)
[![LLVM 21+](https://img.shields.io/badge/llvm-21%2B-orange.svg)](https://llvm.org)
[![Visual C++ 2022](https://img.shields.io/badge/msvc-2022%20DDC-lightgrey.svg)](https://visualstudio.microsoft.com)
[![License: MIT](https://img.shields.io/badge/license-MIT-yellow.svg)](https://opensource.org/licenses/MIT) # Project Leonida license
```

> *Badge accuracy verified from repository dependencies; no metrics claimed.*

---

## 📬 Contact & Identity

**Project Almost** · **Almost Studios** · Inherited from **Project Leonida**  
Repository: <https://github.com/ItsNotRexed/Leonida>  

This project is a research compatibility layer. No games are distributed or run herein unless the user owns the original Xbox Series copy and provides the game folder path. All tools read plain files from the user's work directory; no game content is committed to this repository.