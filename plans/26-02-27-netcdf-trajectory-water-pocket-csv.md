# 26-02-27 netcdf-trajectory-water-pocket-csv

## 1) Problem Framing
- Goal: implement two-input mode in `watpocket` where input-1 is topology (`.pdb/.cif/.mmcif`) and input-2 is trajectory (NetCDF `.nc` for this phase), then compute per-frame pocket occupancy of water oxygens inside the convex hull defined by selected residues.
- In-scope:
  - NetCDF trajectory support (`.nc`) in two-input mode.
  - Topology-driven atom identity and sanity checks (atom-count compatibility).
  - Per-frame convex hull and per-frame water-inside-hull classification.
  - Optional `-o/--output` CSV path for trajectory mode; stdout fallback when omitted.
  - CSV fields: `frame` (1-based), `resnums` (space-separated residue numbers), `total_count`.
- Out-of-scope:
  - XTC/DCD implementation (reserved next phase).
  - New geometry algorithms beyond existing `convex_hull_3` + halfspace test.
  - PBC/unwrapping/alignment logic.
- Assumptions:
  - Build/toolchain remains current CMake + GCC/Clang setup.
  - Chemfiles NetCDF reader is available via vendored `external/chemfiles`.
  - Water residues do not require chain-qualified identity in outputs.
  - Existing selector semantics (`--resnums` with optional chain qualifiers) remain unchanged.

## 2) Success Criteria (Measurable)
- Correctness:
  - For each trajectory frame, output one CSV row with 1-based frame index.
  - `resnums` field is double-quoted; values separated by one space; empty set encoded as `""`.
  - `total_count` equals number of unique water residue IDs in `resnums`.
  - Topology/trajectory atom-count mismatch produces a hard error with both counts.
- Performance:
  - Process frames streaming from trajectory in one pass (`O(n_frames * (hull + water_test))`).
  - No per-frame topology reconstruction from file.
- Resource:
  - Memory bounded by one frame + cached atom-index vectors and hull intermediates.
- Portability:
  - Linux/macOS toolchains currently supported by project CMake flow.
- Maintainability:
  - Keep logic in `src/watpocket/main.cpp` helpers consistent with current monolithic architecture.
  - Keep docs synchronized (`codebasemap.md` and affected `.mmd` assets).

## 3) Architecture Sketch
- Modules/responsibilities:
  - CLI: parse positional inputs + `--resnums`, `-d/--draw`, new `-o/--output`.
  - Topology preparation: resolve selected CA atom indices and water oxygen atom references once from topology frame.
  - Trajectory loop: for each frame, materialize CA points and water oxygen positions from cached atom indices.
  - Geometry: compute hull and halfspaces each frame; classify waters against halfspaces.
  - Output: emit CSV row per frame to stdout or file stream.
- Dataflow:
  - Topology file -> topology frame -> cached `(CA atom indices, water oxygen refs)`.
  - Trajectory `.nc` -> frame -> CA coordinates -> convex hull -> halfspaces.
  - Same frame + cached water refs -> water coordinates -> inside test -> unique residue IDs -> CSV row.
- Concurrency model:
  - Single-threaded deterministic frame loop (existing style).
- Memory model:
  - Reuse vector-based data structures; no global mutable shared state.
- Error/logging:
  - Fail-fast exceptions with frame-context prefixes for per-frame failures.

## 4) Work Breakdown Structure (WBS)

### Phase 0: CLI Contract + Mode Gating
- Changes:
  - Add `-o/--output` option.
  - Enforce `-o` valid only for trajectory mode and `--draw` valid only for single-structure mode.
  - Gate two-input mode to NetCDF trajectories for this phase.
- Validation:
  - CLI smoke for invalid combinations.
- Performance measurement:
  - None.
- Exit criteria:
  - Clear errors for unsupported format/mode combinations.

### Phase 1: Topology-Driven Caches
- Changes:
  - Add helper to resolve CA atom indices once from topology frame.
  - Add helper to collect water oxygen references `(resid, atom_index)` once from topology frame.
  - Add helper to materialize 3D points from atom indices on arbitrary frame.
- Validation:
  - Existing selector/path invariants preserved.
  - Atom-index bounds checks on frame access.
- Performance measurement:
  - Confirm no repeated topology parsing in per-frame path.
- Exit criteria:
  - Cache construction succeeds for sample topology and selector set.

### Phase 2: NetCDF Trajectory Execution Path
- Changes:
  - Replace “not implemented” branch with trajectory frame loop.
  - Apply topology to trajectory (`set_topology`) and validate atom-count compatibility.
  - Compute hull/classification independently per frame.
- Validation:
  - Frame-level errors include frame number.
  - Boundary-inclusion behavior unchanged (`ON_POSITIVE_SIDE` rejection only).
- Performance measurement:
  - Record processed frame count and completion on sample dataset.
- Exit criteria:
  - Produces deterministic per-frame residue sets for same input.

### Phase 3: CSV Output
- Changes:
  - Add CSV serializer helpers (header + row writer + field quoting).
  - Output stream selection: stdout default, `-o` file when set.
- Validation:
  - Header exact: `frame,resnums,total_count`.
  - Empty result encoded as `""` and `0`.
- Performance measurement:
  - Ensure streaming output (no accumulation of all frame results in memory).
- Exit criteria:
  - CSV is well-formed and consumable by standard CSV readers.

### Phase 4: Tests, Docs, and Hardening
- Changes:
  - Add/extend tests using provided NetCDF fixtures in `tests/data/wcn`.
  - Update `README.md`, `codebase-analysis-docs/codebasemap.md`, and architecture/data-flow assets.
- Validation:
  - Build + relevant tests pass locally.
- Performance measurement:
  - Confirm runtime is bounded/acceptable for provided fixtures.
- Exit criteria:
  - Code and docs are in sync and committed as one change set.

## 5) Testing & Verification Plan (HPC-aware)
- Unit-level:
  - CSV quoting/row formatting helper behavior.
  - Atom-index to point materialization bounds checks.
- Integration:
  - `watpocket <topology> <trajectory.nc> --resnums ...` writes expected CSV shape.
  - `-o` writes file; absence of `-o` writes CSV to stdout.
  - Atom-count mismatch path emits deterministic error.
- Regression:
  - Existing single-structure mode outputs remain unchanged.
  - Existing `--draw` behavior still valid in single-input mode.
- Determinism:
  - Same inputs produce identical CSV rows/order.
- Sanitizers:
  - Existing project sanitizer matrix remains available; run default local checks in current environment.

## 6) Performance Engineering Plan
- Baseline:
  - Measure elapsed time for one representative `.nc` run with provided fixture and fixed selector list.
- Profiling methodology:
  - Coarse timing around frame loop (I/O + hull + classification) for first implementation pass.
- Hotspot hypotheses:
  - Per-frame hull build and per-water halfspace tests dominate runtime.
  - I/O of large NetCDF files may dominate wall time.
- Optimization ladder (deferred after correctness):
  - Cache capacity reservations for per-frame vectors.
  - Reduce per-frame string operations in water classification path.
  - Optional spatial prefilter for waters before full halfspace tests (future).
- Guardrails:
  - No optimization that changes residue inclusion semantics without explicit validation.

## 7) Risk Register + Mitigations
- Numerical stability (medium likelihood, medium impact):
  - Risk: frame-specific degeneracy from selected CA coordinates.
  - Mitigation: retain existing collinear/coplanar guards per frame.
  - Detection: frame-indexed error.
- Concurrency/non-determinism (low likelihood, low impact):
  - Risk: future parallelization altering order.
  - Mitigation: keep single-thread deterministic flow; sorted unique residue IDs.
- Performance/memory (medium likelihood, medium impact):
  - Risk: huge NetCDF fixtures causing long runtime.
  - Mitigation: streaming one frame at a time; no global accumulation.
- Portability/build (medium likelihood, low impact):
  - Risk: format-guess edge cases for NetCDF extension handling.
  - Mitigation: explicit `.nc` gate and clear unsupported-format errors.
- Dependency risk (low likelihood, medium impact):
  - Risk: Chemfiles topology attachment mismatch behavior.
  - Mitigation: explicit atom-count sanity checks and frame-level guards.

## 8) Rollout / Migration Plan
- Backwards compatibility:
  - Preserve single-input structure mode interface.
  - Keep trajectory mode to NetCDF only for this release with explicit errors for other formats.
- Incremental rollout:
  - Step 1: Merge NetCDF+CSV implementation.
  - Step 2: Extend to XTC/DCD using same topology-cache and CSV pipeline.
- Validation gates:
  - Build success, integration run on provided fixtures, documentation sync.
- Documentation updates:
  - CLI usage, trajectory constraints, and CSV schema updates in project docs.

## 9) Deliverables Checklist
- [ ] Code: trajectory NetCDF mode with per-frame hull/water analysis.
- [ ] Code: `-o/--output` CSV path for trajectory mode (stdout fallback).
- [ ] Tests: trajectory CSV output and mismatch/error paths.
- [ ] Docs: README + codebase map + architecture/data-flow diagrams.
- [ ] Notes: explicit statement that XTC/DCD are planned, not yet implemented.
