# 26-02-27 Watpocket Initial Feature Plan

## Problem Framing
- Goal: deliver a C++ CLI program `watpocket` that reads a structure frame (v1) and computes a convex hull from selected residue C-alpha atoms.
- Input model:
  - One positional argument: structure file (`.pdb`, `.cif`, `.mmcif`) parsed with Chemfiles.
  - Two positional arguments: topology + trajectory syntax accepted by CLI contract, but trajectory execution is explicitly not implemented in v1.
- Selection model:
  - `--resnums` required, comma-separated selectors.
  - Selector forms: `N` (unqualified residue number) or `CHAIN:N` (chain-qualified).
  - If structure contains more than one chain, all selectors must be chain-qualified.
- Geometry model:
  - For each selected residue, locate atom named `CA`.
  - Build 3D convex hull with CGAL `convex_hull_3`.
  - Reject degenerate selections: fewer than 4 points, collinear, or coplanar.
- Visualization model:
  - `-d/--draw <output.py>` emits a PyMOL script that auto-loads the input structure and draws hull edges as CGO `LINES` (no faces), with clear defaults for color and width.
- In-scope:
  - Single-frame structure path.
  - CLI, validation, hull generation, edge extraction, PyMOL script output.
  - Build integration with vendored `external/chemfiles` and `external/cgal`.
- Out-of-scope (v1):
  - Actual topology+trajectory processing.
  - Surface triangulation/mesh export files.
  - Advanced rendering styles (faces, transparency, animation).

## Assumptions
- Platform: Linux/macOS first; modern GCC/Clang; CMake/Ninja presets continue to work.
- CGAL source tree in `external/cgal` is present in repository.
- Chemfiles source tree in `external/chemfiles` is present in repository.
- Chemfiles exposes residue id and chain metadata for PDB/mmCIF through residue properties.
- Existing CPM flow remains for non-vendored dependencies (e.g., CLI11).

## Success Criteria
- Correctness:
  - `watpocket <input> --resnums ...` succeeds only when all selectors resolve exactly to residues with a `CA` atom.
  - Program exits with actionable errors for missing residues, missing `CA`, multi-chain ambiguity, and geometric degeneracy.
  - Hull edges are extracted from a valid 3D hull and are deterministic for fixed input.
- Performance:
  - Runtime acceptable for typical protein residue subsets (tens to low thousands of residues).
- Resource:
  - Memory bounded by one frame + hull mesh overhead.
- Portability:
  - Builds with template’s supported toolchains where chemfiles/cgal dependencies are available.
- Maintainability:
  - Logic decomposed into parser/selection/hull/draw helpers.
  - Clear user-facing error messages and usage help.

## Architecture Sketch
- Module responsibilities:
  - CLI parser: positional inputs, `--resnums`, `--draw`.
  - Selector parser: parse comma list into typed selectors.
  - Frame loader: read one Chemfiles frame from structure input.
  - Residue resolver: map selectors -> residue -> `CA` atom coordinates.
  - Hull engine: CGAL points -> `Surface_mesh` hull -> unique edge list.
  - Draw emitter: write PyMOL script (load + CGO lines + styling).
- Dataflow:
  - CLI args -> selectors -> frame/topology scan -> selected CA points -> CGAL hull -> edge segments -> optional script output.
- Concurrency model:
  - Single-threaded v1 for determinism and simplicity.
- Memory model:
  - In-memory frame + vectors of selectors, points, edges.
- Error handling/logging:
  - Exceptions converted to concise stderr diagnostics + non-zero exit.

## WBS
### Phase 0: Build Integration
- Add `watpocket` target and make it the only executable target in project.
- Wire vendored dependencies:
  - Chemfiles via `add_subdirectory(external/chemfiles)`.
  - CGAL via `find_package(CGAL CONFIG REQUIRED PATHS external/cgal NO_DEFAULT_PATH)`.
- Keep CPM-based dependency mechanism active for remaining deps.
- Exit criteria:
  - Configure/generate succeeds with vendored libs present.

### Phase 1: CLI + Input Contract
- Implement CLI with required `--resnums` and 1–2 positional inputs.
- Enforce v1 rule: two-positional mode returns “not implemented yet”.
- Enforce `--draw` requires structure input extension in `{.pdb,.cif,.mmcif}`.
- Exit criteria:
  - Invalid combinations fail with clear errors; valid single-input path proceeds.

### Phase 2: Chemfiles Frame + Residue Selection
- Read first frame with Chemfiles.
- Parse selectors (`N` or `CHAIN:N`).
- Detect chain cardinality in loaded structure.
- Enforce chain qualification policy.
- Resolve each selector uniquely and extract `CA` position.
- Exit criteria:
  - All selected residues produce coordinates or explicit diagnostics.

### Phase 3: CGAL Hull + Degeneracy Guards
- Validate selected-point set size (`>=4`).
- Detect collinear/coplanar and fail early.
- Compute hull using `CGAL::convex_hull_3` into `Surface_mesh`.
- Extract undirected unique hull edges.
- Exit criteria:
  - Non-degenerate input yields non-empty edge list.

### Phase 4: PyMOL Draw Output
- Implement `--draw` script emission:
  - `load <input>`
  - CGO object with `LINEWIDTH`, `BEGIN LINES`, color and vertices.
  - `load_cgo(...)` and visibility settings.
- Exit criteria:
  - Script runs in PyMOL and visualizes hull as line network.

### Phase 5: Validation + Hardening
- Build and run binary with representative sample input.
- Add focused tests for selector parsing and error paths where practical.
- Update README with usage examples and constraints.
- Exit criteria:
  - Reproducible build and documented first feature.

## Testing & Verification Plan
- Unit-level checks (where practical in current template):
  - Selector parsing: valid/invalid token forms.
  - Extension validation for draw mode.
- Integration checks:
  - Single-frame PDB and CIF with valid `--resnums`.
  - Multi-chain with unqualified selector -> error.
  - Missing residue / missing `CA` -> error.
  - Degenerate geometric sets -> error.
- Determinism checks:
  - Same input and selector order produce same edge script ordering policy (sorted undirected edges).

## Performance Engineering Plan
- Baseline:
  - Measure elapsed time for frame read + selector resolution + hull generation separately.
- Hotspot hypotheses:
  - Residue lookup by repeated scans may dominate for large selections.
- Optimization ladder (deferred):
  - Build index map from (chain,resid) to residue index.
  - Avoid redundant string allocations in selector parsing.
  - Keep CGAL kernel choice stable for robustness/perf tradeoff.

## Risk Register
- Metadata risk (chain property naming differences across formats)
  - Likelihood: medium; Impact: high.
  - Mitigation: probe known chain property keys and fail with diagnostic listing available properties.
- Geometry degeneracy behavior
  - Likelihood: medium; Impact: medium.
  - Mitigation: explicit collinear/coplanar checks before hull.
- Build risk from vendored dependency integration
  - Likelihood: medium; Impact: high.
  - Mitigation: isolate dependency wiring in CMake and keep CPM path untouched for other libs.
- User input ambiguity for residues
  - Likelihood: high; Impact: medium.
  - Mitigation: strict parser and clear error messages with examples.

## Rollout / Migration
- Replace template executable target with `watpocket` as canonical binary.
- Keep libraries/tests that do not create extra shipped binaries unless requested later.
- Documentation updated with exact CLI contract and v1 limitations.

## Deliverables Checklist
- [ ] `watpocket` executable target in CMake
- [ ] Vendored chemfiles/cgal integration in build
- [ ] CLI parsing and selector parsing
- [ ] Frame loading and CA extraction
- [ ] CGAL hull + edge extraction
- [ ] PyMOL script writer (`--draw`)
- [ ] Error handling for all specified failure modes
- [ ] README usage section update
