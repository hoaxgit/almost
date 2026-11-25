# DEVELOPMENT_GUIDELINES.md
## Project Almost - Development Guidelines

**Studio:** Almost Studios  
**Inherited codebase:** Project Almost  

## 1. General Principles (from Prompt.md Section 7)

### Mandatory Requirements
- **Do not rewrite the entire repository** - Preserve working implementations; rewrite only when strictly necessary
- **Do not delete existing functionality without explicit approval** - Every removal must be approved; no silent deletions
- **Do not silently change public interfaces** - All public API changes must be documented; deprecation path required
- **Do not invent undocumented functionality** - New behavior must have evidence (measured, documented, or tested)
- **Do not claim successful implementation without verification** - Verification required: tests pass, corpus coverage maintained, or explicit unknown status
- **Avoid unnecessary dependencies** - Prefer standard library; third-party only when technically justified
- **Write automated tests for new functionality** - No new code without tests (Prompt.md Section 183, 185)
- **Preserve existing behavior wherever possible** - Changes must be backward-compatible unless explicitly breaking
- **Keep subsystems independently testable** - Each subsystem has standalone test fixtures; no untestable couplings
- **Document important architectural decisions** - Prompt.md Section 8: all new docs identify Project/Studio/Inherited codebase
- **Identify technical assumptions explicitly** - Each decision documents its assumptions (per Prompt.md Section 214-220)

### Development Philosophy (Prompt.md Section 287-291)
- **Understand** - Investigate complete repository before changing anything
- **Preserve** - Keep working implementations; treat inherited codebase as production software
- **Generalize** - Make components reusable beyond the specific compatibility use case
- **Improve** - Incremental improvements over wholesale rewrites

## 2. Workflow Requirements

### Pre-Modification Workflow
1. **Read Prompt.md entirely** - Established rules govern all changes
2. **Check coordination issue** - Verify no one else has claimed this work (AGENTS.md)
3. **Present findings and WAIT for explicit approval** - Prompt.md Section 10: present audit findings, wait for go-ahead
4. **Make changes in small, reviewable increments** - Prompt.md Section 190: one increment at a time
5. **Document architectural decisions** - New docs must identify Project/Studio/Inherited codebase
6. **Write automated tests** - New functionality requires tests (Prompt.md Section 183, 185)
7. **Run existing tests first** - Ensure green before modifying anything
8. **Use `python -I` for Python tools** - Standard library only unless file explicitly imports numpy/Pillow

### Code Contribution Rules
- **Never claim functionality exists merely because source files or function declarations exist** - Evidence from actual source code required (Prompt.md Section 121)
- **Fix, don't disable** - Rejected shaders get translator rules, not skip lists (Prompt.md Section 181)
- **Count over the corpus, not a sample** - Rules must hold across all 26,503 shaders, not a subset (Prompt.md Section 112-113)
- **Do not migrate existing technologies without documented technical justification** (Prompt.md Section 67-68)
- **All new documentation must identify:** Project: Project Almost, Studio: Almost Studios, Inherited codebase: Project Almost (Prompt.md Section 204-208)

### Testing Requirements
- **Run translator test suite:** `python -I scripts/run_semantic_tests.py tests/semantic`
- **Run unit tests:** `python -m pytest tests/test_*.py` (with virtual env + requirements.txt)
- **No game data in commits** - `python -I scripts/check_no_game_data.py <path>` checks first bytes
- **New translator rules:** Add `.asm` fixture in `tests/semantic/fixtures/` + `test_*.py` in `tests/semantic/`
- **New shader rule evidence:** Must show measured behavior, not speculated
- **Preserve test independence** - Tests must not require game executable or game data

### Branch & PR Workflow
- **Claim work in coordination issue** - AGENTS.md: `gh issue list -R hoaxgit/almost --state open --search "Coordinating who works on what in:title"`
- **Put item ID in pull request** - Reference coordination issue number
- **`Depends on: #N` for every PR building on previous work** - Prompt.md Section 251
- **One issue per PR** - Single focused change per pull request
- **No merging without test approval** - All new tests must pass; existing tests must not regress

## 3. Technology Stack (from Prompt.md Section 3)

### Preferred Technologies (already used in repo)
- **C++20/23** - Runtime source code; `CMAKE_CXX_STANDARD 20` in CMakeLists.txt
- **CMake** - Build system; `cmake_minimum_required(VERSION 3.24)`
- **LLVM** - Shader translation (`llvm-mc` for disassembly, `spirv-as`/`spirv-val` for SPIR-V)
- **Windows SDK** - D3D12, DXGI headers and libraries
- **Vulkan** - Where existing implementations justify it (Vulkan backend, GPU info probing)
- **Python** - Development tooling (`make_game.py`, test runners, translation orchestration)
- **Dear ImGui** - Diagnostics (ALMOST TUI progress board)
- **GoogleTest** - CTest test framework (runtime tests)
- **Dear ImGui** - Diagnostics (mentioned in Prompt.md Section 64)

### Do NOT Migrate Without Justification
- Do not replace working C++20 with C# or other language without Prompt.md Section 67 justification
- Do not migrate from CMake to other build system without documented technical justification
- Do not replace LLVM-based translation without evidence it solves a proven problem

## 4. subsystem Classification & Work

### Classification per Prompt.md Section 5
Every major subsystem must be classified as:
1. Generic and reusable
2. Xbox-specific
3. Windows-specific
4. Original-game-specific
5. Experimental
6. Partially implemented
7. Deprecated or obsolete

And maturity:
- Fully implemented and tested
- Implemented but untested
- Partially implemented
- Experimental
- Planned
- Nonfunctional

### Classification Examples (from audit)
| Subsystem | Classification | Maturity |
|---|---|---|
| PE Loader core | Generic and reusable | Fully implemented |
| Import thunk registry | Generic and reusable | Fully implemented |
| File system shims | Xbox-specific | Fully implemented |
| D3D12 XS shims | Xbox-specific | Fully implemented |
| XGameRuntime shims | Xbox-specific | Fully implemented |
| Shader translation | Generic | Partially implemented (some rules missing) |
| Wave64 emulation | Experimental | Experimental (GPU-dependent) |
| BVH builder | Xbox-specific | Partially implemented (shared memory cap) |
| Persistent storage | Generic | Partially implemented (2 GiB fixed quota) |
| GameInput shim | Experimental | Experimental (SDL3-dependent) |
| TLS management | Experimental | Partially implemented (complex thread logic) |
| Vulkan backend | Generic | Fully implemented (where HW available) |
| Root signature forwarding | Xbox-specific | Partially implemented (flag clearing gaps) |

### When Classifying Your Changes
1. Determine classification based on where code lives and what it operates on
2. Document classification in file header or nearby documentation
3. If crossing classifications (e.g., generic code with Xbox dependency), document the crossing
4. Never claim "generic" if code operates on Xbox-specific data without adaptation

## 5. Incremental Change Pattern

### Required Pattern (Prompt.md Section 190)
- Work in small, reviewable increments
- Each increment must be reviewable (small diff, clear purpose)
- Test existing behavior before and after each increment
- Document what changed and why

### Increment Workflow
1. Identify single, well-scoped change (one bug fix, one new rule, one documentation update)
2. Write or update tests for the change
3. Make the minimal code change
4. Run tests to verify
5. Document in appropriate docs file (ARCHITECTURE.md, TECHNICAL_DEBT.md, etc.)
6. Commit with clear message referencing Prompt.md guidelines
6. Open PR with coordination issue reference

### Rollback Pattern
- Each increment must be independently reversible
- Must not break unrelated subsystems
- Must preserve existing test passes

## 6. Documentation Requirements (Prompt.md Section 8)

### Required Documentation Files
All new documentation must identify:
- **Project:** Project Almost
- **Studio:** Almost Studios
- **Inherited codebase:** Project Almost

### Created Files (this audit)
- `docs/ARCHITECTURE.md` - System architecture overview
- `docs/COMPONENT_INVENTORY.md` - Component maturity and reusability classification
- `docs/TECHNICAL_DEBT.md` - Prioritized technical debt list
- `docs/GENERALIZATION_ROADMAP.md` - Phase-based generalization plan
- `docs/COMPATIBILITY_DESIGN.md` - Compatibility principles and API forwarding map
- `docs/DEVELOPMENT_GUIDELINES.md` - This file: incremental change patterns, workflow

### Documentation Creation Rules
- Write documentation alongside code changes, not afterward
- Each new file must follow the header format (Project/Studio/Inherited codebase)
- Keep documentation concise - "Prioritize working, testable software over impressive-looking code" (Prompt.md Section 275)
- Do not overwhelm repository with unnecessary abstractions (Prompt.md Section 273)
- All caps/markdown headers must be consistent with existing docs style

## 7. Code Style & Conventions

### C++ (runtime/src/ and runtime/shims/)
- **C++20** - `CMAKE_CXX_STANDARD 20` in CMakeLists.txt
- **/utf-8** source file encoding (Prompt.md Section 7: preferred technologies)
- **/W4 /permissive-** - Warning level and strict parsing (CMakeLists.txt)
- **/EHsc** - Exception handling for MSVC (Windows-only builds)
- **/GS-** - Buffer security check disable for shim/test code (PE loader uses /GS- in some targets)
- **/OPT:NOREF** - Keep referenced objects (many test targets)
- **Win32_LEAN_AND_MEAN NOMINMAX** - Standard precludes across repo
- **Static CRT** - `/MT` or `/MultiThreaded$<$<CONFIG:Debug>:Debug>` for TLS host and test fixtures

### Python (make_game.py and tools/)
- **Python 3.12+** - Required (`make_game.py` line 66-67)
- **`python -I`** - Standard library only (make_game.py runs with `-I` flag)
- **No numpy/Pillow in core tools** - Those are texture/model tools that fall back when absent (Prompt.md Section 63-64)
- **Shell newlines:** `\n` with `newline="\n"` in `open()` calls (Prompt.md text write convention)
- **Shell quoting:** PowerShell `&&` not valid in PS 5.1; use `cmd1; if ($?) { cmd2 }` (AGENTS.md)

### Windows PowerShell Usage
- **Prefer full cmdlet names** over aliases (AGENTS.md: `Get-ChildItem` not `dir`)
- **Quote paths with spaces** using double quotes (AGENTS.md)
- **Use `;` for sequential commands** PowerShell 5.1 does not support `&&`
- **AVOID changing directories inside commands** - Use `workdir` parameter instead
- **Prefer `Get-ChildItem` over `dir`** (AGENTS.md convention)

### Python Tool Invocation
```python
# Correct: using py() helper from make_game.py
py("rdna2spirv.py", disasm, "--out", spirv, "--shard", "0/8")

# Correct: direct python -I call
subprocess.run([sys.executable, "-I", tool, *args])

# Never: bare python without -I on version checks
# Always: sys.version_info check before operations
```

## 8. Subsystem Independence

### Test Independence Requirements
- **PE loader tests** - Build their own PE fixtures (tiny_dll.cpp, tiny_exe.cpp); no game data needed
- **Shim tests** - Use mock import tables; no GPU required
- **Shader translation tests** - Standalone .asm fixtures in `tests/semantic/fixtures/`; no LLVM/SPIR-V tools needed for some (run_semantic_tests.py)
- **Runtime tests** - CTest executables; some need d3d12.lib (Windows only), some pure stdlib
- **No test should require game executable** - All test fixtures are hand-built, no game content

### Subsystem Coupling Limits
- **Runtime should not depend on D3D12 types in headers** - Forward declarations only (vulkan_backend.h uses abstract interfaces)
- **Shims should not depend on each other circularly** - Registry → file_system, but not reverse
- **Tools should not require game data** - All work on user's own copy; `check_no_game_data.py` enforces this
- **Tests should be runnable in isolation** - Individual test files import only what they need

## 9. Error Handling Patterns

### Runtime Error Handling
- **Throw std::runtime_error** for unrecoverable errors (pe_loader.cpp)
- **Return HRESULT/E_NOTIMPL** for unimplemented imports (registry.cpp shim pattern)
- **Log to stderr, continue** for non-critical (many shim thunks)
- **TerminateProcess** only for `ALMOST_UNIMPLEMENTED=stop` (registry.cpp line 205)

### Shader Stage Error Handling
- **fail()** in make_game.py - exits build with error message (line 113)
- **drop()** - removes stage-marking files so stage re-runs (line 1725)
- **verify_translation_completeness()** - fails if shader count mismatch (line 1888)
- **shader_outputs_reusable()** - checks before re-running stage (line 1168)

### File System Error Handling
- **SetLastError()** before returning FALSE (file_system.cpp, all WINAPI exports)
- **GetLastError()** after failed API call to distinguish errors (file_system.cpp)
- **error codes** documented per function (create_file_w, delete_file_w, etc.)

## 10. Specific Do's and Don'ts

### DO
- Preserve existing behavior when modifying code
- Write tests before or alongside new functionality
- Document assumptions explicitly
- Classify subsystems per Prompt.md Section 5
- Work in small, reviewable increments
- Use `python -I` for Python tooling
- Run existing tests before making changes
- Keep game-specific code separate from generic runtime
- Fix, don't disable (translator rules, not skip lists)
- Count over the corpus (26,503 shaders), not a sample
- Identify technical assumptions explicitly

### DON'T
- Rewrite the entire repository
- Delete existing functionality without approval
- Silently change public interfaces
- Invent undocumented functionality
- Claim successful implementation without verification
- Add unnecessary dependencies
- Assume Xbox and Windows APIs are interchangeable
- Assume CPU compatibility guarantees executable compatibility
- Implement DRM circumvention or auth bypasses
- Use `&&` in PowerShell 5.1 (use `; if ($?) {}` instead)
- Change directories inside bash/PowerShell commands (use workdir instead)
- Overwhelm repo with unnecessary abstractions
- Claim functionality based on filenames or docs alone
- Skip the audit/approval wait cycle (Prompt.md Section 10)

## 11. Coordination & Communication

### Issue Tracking
- Claim work in coordination issue: `gh issue list -R hoaxgit/almost --state open --search "Coordinating who works on what in:title"`
- Post findings as `finding: <one line>` in coordination issue
- Include file/line reference, why it matters, game data/hardware requirements
- Coordinator lists findings for others to work on

### Pull Request Requirements
- Put item ID from coordination issue in PR
- `Depends on: #N` for every PR building on previous work
- AI-assisted changes: state in PR description what was tested and on which GPU/driver
- No PR without passing tests (existing + new)
- One issue per PR maximum

### Communication Style (Prompt.md Section 257-276)
- Act as technically rigorous engineering collaborator
- Be detailed and precise
- Challenge assumptions when necessary
- Explain tradeoffs
- Prefer evidence over speculation
- When something is unknown, explicitly say so
- Do not overwhelm with unnecessary abstractions
- Prioritize working, testable software over impressive-looking code
- AI-assisted: disclose in PR, test on GPU, report counts before/after