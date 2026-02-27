# 26-02-27 Watpocket Water Pocket Selection Plan

## Problem Framing
- Goal: extend `watpocket` so its primary behavior is to identify water oxygen atoms that lie inside the convex hull formed by selected residue C-alpha atoms.
- Existing context:
  - Single-structure mode (`<structure>`) is implemented.
  - Two-positional mode (`<topology> <trajectory>`) is recognized but remains not implemented.
- New behavior:
  - Always compute hull from selected residues.
  - Detect inside-hull waters and report residue numbers.
  - Print PyMOL selection string for matching waters: `select watpocket, resi x+y+z`.
  - If `-d/--draw` is requested, also write a PyMOL script with hull drawing and solvent visibility filtering to only `watpocket` waters.

## Scope
- In-scope:
  - Water oxygen detection from structure frame using Chemfiles.
  - Point-in-convex-hull classification with boundary counted as inside.
  - User output list + selection string.
  - Draw script update (`hide everything, solvent`; `show spheres, watpocket`; `sphere_scale 0.4`).
- Out-of-scope:
  - Implementing trajectory processing for two-positional input.
  - New output file formats beyond console and PyMOL script.

## Assumptions
- Water definition:
  - Residue names: `{HOH, WAT, TIP3, TIP3P, SPC, SPCE}`.
  - Oxygen atom names: `{O, OW}`.
- Water chain IDs are generally absent/irrelevant for reporting.
- `watpocket` selection name is fixed and not configurable.

## Success Criteria
- Correctness:
  - Program reports waters inside or on boundary of hull.
  - Output includes:
    - line listing residue numbers;
    - line with `select watpocket, resi ...`.
  - If none found, program prints explicit message indicating no waters inside hull.
- Draw mode:
  - Script still draws hull edges.
  - Script creates `watpocket` selection, hides all solvent, shows only `watpocket` as spheres, sets `sphere_scale=0.4`.
- Compatibility:
  - Two-positional mode still exits with not-implemented diagnostic.

## Architecture Sketch
- Data pipeline:
  1. Parse CLI and load one Chemfiles frame.
  2. Resolve selected residues -> C-alpha points.
  3. Build convex hull mesh + edge list.
  4. Build facet halfspace predicates from hull.
  5. Enumerate water oxygen candidates and classify points against hull halfspaces.
  6. Collect unique water residue IDs and emit console + optional script output.
- Core helper additions:
  - `collect_waters(...)` for candidate extraction.
  - `build_hull_halfspaces(...)` and `point_inside_or_on_hull(...)` for inclusion checks.
  - `make_resi_selection(...)` for PyMOL selection string.

## WBS
### Phase 0: Plan + Contract Lock
- Encode agreed water naming and output contract.
- Keep topology+trajectory as explicit not implemented.

### Phase 1: Geometric Classification Helpers
- Build oriented facet planes from hull triangles.
- Use interior reference point to orient halfspaces.
- Implement inside-or-boundary classification.

### Phase 2: Water Candidate Extraction
- Iterate residues in topology.
- Filter by water residue names, then oxygen atom names.
- Gather `(resid, coordinate)` records; skip entries without residue ID.

### Phase 3: Main Flow Integration
- Run classification after hull generation.
- Create sorted unique list of water residue IDs.
- Print human-readable results and PyMOL selection line.

### Phase 4: Draw Script Extension
- Keep hull CGO drawing.
- Add `watpocket` selection in script.
- Hide all solvent and show spheres for `watpocket`; set `sphere_scale` to 0.4.

### Phase 5: Verification
- Build target and verify on provided PDB example.
- Validate no-water case message and selection behavior.

## Testing & Verification Plan
- Functional checks:
  - Mixed inside/outside waters produce non-empty list and valid `select watpocket, resi ...`.
  - No inside waters prints explicit message.
  - Boundary water points are included.
- Draw checks:
  - Script loads structure, draws hull, creates `watpocket` selection, and displays only selected solvent spheres.
- Regression checks:
  - Existing residue selector validation and degeneracy checks remain intact.

## Risks & Mitigations
- Hull face orientation ambiguity
  - Mitigation: orient each face via interior reference point before side tests.
- Residue naming variation in water models
  - Mitigation: centralize allowed sets and keep explicit error/report behavior.
- Ambiguous residue IDs across chains
  - Mitigation: user-approved reporting by residue number only; keep solvent filter in script.

## Deliverables Checklist
- [ ] `watpocket` default path reports waters inside hull.
- [ ] Console output includes `select watpocket, resi ...` (or no-water message).
- [ ] `--draw` script includes hull drawing + solvent filtering + `watpocket` spheres.
- [ ] Two-positional mode remains not implemented with clear error.
