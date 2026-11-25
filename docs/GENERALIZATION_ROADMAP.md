# GENERALIZATION_ROADMAP.md
## Project Almost - Generalization Roadmap

**Studio:** Almost Studios  
**Inherited codebase:** Project Almost  

## Phase 1: Foundation (Current State)

### Completed Generalization
- PE loader core rewritten for generic module mapping (no Xbox-specific assumptions)
- Import thunk registry pattern generalized (HResult/Other classification, policy engine)
- File system overlay pattern generalized (GAME:/ drive → overlay mapping, marker-based deletion)
- Shader translation infrastructure generalized (shard management, source digest tracking, wave64 emulation)
- Cross-build CMake configuration generalized (clang-cl/xwin/conditional features)

### Generalized Components (Reusable Beyond Xbox→Windows)
- `runtime/src/pe_loader.cpp` - Generic PE mapper for manually mapped images
- `runtime/shims/registry.cpp` - Import thunk classification and thunk generation pattern
- `runtime/shims/file_system.cpp` - Console path → overlay mapping (adaptable to other console APIs)
- `tools/rdna2spirv.py` - RDNA2→SPIR-V translator with shard parallelization
- `tests/semantic/` - Translator rule fixtures with standalone test harness

### Remaining Phase 1 Work
- [ ] Document all generic interfaces with Doxygen-style comments
- [ ] Extract platform-agnostic core into separate library targets
- [ ] Add C++ unit tests for PE loader independent of D3D12

## Phase 2: Windows Host Integration

### Targeted Generalization
- D3D12 XS shim forwarding (Xbox → Windows D3D12) → generalize as API contract translation layer
- XGameRuntime offline classes → generalize as GDK-to-Windows thunk framework
- GameInput SDL3 shim → generalize as controller ABI translation layer

### Phase 2 Milestones
- [ ] Abstract D3D12 XS shim into generic "import forwarding" interface
- [ ] Extract XGameRuntime class pattern into reusable shim base class
- [ ] Create SDL3-independent GameInput shim build option
- [ ] Add Linux-compatible test fixtures for PE loader and registry

### Phase 2 Dependencies
- Vulkan SDK availability for GPU info probing
- SDL3 static library for GameInput shim
- CMake cross-compile toolchain validation

## Phase 3: Multi-Platform Portability

### Targeted Generalization
- Vulkan backend → support multiple GPU vendors (AMD, Intel, NVIDIA)
- File system overlay → adapt to different console/PC filesystem conventions
- TLS management → platform-independent thread-local storage abstraction

### Phase 3 Milestones
- [ ] Vulkan device filter abstraction (already partially in `vulkan_device_filter.h`)
- [ ] Cross-platform path mapping (currently Windows-only console path parsing)
- [ ] TLS slot manager independent of Windows TEB layout
- [ ] Build system: configure for Linux x64 + Vulkan, not just Windows + D3D12

### Phase 3 Dependencies
- Vulkan ICD matching across GPU vendors
- POSIX path mapping complement to Windows console path parsing
- ASM.js or WebAssembly fallback for shaders (experimental)

## Phase 4: Full Compatibility Layer

### Targeted Generalization
- All Xbox GDK imports forwarded to Windows/Valkan equivalents
- Complete shader translation rule set (26,503 corpus coverage)
- Full filesystem emulation (GAME:/ drive, overlay, persistent storage, mounts)
- All XGameRuntime classes operational offline

### Phase 4 Milestones
- [ ] 100% shader translation coverage (zero refused shaders)
- [ ] All D3D12 XBox imports forwarded or emulated
- [ ] Complete XGameRuntime class set (all thunks implemented or E_NOTIMPL)
- [ ] Full console path filesystem emulation (all GDK path types)
- [ ] GameInput fully operational (DualSense effects, lightbar)
- [ ] Persistent local storage with configurable quota

### Phase 4 Success Criteria
- Game EXE maps and starts without crashes
- D3D12 device creation succeeds via Xbox-ABI proxy
- All runtime classes queryable through QueryApiImpl
- Filesystem operations (create, read, delete, find) behave like console
- Input (controller) works with DualSense features
- Persistent storage within quota works across sessions

## Generalization Principles (from Prompt.md)

1. **Keep game-specific implementations separate from generic runtime infrastructure** - Xbox→Windows translation stays in shims/; generic PE loading in runtime/src/
2. **Classify every subsystem as generic/reusable/Xbox-specific/Windows-specific/game-specific/experimental/partially implemented/deprecated** - Done in COMPONENT_INVENTORY.md
3. **Never claim functionality exists merely because source files or function declarations exist** - Evidence required from actual source code
4. **Preserve existing behavior wherever possible** - Fix, don't disable; translator rules, not skip lists
5. **Count over the corpus, not a sample** - Rules validated across all 26,503 shaders
6. **Do not assume Xbox and Windows implementations of the same API are interchangeable** - Explicitly documented in COMPATIBILITY_DESIGN.md
7. **Do not assume CPU instruction compatibility guarantees executable compatibility** - PE loading, relocations, TLS all need explicit handling
8. **Document important architectural decisions** - Prompt.md Section 8: all new docs identify Project/Studio/Inherited codebase
9. **Identify technical assumptions explicitly** - Each debt item documents its assumptions
10. **Explain potentially breaking changes before making them** - Roadmap phases are incremental