# 26-02-27 parm7-topology-support

## 1) Problem Framing
- Goal: add Amber `parm7/prmtop` support as the first argument in trajectory mode (`<topology> <trajectory.nc>`) while preserving existing PDB/CIF topology behavior.
- In-scope:
  - Parse `parm7` sections needed by `watpocket`: `POINTERS`, `ATOM_NAME`, `RESIDUE_LABEL`, `RESIDUE_POINTER`, and optional `RESIDUE_CHAINID`.
  - Use parsed topology to resolve selected CA atoms and water oxygen atoms for per-frame analysis.
  - Keep atom-count sanity checks against trajectory frames.
  - Preserve NetCDF-only trajectory gate for now.
- Out-of-scope:
  - Full force-field parameter parsing/use.
  - Single-input mode for `parm7` (no coordinates in topology-only input).
  - XTC/DCD integration (future).
- Assumptions:
  - `parm7` uses fixed `%FLAG`/`%FORMAT` sections as described in the provided reference.
  - Residue IDs for selector matching in `parm7` are residue-order indices (1..NRES).
  - `RESIDUE_CHAINID` may be absent; chain-qualified selectors must then error.

## 2) Success Criteria (Measurable)
- Correctness:
  - `watpocket <topology.parm7> <traj.nc> ...` runs and produces per-frame CSV output.
  - `parm7` atom count (`NATOM`) matches trajectory frame atom count; mismatch errors include both values.
  - Selector resolution for CA atoms behaves deterministically and errors on missing/duplicate CA.
  - Chain-qualified selectors are accepted only when `RESIDUE_CHAINID` exists.
- Performance:
  - `parm7` parsing is one-time per run; frame loop remains streaming.
- Resource:
  - Topology parser stores only required arrays, not full force-field data.
- Portability:
  - Works in current CMake/Linux toolchain setup using standard library only.
- Maintainability:
  - Parser and topology adapters remain in `src/watpocket/main.cpp` (current monolithic architecture) with clear helper boundaries.

## 3) Architecture Sketch
- New `parm7` topology adapter layer in `main.cpp`:
  - Section reader: `%FLAG` + `%FORMAT` capture.
  - Fixed-width decoder for `a4` and `i8`-style fields.
  - `Parm7Topology` model with:
    - `atom_names`
    - `residue_labels`
    - residue atom ranges from `RESIDUE_POINTER`
    - optional `residue_chain_ids`
- Existing frame analysis reused:
  - Cached CA atom indices and water oxygen refs.
  - Per-frame coordinates from trajectory frames.
  - Existing CGAL hull + halfspace point-in-hull logic unchanged.

## 4) Work Breakdown Structure (WBS)

### Phase 0: Plan + Contracts
- Changes:
  - Lock `parm7` scope to topology metadata only.
  - Confirm selector behavior with optional chain IDs.
- Validation:
  - Plan references to provided PDF and `rms` parser.
- Exit criteria:
  - Agreed parsing and selector contracts documented.

### Phase 1: `parm7` Parser (Minimal Required Sections)
- Changes:
  - Implement section scanner and `%FORMAT` parser.
  - Parse required sections and validate counts/indices.
  - Build residue atom ranges and atom-to-residue mapping.
- Validation:
  - Error on missing required sections.
  - Error on invalid pointers/count mismatches.
- Performance measurement:
  - One-time parse overhead only.
- Exit criteria:
  - `Parm7Topology` object built for valid files.

### Phase 2: Topology Adapter Integration
- Changes:
  - Add overloads for CA selection and water-oxygen reference extraction from `Parm7Topology`.
  - Integrate in trajectory-mode branch by topology extension detection.
  - Keep chemfiles topology path for PDB/CIF/MMCIF unchanged.
- Validation:
  - Chain selector error when `RESIDUE_CHAINID` missing.
  - Same CSV framing/output semantics retained.
- Exit criteria:
  - Both topology families (chemfiles structure, `parm7`) feed the same frame loop.

### Phase 3: Verification + Documentation
- Changes:
  - Validate build and runtime on provided `parm7` + NetCDF assets.
  - Update `README.md`, `codebasemap.md`, and architecture/data-flow/library-boundary diagrams.
- Validation:
  - Existing CLI smoke tests pass.
  - Positive `parm7` trajectory command produces CSV output.
- Exit criteria:
  - Code/docs in sync in same change set.

## 5) Testing & Verification Plan
- Unit-style checks (via helper validation paths):
  - `%FORMAT` parsing and required section presence.
  - Pointer-derived array sizes and residue ranges.
- Integration checks:
  - `parm7 + nc` successful run with CSV output.
  - `parm7 + chain selector without RESIDUE_CHAINID` -> explicit error.
  - Existing PDB/CIF topology + nc path still works.
- Regression checks:
  - Single-input structure mode unchanged.
  - Existing CLI tests (`help`, `version`) pass.

## 6) Performance Engineering Plan
- Baseline:
  - Measure startup parse time and total runtime for one representative trajectory.
- Hotspot expectation:
  - Runtime remains dominated by per-frame hull/classification, not topology parse.
- Guardrails:
  - No parser changes that alter residue/atom mapping semantics without explicit validation.

## 7) Risk Register + Mitigations
- Format variability risk (medium):
  - Mitigation: strict required-section checks with actionable errors.
- Pointer consistency risk (medium):
  - Mitigation: validate `NATOM/NRES` sizes and `RESIDUE_POINTER` monotonicity/bounds.
- Selector semantics risk (low-medium):
  - Mitigation: explicit chain-selector rules tied to `RESIDUE_CHAINID` availability.
- Regression risk in existing path (medium):
  - Mitigation: isolate `parm7` branch and keep chemfiles branch unchanged.

## 8) Rollout / Migration Plan
- Backward compatibility:
  - Preserve existing topology support and CLI contracts.
- Incremental rollout:
  - Add `parm7` only for 2-input mode first.
  - Expand to additional trajectory formats later using same topology adapter.
- Validation gates:
  - Build + runtime checks + docs sync.

## 9) Deliverables Checklist
- [x] `parm7` minimal parser for required topology metadata.
- [x] Trajectory-mode integration using parsed `parm7` topology.
- [x] Selector behavior updated for optional `RESIDUE_CHAINID`.
- [x] CSV output behavior unchanged for trajectory mode.
- [x] Documentation updates across README and codebase-analysis docs/assets.
