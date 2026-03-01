# 26-03-01 library-first-refactor

## 0) Problem Framing

### Goal (engineering terms)
Refactor `watpocket` from “CLI-only implementation” into a **reusable C++ library** with a stable public API, while keeping a `watpocket` binary that is a thin wrapper over that API.

### Key user decisions (from 2026-03-01)
1. Rename/export the CMake package to `watpocket` (so downstream uses `find_package(watpocket CONFIG REQUIRED)`).
2. C++ API only (no C ABI in this refactor).
3. Provide first-class trajectory-mode analysis in the library.
4. Chemfiles should be an internal dependency at the library boundary:
   - Public headers should not require Chemfiles.
   - Consumers can either: call watpocket APIs that do I/O internally, or integrate with their own Chemfiles objects without forcing a single Chemfiles dependency choice.

### In scope
- Create and export an installable `watpocket` library (`libwatpocket.*`) plus a `watpocket` binary.
- Define a minimal, stable public API in `include/watpocket/`.
- Move implementation out of `src/watpocket/main.cpp` into library sources.
- Keep current CLI behavior and tests passing.
- Update documentation in `codebase-analysis-docs/` in lockstep with code changes.

### Out of scope (explicit)
- Changing algorithmic semantics, selector language, output formats, or error strings (unless tests are updated intentionally).
- Adding non-NetCDF trajectory format support or new selection languages.
- Adding C ABI, Python bindings, or plugin system (can be future work).

### Assumptions
- Build system: CMake (existing).
- Platforms: existing CI targets (Linux/macOS/Windows).
- Compiler: supports C++23 (repo default).
- Determinism: outputs remain stable for same inputs (as today).

## 1) Clarifying “Chemfiles Objects” vs “Internal Dependency”

### Are requirements (3) and (5) contradictory?
They are **in tension**, but not contradictory if we separate:
- **Public ABI/API boundary** (installed library headers and exported symbols): must not expose Chemfiles types.
- **Convenience integration** for users who already use Chemfiles: can be provided as *optional adapters* that compile against the user’s Chemfiles headers, without watpocket itself depending on those headers in its installed interface.

### Feasible design that satisfies both
Use a 2-layer approach:

1. **Core installed library API** (`watpocket`):
   - Accepts watpocket-owned data structures (plain structs/vectors/spans of coordinates + ids).
   - Provides I/O entry points that take file paths and internally use the vendored (or configured) Chemfiles.
   - Does not mention Chemfiles in installed headers.

2. **Optional Chemfiles integration header** (header-only adapter, not part of the compiled ABI):
   - `include/watpocket/compat/chemfiles.hpp` (or similar).
   - Contains inline functions that accept `chemfiles::Frame` / `chemfiles::Topology` (from the *consumer’s* Chemfiles headers) and convert to watpocket core request types.
   - Because this adapter is inline and only uses Chemfiles in headers, it does not force watpocket’s compiled library ABI to depend on Chemfiles. Consumers who don’t want Chemfiles never include it.

Notes:
- This does not guarantee you can safely link two different Chemfiles builds into one process in all configurations. To reduce collision risk, prefer: watpocket links Chemfiles privately and uses hidden symbol visibility where possible; also provide a CMake option to link against an external/system Chemfiles instead of the vendored one.

## 2) Success Criteria (Measurable)

### Correctness / Compatibility
- Existing `ctest` suite passes, including CLI tests that check error strings and draw artifacts.
- CLI output matches current behavior for:
  - structure mode (`--draw .py/.pdb` behavior)
  - trajectory mode (`-o` required; `.nc` only; per-frame warning/skip; `.pdb` MODEL output)
  - selector invariants and error messages.

### Build / Packaging
- Build produces:
  - `watpocket` executable
  - `libwatpocket.*` library artifact
- Install/export supports downstream:
  - `find_package(watpocket CONFIG REQUIRED)`
  - `target_link_libraries(app PRIVATE watpocket::watpocket)` (or equivalent exported target)

### API Quality
- Public headers in `include/watpocket/`:
  - do not include Chemfiles or CGAL headers
  - are “minimally complete” for the supported use cases
  - clearly specify contracts (preconditions and error behavior)
- Provide a first-class trajectory API (streaming/callback), not only CLI glue.

### Performance (guardrail)
- No worse than current CLI on WCN sample inputs within noise (baseline before/after).
- No new per-frame heap churn in the hot loop beyond what’s required by the existing algorithm (measure if needed).

## 3) Architecture Sketch

### High-level modules
Split along reasons-to-change and dependency boundaries:

- `watpocket::api` (public facade):
  - stable types: selectors, analysis options, results, errors
  - entry points: structure analysis and trajectory analysis
- `watpocket::core` (implementation, not installed):
  - selection resolution
  - geometry hull build (CGAL)
  - water identification + classification
- `watpocket::io` (implementation, not installed):
  - internal Chemfiles-based readers (structure + trajectory)
  - parm7 parser
  - serializers (CSV, PDB hull, PyMOL)
- `watpocket::cli` (executable):
  - CLI11 parsing + mapping args to library calls

### Dependency direction (hard boundary)
- Public headers: depend only on the C++ standard library (and optionally `span`-like abstractions).
- Library implementation: may depend on Chemfiles + CGAL, but those are PRIVATE in the target’s usage requirements.
- CLI depends on CLI11 and on watpocket library.

### Dataflow (structure mode)
`path -> internal reader -> (selectors -> CA points) -> hull -> waters -> result -> optional serializers`

### Dataflow (trajectory mode)
`(topology path + traj path) -> internal reader -> precompute indices/refs -> frame loop -> per-frame hull + classify -> callback/accumulators -> result/statistics -> optional serializers`

### Error handling contract
- Library returns a structured error type (preferred) or throws a documented exception type; choose one and enforce consistently.
- CLI preserves current UX:
  - fatal setup errors -> print `Error: ...` and exit 1
  - per-frame errors -> warning to stderr and skip

## 4) Work Breakdown Structure (Phased)

### Phase 0: Baseline + Guardrails
1. Confirm baseline:
   - Build + run `ctest` and capture current outputs for WCN tests.
2. Freeze behavior contracts:
   - Identify exact error strings asserted by tests (trajectory gating, draw extension, output required, parm7 guidance).
   - Document them in the plan’s checklist and keep them stable during refactor.
Exit criteria:
- Baseline tests pass; behavior contracts enumerated.

### Phase 1: Public API Design (before moving code)
1. Define public API (no Chemfiles/CGAL in headers):
   - `watpocket::ResidueSelector`
   - `watpocket::ParseError` / `watpocket::Error` model
   - `watpocket::StructureResult`
   - `watpocket::TrajectoryResult` and `TrajectorySummary`
   - `watpocket::TrajectoryCallback` (callable invoked per successful frame)
2. Decide API style:
   - Prefer a small set of free functions or a `Watpocket` facade class, but keep construction simple.
3. Define trajectory API shape:
   - `analyze_trajectory(TopologySource, TrajectorySource, Options, Callback)` returning summary + stats.
   - Ensure callback sees the minimal per-frame data needed for CSV/draw/stats (residue ids, counts, hull edges optionally).
Deliverables:
- New header set in `include/watpocket/` with doc comments.
Exit criteria:
- CLI can be expressed purely in terms of the new API (no missing capability).

### Phase 2: CMake Target Split + Packaging Rename
1. Add library target (cannot share target name with executable):
   - Create `watpocket_lib` with `OUTPUT_NAME watpocket`.
   - Link `chemfiles` and `CGAL::CGAL` privately/publicly as appropriate (usage requirements should not leak to consumers).
   - Add export header generation (like `sample_library`) for shared builds.
2. Keep executable target `watpocket` and link it to `watpocket_lib`.
3. Rename installed/exported package to `watpocket`:
   - Update top-level `myproject_package_project(NAME watpocket ...)`.
   - Update `test/CMakeLists.txt` to `find_package(watpocket CONFIG REQUIRED)`.
4. Export targets with a stable namespace:
   - Install exports should provide `watpocket::watpocket` (set `EXPORT_NAME` on `watpocket_lib` or create a real target named `watpocket` in the export set if needed).
Exit criteria:
- Build succeeds; tests still compile (even if they fail at runtime until code moves).

### Phase 3: Move Implementation into Library (incremental slices)
Move code in slices that keep tests passing at each step:

Slice A: Selector parsing + residue lookup + CA resolution
- Extract from `main.cpp` into `src/watpocket_lib/selector.*`.
- Keep identical error strings.
- Unit-test selector parsing.

Slice B: parm7 topology parsing
- Extract into `src/watpocket_lib/parm7.*`.
- Keep identical error strings (chain selector + missing RESIDUE_CHAINID).
- Add targeted unit tests for parsing failures (avoid full datasets where possible).

Slice C: Geometry hull + halfspaces
- Extract into `src/watpocket_lib/geometry.*`.
- Preserve kernel (EPIK), degeneracy checks, inside-on-boundary behavior.
- Add small unit tests for degeneracy guards (synthetic points).

Slice D: Water identification + classification
- Extract into `src/watpocket_lib/water.*`.
- Ensure water heuristics remain identical.

Slice E: Serializers
- Extract CSV + stats into `io_csv.*`.
- Extract PDB + PyMOL writers into `io_draw.*`.
- Keep output formats stable; extend tests only if needed.

Slice F: High-level library facade
- Implement `analyze_structure_file` and `analyze_trajectory` using internal I/O and core components.

Exit criteria:
- `src/watpocket/main.cpp` becomes thin (CLI parsing + library calls).
- All tests pass.

### Phase 4: Optional Chemfiles Adapter (satisfy “works well with Chemfiles objects”)
1. Add header-only adapter:
   - `include/watpocket/compat/chemfiles.hpp`
   - Inline conversion helpers from `chemfiles::Frame` to watpocket request data.
   - Make this header optional; do not include it from core public headers.
2. Add a small compile-only test:
   - If project can locate Chemfiles headers (vendored), compile a tiny unit test that includes the adapter and calls a conversion function (no runtime needed).
Exit criteria:
- Consumers can integrate with their own Chemfiles objects without watpocket forcing Chemfiles into the core API.

### Phase 5: Documentation Sync + Hardening
1. Update `codebase-analysis-docs/codebasemap.md`:
   - new library targets + file layout
   - updated call traces and symbol index
2. Update `codebase-analysis-docs/assets/*.mmd`:
   - library boundary diagram: CLI vs library vs dependencies
   - updated architecture and dataflow diagrams
3. Add a short “Using as a library” section in `README.md`.
Exit criteria:
- Docs match reality; no “monolithic main.cpp” claims remain.

## 5) Testing & Verification Plan

### Unit tests (fast, deterministic)
- Selector parsing:
  - valid tokens
  - malformed tokens
  - empty tokens
- Geometry degeneracy guards using synthetic points:
  - <4 points
  - collinear
  - coplanar

### Integration tests (existing CLI tests)
- Keep current CMake tests that execute `watpocket` and validate:
  - `--help` works
  - `--version` matches
  - draw extension errors
  - trajectory `-o` required
  - `.py` rejected in trajectory mode
  - WCN draw PDB contains waters

### New library integration tests
- A “library smoke” test that calls:
  - `analyze_structure_file` on WCN sample PDB (if available) and checks:
    - does not throw
    - returns stable counts
  - `analyze_trajectory` on WCN parm7 + nc with a callback accumulating per-frame counts (minimal assertions).

### Determinism checks
- Ensure per-frame returned water residue ids are sorted and stable.
- Ensure hull edge ordering, if exposed, is either explicitly documented as unordered or stabilized.

## 6) Performance Engineering Plan (pragmatic)
- Baseline:
  - time structure-mode and a short trajectory run on WCN sample before refactor.
- Hypotheses:
  - refactor should be net-neutral; risk is extra allocations/copies at API boundaries.
- Guardrails:
  - keep per-frame allocations minimal; pass spans/refs where safe; avoid repeated string normalization in loops.

## 7) Risk Register + Mitigations

- Risk: Package rename breaks install tests and downstream usage.
  - Mitigation: update `test/CMakeLists.txt` in the same change; add an example snippet to README.
  - Detection: `ctest` fails on config package test.

- Risk: “Chemfiles internal” conflicts with “user links their own Chemfiles” (symbol collisions).
  - Mitigation: keep Chemfiles PRIVATE; prefer hidden visibility; offer CMake option to build against system Chemfiles; keep public headers Chemfiles-free.
  - Detection: downstream link/runtime issues; CI with shared builds if available.

- Risk: ABI exposure leaks CGAL/Chemfiles via public headers.
  - Mitigation: enforce `include/watpocket/*.hpp` includes only std headers; keep adapters in `compat/`.
  - Detection: CI/build fails in a minimal consumer project or includes show CGAL/Chemfiles.

- Risk: Changing error strings breaks tests.
  - Mitigation: centralize exact strings as constants in the library; keep tests as the contract.
  - Detection: `ctest` failures with regex mismatch.

- Risk: Trajectory API too opinionated (forces CSV/PDB).
  - Mitigation: expose callback-based per-frame result; keep serializers optional utilities.

## 8) Rollout / Migration
- Keep `watpocket` CLI as the stability anchor; refactor internals under it.
- Introduce library API as v0 (pre-1.0), but still design with stable contract discipline.
- After package rename, provide a short transition note in docs (old `myproject` name deprecated).

## 9) Deliverables Checklist

- CMake:
  - library target building `libwatpocket.*`
  - exported package `watpocket`
  - executable `watpocket` links the library; CLI11 only in the executable
- Public headers:
  - `include/watpocket/*.hpp` (Chemfiles/CGAL-free)
  - optional `include/watpocket/compat/chemfiles.hpp`
- Implementation split into multiple `.cpp/.hpp` units (no monolithic `main.cpp`)
- Tests:
  - existing CLI tests preserved
  - new library unit/integration tests added
- Docs updated:
  - `codebase-analysis-docs/codebasemap.md`
  - `codebase-analysis-docs/assets/library-boundary.mmd`
  - `codebase-analysis-docs/assets/architecture.mmd`
  - `codebase-analysis-docs/assets/data-flow.mmd`

## 10) Skill Topics Used (for this plan)

### cpp-api-design
- API as a Stable Contract (Ch1): treat CLI-tested behavior as contract; design library API to be stable.
- Minimal, Discoverable, Hard-to-Misuse Interfaces (Ch2): keep API small; avoid exposing CGAL/Chemfiles.
- Information Hiding and Pimpl Boundaries (Ch2/Ch3): keep heavy deps out of public headers; keep ABI stable.
- Requirements- and Use-Case-Driven API Design (Ch4): structure/trajectory/CLI mapping drives interface.
- Choose API Style Explicitly (Ch5): C++-only, small facade + free functions; optional adapter header.
- C++ API Surface Hygiene (Ch6): namespaces, export macros, symbol visibility discipline.
- Documentation as Behavioral Contract (Ch9): update codebase-analysis-docs and README in lockstep.
- Automated API Testing and Testable Design (Ch10): add library tests; keep CLI integration tests as regression suite.

### cpp-software-design
- Dependency-Driven Software Design (Ch1): enforce direction (public API -> std only; impl -> deps).
- Design for Change and Separation of Concerns (Ch1): split selector/geometry/water/io/cli.
- Design for Testability (Ch1): seams at I/O and per-frame analysis; callback-based trajectory API.
- Adapter pattern as integration tool (Ch6): optional Chemfiles adapter header.
- Strategy/Command via composition (Ch5): trajectory processing via callback strategy; optional output serializers.
- Architecture Documentation as a Design Tool (Ch2): keep `codebase-analysis-docs/assets/*.mmd` updated.

### cpp-design-patterns
- Ownership Modeling with RAII (Ch3/Ch5): explicit ownership of results/buffers; exception-safe file handling.
- Adapter and Decorator Composition (Ch16): adapter header translating Chemfiles frames to core request types.
- Type Erasure for Uniform Runtime Interfaces (Ch6): trajectory callback as value-based callable (`std::function`-like) if needed without template bloat.
- Fluent APIs and Builder Construction (Ch9): optional “options/builder” shape if the options grow (kept as a possible extension, not mandatory on day 1).

