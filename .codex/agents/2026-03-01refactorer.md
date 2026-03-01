# watpocket Library-First Refactorer Prompt (2026-03-01)

You are a C++ refactoring agent working in `/home/pbarletta/labo/26/watpocket`.

## Mandatory Process (Read First)
Follow `AGENTS.md`:

1. Read `codebase-analysis-docs/codebasemap.md` before making changes.
2. Read the relevant source files you will modify.
3. Make the requested code changes.
4. After **every** code change, immediately update documentation so it stays in sync:
   - `codebase-analysis-docs/codebasemap.md`
   - `codebase-analysis-docs/assets/*.mmd` when architecture/data flow/boundaries change
5. Keep documentation updates in the same change set as the code change.

Skill requirements (this refactor touches both geometry and molecular IO):

- Read and follow `/home/pbarletta/.codex/skills/cgal/SKILL.md` (Convex_hull_3 scope).
- Read and follow `/home/pbarletta/.codex/skills/chemfiles/SKILL.md`.

## Refactor Goal
Re-implement the project as a **library-first** design:

- Build a reusable `watpocket` C++ library that other programs can link against and call.
- Keep building a `watpocket` CLI binary, but make it a thin wrapper that calls the library.

This must preserve current behavior while introducing a clean, installable API.

## Current State (Baseline You Must Preserve)
Verify in-tree before edits, but expect:

- All production logic lives in `src/watpocket/main.cpp` in an anonymous namespace (selection parsing, parm7 parsing, hull build, water classification, CSV/PDB/PyMOL writers, trajectory loop).
- `src/watpocket/CMakeLists.txt` builds only an executable target `watpocket` and links: `chemfiles`, `CGAL::CGAL`, and `CLI11::CLI11`.
- Tests in `test/CMakeLists.txt` exercise the CLI contracts (help/version, draw extension errors, trajectory mode rules, WCN data-based draw checks). These must keep passing.

## Non-Negotiable Constraints

### Keep CLI Behavior Stable
Do not change:

- CLI options, required/optional rules, and mode gating.
- Output formats (CSV schema, draw artifacts) and error/warning strings used by tests.
- Structure-mode and trajectory-mode semantics (including “skip frame with warning” behavior).

### Keep Selection/Identity Semantics Stable
Do not relax or reinterpret existing invariants:

- `--resnums` is a custom CSV selector grammar: `RESID` or `CHAIN:RESID`.
- Multi-chain structures require chain-qualified selectors.
- Each selector must match exactly one residue; each selected residue must have exactly one `CA`.
- `parm7` chain-qualified selectors require `RESIDUE_CHAINID`.

### Keep Geometry Semantics Stable
Preserve:

- Kernel choice: `CGAL::Exact_predicates_inexact_constructions_kernel` (EPIK).
- Degeneracy errors: fewer than 4 points, all collinear, all coplanar are hard errors.
- Point-in-hull: boundary counts as inside (current halfspace/oriented-side semantics).

### Keep Dependency Boundaries Correct

- The **library** must not depend on `CLI11`.
- Vendored deps remain as-is: `external/chemfiles`, `external/cgal` (no network fetch for these).
- The **executable** may continue using `CLI11` for argument parsing only.

## Target Architecture

### Library Target
Add a library target that builds an artifact named `watpocket` (i.e., `libwatpocket.*`) and can be installed/exported.

Recommended approach (minimize disruption):

- Keep executable target name `watpocket` (tests and VS startup expect it).
- Add a new library target `watpocket_lib` with:
  - `set_target_properties(watpocket_lib PROPERTIES OUTPUT_NAME watpocket)`
  - `target_link_libraries(watpocket_lib PUBLIC chemfiles CGAL::CGAL)`
  - visibility/export header support (see how `sample_library` uses `GenerateExportHeader`)
- Link the executable to the library:
  - `target_link_libraries(watpocket PRIVATE watpocket_lib CLI11::CLI11 ...)`

Also provide a namespaced alias for consumers in the build tree:

- `add_library(watpocket::watpocket ALIAS watpocket_lib)`

### Public Headers (Installable API)
Create new public headers under `include/watpocket/` (this project currently has `include/myproject/` for template code; that’s fine).

Design the API to be:

- small and stable
- usable from other programs without going through the CLI
- not exposing CLI11
- avoiding CGAL types in public headers when feasible

Suggested minimum API surface (adapt as needed, but keep it minimal):

- `watpocket::ResidueSelector` (chain optional + resid + raw)
- `watpocket::parse_residue_selectors(std::string_view)` with identical validation semantics
- “Core analysis” entry points, sufficient to implement the CLI:
  - analyze a structure file (one frame) and return a result
  - analyze a `chemfiles::Frame` given precomputed indices/refs (for embedding)
  - (optional) analyze trajectory frames in a loop via a callback interface, so the library does not require writing CSV itself unless you deliberately keep serialization in the library

Keep current serializers available to the CLI:

- CSV row/header writer
- trajectory statistics printer
- hull PDB writer (single frame + `MODEL` variant)
- PyMOL script writer (structure mode only)

If you keep serializers in the library, ensure they’re usable from programs without assuming stdout/stderr.

### Source Refactor
Move code out of `src/watpocket/main.cpp` into library translation units. A reasonable split:

- `selector.*`: selector parsing, residue lookup, CA index resolution
- `parm7.*`: minimal parm7 reader/types
- `geometry.*`: hull build, halfspace construction, point-in-hull test
- `water.*`: water oxygen refs, materialization, inside-hull classification
- `io_draw.*`: PDB/PyMOL writers
- `io_csv.*`: CSV writers and statistics helpers
- `errors.*`: exception messages/constants if needed to preserve test strings

Keep `src/watpocket/main.cpp` as a thin wrapper:

- parse CLI args (CLI11)
- decide mode (1 input vs 2 inputs)
- validate high-level constraints
- call library functions
- preserve error handling contracts (fatal => exit 1; per-frame => warning + skip)

## Packaging / Install / Export
Update CMake packaging so downstream projects can consume the library:

- Update `myproject_package_project(...)` in top-level `CMakeLists.txt` to install/export the library target and public headers.
- Ensure `PUBLIC_INCLUDES` includes the new `include/` subtree (so `include/watpocket/*.hpp` installs).
- Do not break `test/CMakeLists.txt` which currently does `find_package(myproject CONFIG REQUIRED)` unless you intentionally rename the package and update tests accordingly.

## Tests (Must Pass + Add Library Test)

1. All existing CLI tests must continue to pass unchanged unless you have a compelling reason to update them.
2. Add at least one new unit test covering the library API:
   - selector parsing happy path + error path
   - a small, cheap “analysis can be called” test (avoid heavy data; use existing WCN fixture only if needed)

## Documentation Sync (Mandatory)
After each code move/addition, update:

- `codebase-analysis-docs/codebasemap.md`:
  - update “monolithic main.cpp” statements
  - update the directory map and symbol index to match new files
  - update build/targets guidance to mention the new library target and dependency boundary (CLI11 in binary only)
- `codebase-analysis-docs/assets/library-boundary.mmd`:
  - show the new library boundary vs CLI wrapper boundary
- `codebase-analysis-docs/assets/architecture.mmd` and `data-flow.mmd`:
  - reflect new module boundaries (library modules called by CLI)

## Acceptance Criteria

- Configure/build succeeds with CMake.
- `ctest` passes (including the existing CLI tests).
- Build outputs:
  - executable `watpocket`
  - library artifact `libwatpocket.*`
- The executable’s implementation is predominantly library calls, not duplicated logic.
- The new API is installed/exported via the project’s CMake packaging flow.

