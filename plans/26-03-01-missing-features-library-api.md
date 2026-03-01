# 26-03-01 missing-features-library-api

## 1) Problem Framing

### Goal
Finish the “library-first” conversion by implementing the *missing pieces*:

- The `watpocket` CLI becomes a thin wrapper that calls the library API (no Chemfiles/CGAL in the CLI target).
- The exported CMake package `watpocket` exports **only** the library target as `watpocket::watpocket`.
- The public installed headers stay **std-only** (no Chemfiles dependency for consumers).
- `analyze_trajectory_files()` remains a first-class library API and continues to support writing CSV and draw-PDB directly.

### Current state (baseline)
- Library target exists: `watpocket_lib` (monolithic implementation is acceptable for now).
- Public header exists: `include/watpocket/watpocket.hpp` (Chemfiles-free).
- CLI target still compiles/links with Chemfiles/CGAL because `src/watpocket/main.cpp` is still monolithic.
- Package name is already set to `watpocket`, but packaging currently exports more than just the library target.

### In scope
- CMake packaging/export changes to export library only.
- CLI refactor to use library entry points and preserve existing CLI behavior/tests.
- Add at least one “consumer build” check ensuring public API does not require Chemfiles.
- Update `codebase-analysis-docs/` to match the new architecture.

### Out of scope
- Splitting `src/watpocket_lib/watpocket.cpp` into multiple translation units (explicitly deferred).
- Adding new formats, selection languages, or changing output schemas.
- Introducing a C ABI.

## 2) Success Criteria (Measurable)

### Build/Install/Export
- `cmake -S . -B build && cmake --build build` succeeds.
- `cmake --install build` produces:
  - a library artifact `libwatpocket.*`
  - installed headers under `include/watpocket/`
  - a CMake config package consumable via `find_package(watpocket CONFIG REQUIRED)`
- The exported config exposes `watpocket::watpocket` and does **not** export the `watpocket` executable target.

### Consumer UX (no Chemfiles dependency)
- A minimal consumer can:
  - `#include <watpocket/watpocket.hpp>`
  - link to `watpocket::watpocket`
  - compile without needing Chemfiles headers.

### CLI compatibility
- `ctest` passes (including existing CLI tests).
- CLI behavior remains identical (error strings and output formats that tests rely on).

## 3) Architecture Sketch (Target End State)

- `watpocket_lib` (shared library):
  - owns: Chemfiles IO, CGAL hull compute, water classification, CSV/draw writing helpers.
  - exports: `watpocket` C++ API (exceptions on error).
- `watpocket` executable:
  - owns: CLI11 argument parsing and mapping.
  - depends only on: CLI11 + `watpocket_lib`
  - no Chemfiles/CGAL includes required.

Dependency direction:
- Installed headers (`include/watpocket/*.hpp`): std-only.
- Library implementation (`src/watpocket_lib/watpocket.cpp`): Chemfiles + CGAL allowed, PRIVATE to the target.

## 4) Work Breakdown Structure (WBS)

### Phase 0: Baseline + Contract Freeze
1. Record the CLI-contract strings that tests assert (regexes in `test/CMakeLists.txt`):
   - draw extension rejection
   - trajectory draw `.py` rejection
   - trajectory requires `-o`
   - parm7 requires two inputs
   - parm7 chain selector without `RESIDUE_CHAINID`
2. Build and run `ctest` as the baseline gate.

Exit criteria:
- Baseline passes and the “don’t break these strings” list is explicit in the plan’s checklist.

### Phase 1: Packaging: Export Only `watpocket::watpocket`
1. Update top-level `myproject_package_project(...)` invocation to export/install only:
   - `watpocket_lib` (exported as `watpocket::watpocket` via `EXPORT_NAME watpocket`)
2. Ensure the executable `watpocket` still installs as a runtime artifact (optional):
   - Either keep a separate `install(TARGETS watpocket RUNTIME ...)` without exporting it,
   - Or leave it uninstalled for now; tests should still pass in build-tree.
3. Add a “config package consumer compile” target under `test/`:
   - tiny `consumer.cpp` that includes `watpocket/watpocket.hpp` and calls `watpocket::build_version()`
   - link it against `watpocket::watpocket`
   - this validates exported usage requirements don’t leak Chemfiles headers/libs.

Exit criteria:
- Configure/generate succeeds (no export-set dependency errors).
- Consumer compile test builds.

### Phase 2: CLI Wrapper Refactor (Keep Behavior Identical)
Goal: `src/watpocket/main.cpp` becomes CLI-only (CLI11 + calls into watpocket library).

1. Replace direct helper calls in `main.cpp` with library calls:
   - selector parsing: `watpocket::parse_residue_selectors(resnums)`
   - structure mode:
     - `auto r = watpocket::analyze_structure_file(input_path, selectors)`
     - print stdout exactly as before (including “No water oxygen…” message and selection line)
     - if `--draw`:
       - `.py`: call `watpocket::write_pymol_draw_script(input_path, draw_path, r.hull.edges, r.water_residue_ids)`
       - `.pdb`: call `watpocket::write_hull_pdb(draw_path, r.hull, r.water_atoms_for_pdb)`
   - trajectory mode:
     - keep the CLI gating rules (`-o` required; `.nc` only; `.pdb` draw only) in CLI so error strings remain stable
     - call `watpocket::analyze_trajectory_files(topology_path, trajectory_path, selectors, csv_out, draw_out, callbacks)`
     - callbacks should reproduce current warning behavior:
       - `on_warning`: prints the warning message to stderr (exact formatting)
     - print the same “Wrote trajectory…” lines and processed frame counts (either:
       - derive from `TrajectorySummary` returned by library, or
       - keep current CLI prints but source data from the summary)
2. Remove Chemfiles/CGAL includes from `src/watpocket/main.cpp`.
3. Update `src/watpocket/CMakeLists.txt` to drop `chemfiles` and `CGAL::CGAL` link deps once includes are gone.

Exit criteria:
- `watpocket` executable builds with only `CLI11::CLI11` + `watpocket_lib`.
- All CLI tests pass unchanged.

### Phase 3: Documentation Sync (Mandatory)
Update docs after each code move that changes boundaries:
1. `codebase-analysis-docs/codebasemap.md`:
   - update “monolithic main.cpp” statements (CLI is now wrapper; library holds implementation)
   - update build/targets description (watpocket_lib + watpocket binary linkage)
   - update symbol index for new public API functions
2. `codebase-analysis-docs/assets/library-boundary.mmd`:
   - reflect that Chemfiles/CGAL are inside the library boundary, not visible to consumers
3. `codebase-analysis-docs/assets/architecture.mmd` and `data-flow.mmd`:
   - show CLI -> library calls, and library owning IO/compute/output writers

Exit criteria:
- Docs match the implemented architecture and build targets.

## 5) Testing & Verification Plan

- Unit tests:
  - keep `test/watpocket_api_tests.cpp` as API sanity checks
  - add the config-package consumer build target (compile-time guardrail)
- Integration tests:
  - existing CLI tests remain the primary regression contract
- Determinism:
  - ensure water residue ids remain sorted and stable across refactor

## 6) Performance Plan (Minimal)
- Baseline: time a short WCN run before and after CLI wrapper refactor.
- Guardrail: wrapper refactor should be net-neutral; avoid unnecessary copying between CLI and library.

## 7) Risks + Mitigations

- Risk: Exporting executable target causes name collisions / confusing consumer usage.
  - Mitigation: export only `watpocket::watpocket` (library) as decided; install executable separately if needed.
- Risk: CLI error strings drift during refactor.
  - Mitigation: keep all CLI gating in CLI; treat tests as contract.
- Risk: Public headers accidentally include Chemfiles/CGAL.
  - Mitigation: enforce std-only includes in `include/watpocket/watpocket.hpp`; consumer compile test.

## 8) Deliverables Checklist

- CMake:
  - `watpocket` package exports only `watpocket::watpocket`
  - `watpocket` executable builds without Chemfiles/CGAL link deps
- Code:
  - `src/watpocket/main.cpp` is CLI-only wrapper
- Tests:
  - existing CLI tests pass
  - API tests pass
  - consumer compile test passes
- Docs:
  - `codebase-analysis-docs/codebasemap.md` updated
  - `codebase-analysis-docs/assets/*.mmd` updated for new boundaries

## 9) Skill Topics Used To Write This Plan

### cpp-api-design
- “API as a Stable Contract” (Ch1): treat CLI tests/error strings as contract while refactoring internals.
- “Minimal, Discoverable, Hard-to-Misuse Interfaces” (Ch2): keep public header Chemfiles-free; file-path entry points.
- “Information Hiding and Pimpl Boundaries” (Ch2/Ch3): hide Chemfiles/CGAL behind the library boundary.
- “C++ API Surface Hygiene” (Ch6): exported symbol policy + not leaking deps through headers.
- “Automated API Testing and Testable Design” (Ch10): add consumer compile test and API sanity tests.

### cpp-software-design
- “Dependency-Driven Software Design” (Ch1): strict dependency direction (CLI -> library; public headers std-only).
- “Design for Change and Separation of Concerns” (Ch1): move behavior into library, keep CLI as parser/mapping only.
- “Design for Testability” (Ch1): keep stable seams (callbacks, summary objects) to test without Chemfiles.
- “Architecture Documentation as a Design Tool” (Ch2): update `codebase-analysis-docs/` as part of the work.

### cpp-design-patterns
- “Ownership Modeling with RAII” (Ch3/Ch5): library owns IO resources; exception-based error model.
- “Adapter and Decorator Composition” (Ch16): CLI acts as a thin adapter from CLI11 args to library calls (no extra behavior).

