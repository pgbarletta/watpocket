# 26-03-01 Exported PointSoA Readers

## 1) Problem Framing

### Goal
Add **exported, high-level, std-only** APIs that return `watpocket::PointSoA` for caller-provided atom indices so downstream users can build workflows equivalent to:
- extract selected points
- compute hull
- classify waters inside hull

The immediate scope is API exposure of point extraction from:
- a structure input (first frame)
- a trajectory input at a specified frame

### Why this now (current-codebase alignment)
The codebase already completed the AoS -> SoA migration and has a private geometry bridge (`src/watpocket_lib/point_soa_cgal_adapter.*`) plus reusable extraction helper (`fill_points_from_atom_indices(...)`). This plan builds on that architecture and does **not** reintroduce AoS/public CGAL types.

### In Scope
- Export one structure reader function that opens input internally and returns `PointSoA`.
- Export one trajectory reader function that opens topology+trajectory internally, reads a specified frame, and returns `PointSoA`.
- Both functions accept:
  - `atom_indices` (0-based)
  - optional selection `label` used in validation/error messages
- Both functions throw `watpocket::Error` on invalid atom indices and IO/format errors.
- Add tests for API behavior, error paths, and determinism.
- Keep public API CGAL/Chemfiles-free in public headers.

### Out of Scope
- Parallel frame processing implementation.
- Breaking migration of existing analysis APIs.
- New geometry behavior changes.
- Benchmark-report deliverables.

### Assumptions
- Build system remains CMake and binary target remains `watpocket`.
- Public header remains `include/watpocket/watpocket.hpp` (std-only).
- Internal extraction helper `fill_points_from_atom_indices(...)` stays the canonical atom-index validation/fill path.
- Trajectory frame numbering for new API is **1-based** to match existing user-facing reporting.
- Default label when omitted is `"selected atoms"`.
- Trajectory support in this API mirrors current runtime constraints: NetCDF trajectory (`.nc`) with topology from structure or parm7/prmtop.

## 2) Public API Contract (Exact)

### Proposed exported APIs

```cpp
[[nodiscard]] WATPOCKET_LIB_EXPORT PointSoA read_structure_points_by_atom_indices(
  const std::filesystem::path& input_path,
  const std::vector<std::size_t>& atom_indices,
  std::string_view label = "selected atoms");

[[nodiscard]] WATPOCKET_LIB_EXPORT PointSoA read_trajectory_points_by_atom_indices(
  const std::filesystem::path& topology_path,
  const std::filesystem::path& trajectory_path,
  std::size_t frame_number,
  const std::vector<std::size_t>& atom_indices,
  std::string_view label = "selected atoms");
```

### Input/output semantics
- `atom_indices` are 0-based and interpreted against the loaded frame atom order.
- Output size is exactly `atom_indices.size()`, preserving caller index order (duplicates allowed and preserved).
- `atom_indices.empty()` is valid and returns empty `PointSoA`.
- `frame_number` is 1-based for trajectory API; `0` is invalid.

### Error contract
- Any invalid atom index throws `watpocket::Error` and includes:
  - `label`
  - offending atom index
  - frame atom count
- Structure file not found / unreadable throws `watpocket::Error` with path context.
- Trajectory API throws `watpocket::Error` for:
  - missing topology/trajectory path
  - unsupported trajectory extension (non-`.nc`)
  - `frame_number == 0`
  - out-of-range frame number (message includes requested frame and available frame count)
  - topology/trajectory atom-count mismatch

### Dependency boundary rule
- `include/watpocket/watpocket.hpp` remains std-only and must not include Chemfiles/CGAL headers/types.

## 3) Design Review (cpp-api-design + cpp-software-design)

This section records design choices that were updated to match better API/design practice.

### Choice A: Keep additive, minimal API surface
- Decision: add only two exported readers; avoid introducing additional polymorphic abstractions now.
- Reasoning:
  - `cpp-api-design` (minimal, discoverable, hard-to-misuse interfaces)
  - `cpp-software-design` (avoid speculative extension points; design for real change)

### Choice B: Separate concerns by responsibility
- Decision: retain a pure extraction helper (`fill_points_from_atom_indices`) and keep exported readers as IO wrappers.
- Reasoning:
  - `cpp-software-design` (separation of concerns, dependency-driven design)
  - `cpp-api-design` (stable contract + maintainable implementation boundary)

### Choice C: Explicit semantic contracts
- Decision: codify 0-based atom indices + 1-based frame numbering + empty-index behavior.
- Reasoning:
  - `cpp-software-design` (semantic abstractions / contract clarity)
  - `cpp-api-design` (documentation as behavioral contract)

### Choice D: No public leakage of backend details
- Decision: keep Chemfiles/CGAL types private; exported API remains std-only owning return type.
- Reasoning:
  - `cpp-api-design` (information hiding, loose coupling)
  - `cpp-software-design` (ownership of abstractions)

### Choice E: Performance tradeoff acknowledged but bounded
- Decision: keep high-level convenience APIs opening files internally per call; defer handle-based batched API.
- Reasoning:
  - `cpp-api-design` (measure-first performance)
  - `cpp-software-design` (avoid premature abstraction complexity)

## 4) Architecture and Abstraction Ownership

### Public/API layer
- File: `include/watpocket/watpocket.hpp`
- Owns only exported declarations + contract docs.

### Library implementation layer
- File: `src/watpocket_lib/watpocket.cpp`
- Owns readers:
  - open/read structure frame
  - open/read topology+trajectory frame
  - route into shared index->SoA extraction helper
- Owns conversion of implementation exceptions to `watpocket::Error`.

### Private geometry layer (unchanged)
- Files: `src/watpocket_lib/point_soa_cgal_adapter.hpp/.cpp`
- Not touched by this feature except compatibility assurance.

### Dataflow (target)
- Structure API:
  - input path -> Chemfiles read first frame -> `fill_points_from_atom_indices` -> return `PointSoA`
- Trajectory API:
  - topology/trajectory paths -> sanity checks (format/frame count/atom count) -> read requested frame -> `fill_points_from_atom_indices` -> return `PointSoA`

## 5) Work Breakdown Structure (File-Level)

### Phase 0: Contract freeze
Task 0.1: finalize exact signature and behavior
- Files:
  - `include/watpocket/watpocket.hpp`
- Done when:
  - semantics for 0-based indices, 1-based frame, empty indices, default label are documented.

### Phase 1: Public header addition
Task 1.1: add exported declarations + comments
- Files:
  - `include/watpocket/watpocket.hpp`
- Acceptance:
  - package consumer compiles with no Chemfiles/CGAL includes from public headers.

### Phase 2: Structure reader implementation
Task 2.1: implement `read_structure_points_by_atom_indices`
- Files:
  - `src/watpocket_lib/watpocket.cpp`
- Requirements:
  - rejects non-existent path
  - rejects parm7/prmtop in structure API (same policy as analyze structure path)
  - reads first frame and fills SoA via shared helper
- Acceptance:
  - deterministic coordinate equality for known fixture
  - invalid index error contains label and offending index

### Phase 3: Trajectory reader implementation
Task 3.1: implement `read_trajectory_points_by_atom_indices`
- Files:
  - `src/watpocket_lib/watpocket.cpp`
- Requirements:
  - validate `frame_number >= 1`
  - validate `.nc` trajectory extension
  - load topology with existing topology-path rules (structure or parm7/prmtop)
  - atom-count sanity checks against requested frame path
  - frame range validation using trajectory size / step addressing
  - SoA fill via shared helper
- Acceptance:
  - valid frame extraction test passes
  - out-of-range frame test passes
  - invalid index and atom-count mismatch tests pass

### Phase 4: Tests
Task 4.1: API tests
- Files:
  - `test/watpocket_api_tests.cpp`
- Cases:
  - structure valid extraction
  - structure invalid index
  - structure empty indices
  - trajectory valid extraction
  - trajectory `frame_number == 0`
  - trajectory out-of-range frame
  - trajectory invalid index
  - trajectory empty indices
  - deterministic repeated-call equality

Task 4.2: package consumer compile guard
- Files:
  - `test/package_consumer.cpp` (only if needed)
  - `test/CMakeLists.txt` (if new consumer assertion needed)
- Acceptance:
  - `package.consumer_runs` remains green.

### Phase 5: Documentation sync
Task 5.1: update maps/diagrams
- Files:
  - `codebase-analysis-docs/codebasemap.md`
  - `codebase-analysis-docs/assets/*.mmd` (if flow diagrams change)
- Acceptance:
  - docs mention new exported reader APIs and where they live.

## 6) Verification Plan

### Build + tests
- `cmake --build build --target watpocket watpocket_api_tests watpocket_package_consumer`
- `ctest --test-dir build -R "^(api|package)\\." --output-on-failure`
- `ctest --test-dir build -R "^cli\\." --output-on-failure` (regression guard)

### Behavioral assertions
- Output coordinates exactly match `frame.positions()[idx]` order.
- Duplicate indices produce duplicate output points in same order.
- Empty index list returns `PointSoA{size=0}`.
- All error paths throw `watpocket::Error` (not raw std exceptions).

### Non-regression
- Existing `analyze_structure_file` and `analyze_trajectory_files` behavior unchanged.
- No CGAL/Chemfiles leakage through public header.

## 7) Risks and Mitigations

- Frame numbering confusion (medium/high)
  - Mitigation: explicit 1-based contract in header docs + tests for 0/out-of-range.

- API error inconsistency (medium/medium)
  - Mitigation: reuse shared fill helper and centralized boundary catches.

- Convenience API overhead for repeated calls (high/medium)
  - Mitigation: document as high-level convenience; follow-up batched/handle APIs later.

- Backend leakage through headers (low/high)
  - Mitigation: package consumer test + std-only signatures.

## 8) Detail Sufficiency Check

This revised plan is considered implementation-ready because it now includes:
- exact signatures and semantic contracts
- precise error-path expectations
- file-by-file WBS with acceptance criteria
- explicit test matrix including edge cases (`empty`, duplicate indices, frame bounds)
- regression and boundary checks aligned with current codebase architecture

## 9) Deliverables Checklist

- [ ] 2 exported reader APIs in public header with full contract docs
- [ ] structure + trajectory reader implementations in `watpocket.cpp`
- [ ] shared index->SoA extraction reuse (no duplicated bounds logic)
- [ ] API tests covering success + failure + determinism + empty/duplicate index semantics
- [ ] package consumer/public-header boundary checks passing
- [ ] codebase docs updated in same change set
