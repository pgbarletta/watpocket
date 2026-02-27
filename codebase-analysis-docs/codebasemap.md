# Codebase Map: watpocket
## 0. Quick Start for Contributors
### Add a new analysis: starting points + checklist
- Primary implementation file is `src/watpocket/main.cpp`; current pipeline is monolithic in this translation unit, so new analyses should usually be added as new helper functions near `compute_hull_data()` and `find_waters_inside_hull()` and then called from `main()` (source: `src/watpocket/main.cpp:compute_hull_data`, `src/watpocket/main.cpp:find_waters_inside_hull`, `src/watpocket/main.cpp:main`).
- Checklist:
  - Define CLI interface in `main()` using CLI11 (`app.add_option` / `app.add_flag`) (source: `src/watpocket/main.cpp:main`).
  - Keep selector parsing and residue resolution consistent with existing helpers (`parse_residue_selectors`, `resolve_ca_points`) (source: `src/watpocket/main.cpp:parse_residue_selectors`, `src/watpocket/main.cpp:resolve_ca_points`).
  - Decide whether the feature is structure-only or trajectory-capable; trajectory mode currently throws by design (source: `src/watpocket/main.cpp:main`).
  - Define output path(s): stdout, PyMOL script, or new artifact.
  - Add/extend tests (currently only CLI smoke and sample-library unit tests exist; no watpocket functional regression tests) (source: `test/CMakeLists.txt`).

### Add/modify protein/ligand selection semantics
- Current program does not implement explicit protein-vs-ligand classification; it operates on residue selectors and water residue heuristics (source: `src/watpocket/main.cpp:parse_residue_selectors`, `src/watpocket/main.cpp:collect_water_oxygens`).
- Selection semantics are custom CSV tokens (`12` or `A:12`) rather than `chemfiles::Selection` language (source: `src/watpocket/main.cpp:parse_selector_token`).
- Multi-chain inputs enforce chain-qualified selectors (source: `src/watpocket/main.cpp:resolve_ca_points`).

### Debug PBC / units / alignment issues
- No PBC unwrap/rewrap/alignment is currently implemented in watpocket; coordinates are consumed as read from Chemfiles first frame (source: `src/watpocket/main.cpp:main`, `src/watpocket/main.cpp:resolve_ca_points`).
- `chemfiles::UnitCell` and frame step/time are not read or used, so unit/PBC issues are currently latent integration gaps, not active transforms (source: `src/watpocket/main.cpp`, absence of `UnitCell`/time/step usage).

### Debug CGAL robustness issues
- Kernel is `CGAL::Exact_predicates_inexact_constructions_kernel` (EPIK), giving exact orientation predicates with inexact constructed coordinates (source: `src/watpocket/main.cpp:Kernel`).
- Robustness gates before hull build: `<4 points`, `all collinear`, `all coplanar` are hard errors (source: `src/watpocket/main.cpp:compute_hull_data`, `src/watpocket/main.cpp:are_all_collinear`, `src/watpocket/main.cpp:are_all_coplanar`).
- Point classification treats boundary points as inside by rejecting only `ON_POSITIVE_SIDE` against inward-oriented face planes (source: `src/watpocket/main.cpp:point_inside_or_on_hull`, `src/watpocket/main.cpp:compute_hull_data`).

### Improve performance (I/O vs geometry) starting points
- I/O today reads exactly one frame (`trajectory.read()`), so runtime is dominated by one parse pass and geometry/classification loops (source: `src/watpocket/main.cpp:main`, `src/watpocket/main.cpp:find_waters_inside_hull`).
- Build-time cost is dominated by dependency targets and test executables when building whole tree; `comp.sh` already constrains build to target `watpocket` and disables heavy tooling/sanitizers (source: `comp.sh`, `Dependencies.cmake`, `test/CMakeLists.txt`, `ProjectOptions.cmake`).

### Refactor safely: invariants and guardrails
- Keep single binary deliverable `watpocket` intact (`add_executable(watpocket ...)`, packaged target is `watpocket`) (source: `src/watpocket/CMakeLists.txt`, `CMakeLists.txt:myproject_package_project`).
- Preserve vendored dependency model for CGAL and Chemfiles (`external/cgal`, `external/chemfiles`) while leaving CPM enabled for other packages (source: `Dependencies.cmake`).
- Do not silently relax selector invariants: unique residue match, exactly one CA per selected residue, and multi-chain qualification rules (source: `src/watpocket/main.cpp:resolve_ca_points`, `src/watpocket/main.cpp:find_ca_atom_index`).

## 1. Repository Overview
### Directory tree (high-level)
```text
.
├── CMakeLists.txt
├── Dependencies.cmake
├── ProjectOptions.cmake
├── cmake/                     # shared CMake modules (sanitizers, warnings, linker, CPM bootstrap)
├── configured_files/          # template for generated config header
├── include/myproject/         # sample library public header (template scaffold)
├── src/
│   ├── sample_library/        # template library
│   ├── watpocket/             # production CLI binary
│   └── ftxui_sample/          # template sample app (not wired into build)
├── test/                      # template tests + CLI smoke tests
├── tests/data/wcn/            # watpocket sample data + script examples
├── external/
│   ├── chemfiles/             # vendored dependency
│   └── cgal/                  # vendored dependency
└── .github/workflows/         # CI, CodeQL, formatting, wasm template flow
```

### Build systems and entrypoints
- Build system is CMake; top-level adds `configured_files`, `src`, and optionally `test`/`fuzz_test` (source: `CMakeLists.txt`).
- Runtime entrypoint for project deliverable is `src/watpocket/main.cpp` (`main`) (source: `src/watpocket/main.cpp:main`).
- `src/CMakeLists.txt` currently builds `sample_library` and `watpocket`; `ftxui_sample` exists on disk but is not added (source: `src/CMakeLists.txt`, `src/ftxui_sample/CMakeLists.txt`).

### How chemfiles and CGAL are discovered/linked
- CGAL: `find_package(CGAL CONFIG REQUIRED PATHS external/cgal NO_DEFAULT_PATH)` (source: `Dependencies.cmake`).
- Chemfiles: vendored subdirectory `add_subdirectory(external/chemfiles ...)` with tests/docs disabled (source: `Dependencies.cmake`).
- CPM remains enabled and currently fetches Catch2 + CLI11 if absent (source: `Dependencies.cmake`, `cmake/CPM.cmake`).
- CPM bootstrap always performs a `file(DOWNLOAD ...)` for `CPM.cmake` unless provided via cache location/environment path (source: `cmake/CPM.cmake`).

### Configuration surfaces (CLI flags, config files, env vars)
- CLI (`watpocket`):
  - positional `inputs` with arity 1..2
  - `--resnums` required
  - `-d,--draw` optional output script path
  - `--version` (source: `src/watpocket/main.cpp:main`).
- CMake options: sanitizers, static analyzers, ccache, coverage, IPO, hardening via `ProjectOptions.cmake` (source: `ProjectOptions.cmake`).
- Env/config affecting dependency download/cache:
  - `CPM_SOURCE_CACHE` env/CMake variable changes CPM download location (source: `cmake/CPM.cmake`).
- Presets exist for multi-platform configure/test flows (source: `CMakePresets.json`).

## 2. Architecture at a Glance
### Subsystem diagram
- Asset file: `codebase-analysis-docs/assets/architecture.mmd`
- Data-flow asset: `codebase-analysis-docs/assets/data-flow.mmd`
- Library-boundary asset: `codebase-analysis-docs/assets/library-boundary.mmd`

```mermaid
flowchart LR
  CLI[CLI / Argument Parsing\n`main()` + CLI11] --> IO[Chemfiles Input\n`chemfiles::Trajectory::read()`]
  IO --> SEL[Residue Resolution\n`parse_residue_selectors()` + `resolve_ca_points()`]
  SEL --> GEO[Hull Geometry\n`compute_hull_data()`]
  GEO --> ANA[Water Analysis\n`find_waters_inside_hull()`]
  ANA --> OUT[Console Output\nresidue ids + PyMOL selection]
  GEO --> DRAW[PyMOL Script Writer\n`write_pymol_script()`]
  ANA --> DRAW
  DRAW --> FILE[`.py` script artifact]
```

### Data-flow diagram (frame lifecycle)
```mermaid
flowchart TD
  A[Input path(s)] --> B{1 or 2 positional args?}
  B -->|1| C[Open structure via Chemfiles\n`Trajectory(path,'r')`]
  B -->|2| X[Trajectory mode branch\nthrows \"not implemented yet\"]
  C --> D[Read first frame\n`trajectory.read()`]
  D --> E[Build residue lookup\nby chain/id and id]
  E --> F[Resolve CA coordinates\nselected residues]
  F --> G[Degeneracy checks\n<4 / collinear / coplanar]
  G --> H[CGAL convex hull\n`convex_hull_3` -> `Surface_mesh`]
  H --> I[Derive hull edges + inward halfspaces]
  D --> J[Collect water oxygen atoms\nresname+atom filters]
  I --> K[Point-in-hull tests\n`Plane_3::oriented_side`]
  J --> K
  K --> L[Sorted unique water residue ids]
  L --> M[Print residues + `select watpocket, resi ...`]
  L --> N{`-d/--draw` set?}
  N -->|yes| O[Write PyMOL script\nload input, hull lines, watpocket spheres]
```

### End-to-end data flow narrative
- Inputs are parsed with CLI11 in `main()`.
- For 2 positional files, execution aborts with a not-implemented error; for 1 file, Chemfiles reads one frame.
- Residue selectors are parsed and mapped to residue objects using topology-derived lookup tables; each selected residue must map to exactly one residue and exactly one `CA` atom.
- CA coordinates become `CGAL::Point_3` input to `CGAL::convex_hull_3`, producing a `CGAL::Surface_mesh`.
- Hull mesh edges are extracted for rendering; hull face planes are oriented inward and stored as halfspaces.
- Water oxygen atoms are identified from residue/atom name heuristics and tested against halfspaces.
- Outputs include residue-id list + PyMOL selection string; optional draw mode emits a PyMOL script with hull lines and water visualization commands. (source: `src/watpocket/main.cpp:main`, `parse_residue_selectors`, `resolve_ca_points`, `compute_hull_data`, `collect_water_oxygens`, `find_waters_inside_hull`, `write_pymol_script`)

### Representative workflows (module-level call traces)
1. Single-structure, analysis-only (no `-d`)
   - Example form: `watpocket protein.pdb --resnums A:12,A:15,A:18,A:26` (source pattern: `README.md`).
   - Call spine:
```text
main
  -> parse_residue_selectors
  -> chemfiles::Trajectory(...).read()
  -> resolve_ca_points
  -> compute_hull_data
  -> find_waters_inside_hull
  -> stdout summary + select line
```
2. Single-structure, draw mode
   - Example script path shown in repo workflow data (`tests/data/wcn/run.sh` calling `-d sal.py`) (source: `tests/data/wcn/run.sh`).
   - Additional branch:
```text
...same as above...
  -> write_pymol_script
  -> output .py script with hull CGO + watpocket solvent display commands
```
3. Topology + trajectory invocation (currently blocked)
   - CLI accepts two positional inputs by signature, but branch throws `trajectory mode ... is not implemented yet` before any frame loop (source: `src/watpocket/main.cpp:main`).
   - This is the future extension point for per-frame analysis.

### Cross-cutting concerns
- Correctness (units/PBC): no coordinate transforms or unit conversion implemented; correctness assumes input coordinates are directly suitable for hull geometry (source: `src/watpocket/main.cpp`).
- Geometry robustness: explicit degeneracy guards + exact-predicate kernel + face-orientation correction (source: `src/watpocket/main.cpp:are_all_collinear`, `are_all_coplanar`, `compute_hull_data`).
- Determinism: uses deterministic containers/sorting for output IDs and deduplicated edges (`std::set`, `std::sort`, `std::unique`) (source: `src/watpocket/main.cpp:compute_hull_data`, `find_waters_inside_hull`).
- Performance: single-threaded loops; no frame streaming yet (source: `src/watpocket/main.cpp`).
- Error reporting: exceptions converted to `stderr` error lines with process exit code 1 (source: `src/watpocket/main.cpp:main`).

## 3. Domain Data Model
### Chemical view
- `ResidueSelector` encodes requested residue as optional chain + residue id from `--resnums` CSV tokens (source: `src/watpocket/main.cpp:ResidueSelector`, `parse_selector_token`).
- `ResidueLookup` indexes topology residues by `(chain,id)` and by `id` only, and tracks set of observed chains (source: `src/watpocket/main.cpp:ResidueLookup`, `build_residue_lookup`).
- Water molecule proxy model is oxygen atom hits belonging to residues whose resname is in `{HOH,WAT,TIP3,TIP3P,SPC,SPCE}` and atom name in `{O,OW}` (source: `src/watpocket/main.cpp:collect_water_oxygens`).
- Ligand-specific identity rules are not present in current implementation.

### Trajectory view
- CLI allows either one structure input or two files (`<topology> <trajectory>`), but only one-file path is implemented (source: `src/watpocket/main.cpp:main`).
- Runtime processing model is single-frame: `trajectory.read()` once, then all analysis on that frame (source: `src/watpocket/main.cpp:main`).
- Frame/time metadata are currently ignored.

### Geometry view
- Kernel: `CGAL::Exact_predicates_inexact_constructions_kernel` (source: `src/watpocket/main.cpp:Kernel`).
- Primitives:
  - `Point3` for atom coordinates
  - `Mesh` (`CGAL::Surface_mesh<Point3>`) for hull
  - `Plane3` for inward halfspaces (source: `src/watpocket/main.cpp:Point3`, `Mesh`, `Plane3`, `compute_hull_data`).
- Mapping:
  - residue selector -> unique residue -> CA atom index -> `Point3`
  - classified water oxygen points -> residue IDs -> CLI/PyMOL outputs (source: `src/watpocket/main.cpp:resolve_ca_points`, `collect_water_oxygens`, `find_waters_inside_hull`).

### Metadata/properties
- Chain ID extraction uses residue string properties `chainid` first, then `chainname`, else empty string (source: `src/watpocket/main.cpp:residue_chain_id`).
- Drawability filter uses filename extensions `.pdb`, `.cif`, `.mmcif` (source: `src/watpocket/main.cpp:is_drawable_structure_path`).

## 4. Input, Selection, and Preprocessing
### How selection is defined
- Selection parser is custom token logic, not `chemfiles::Selection`:
  - split CSV by comma
  - parse each token as `RESID` or `CHAIN:RESID`
  - reject malformed values (source: `src/watpocket/main.cpp:parse_residue_selectors`, `parse_selector_token`, `parse_int64`).
- Matching behavior:
  - multichain topology requires explicit chain-qualified selectors
  - missing residue or non-unique residue match is an error
  - each selected residue must contain exactly one `CA` atom (source: `src/watpocket/main.cpp:resolve_ca_points`, `find_ca_atom_index`).

### Coordinate preprocessing (PBC/alignment/units)
- No wrap/unwrap/reimage/alignment/centering stage currently exists; positions are consumed as-is from `frame.positions()` (source: `src/watpocket/main.cpp:resolve_ca_points`, `collect_water_oxygens`).
- No explicit unit conversion is applied in code.

### Invariants after preprocessing
- Selector list is non-empty and syntactically valid (source: `src/watpocket/main.cpp:parse_residue_selectors`).
- Selected points correspond to exactly one CA atom per requested residue (source: `src/watpocket/main.cpp:resolve_ca_points`, `find_ca_atom_index`).
- Before geometry build: at least four points, non-collinear, non-coplanar (source: `src/watpocket/main.cpp:compute_hull_data`).

## 5. Geometry System (CGAL)
### Library boundary diagram (chemfiles ↔ adapters ↔ CGAL)
```mermaid
flowchart LR
  subgraph CHEM[chemfiles]
    T[Trajectory]
    F[Frame]
    Top[Topology + Residue]
    Pos[positions()]
    Prop[Residue properties\n`chainid`/`chainname`]
  end

  subgraph APP[watpocket adapters (`src/watpocket/main.cpp`)]
    Parse[Selector parsing\n`parse_residue_selectors()`]
    Lookup[Residue maps\n`build_residue_lookup()`]
    CA[CA extraction\n`find_ca_atom_index()`]
    Water[Water oxygen extraction\n`collect_water_oxygens()`]
    Classify[Hull membership\n`point_inside_or_on_hull()`]
  end

  subgraph CGAL[CGAL]
    Ker[Kernel\nEPIK]
    CH[convex_hull_3]
    Mesh[Surface_mesh]
    Plane[Plane_3 halfspaces]
  end

  T --> F --> Top
  F --> Pos
  Top --> Prop

  Top --> Lookup
  Pos --> CA
  Parse --> CA
  Pos --> Water
  Water --> Classify

  CA --> Ker
  Ker --> CH --> Mesh --> Plane --> Classify
```

### Which CGAL components are used and why
- `CGAL::convex_hull_3`: computes 3D convex hull from selected CA points (source: `src/watpocket/main.cpp:compute_hull_data`).
- `CGAL::Surface_mesh`: stores hull topology/geometry for extracting edges and faces (source: `src/watpocket/main.cpp:Mesh`, `compute_hull_data`).
- `CGAL::collinear` / `CGAL::coplanar`: preflight degeneracy checks (source: `src/watpocket/main.cpp:are_all_collinear`, `are_all_coplanar`).
- `Plane_3::oriented_side`: point-in-convex-polytope test via halfspace intersection (source: `src/watpocket/main.cpp:compute_hull_data`, `point_inside_or_on_hull`).

### Kernel configuration and robustness strategy
- Kernel choice is EPIK (exact predicates, inexact constructions), balancing robustness and speed for orientation/side tests (source: `src/watpocket/main.cpp:Kernel`).
- Degeneracy handling is explicit and fail-fast with clear errors before hull call (source: `src/watpocket/main.cpp:compute_hull_data`).
- Face orientation normalization:
  - interior reference point is hull-vertex centroid
  - if a face plane points inward (positive side for interior), point order is flipped to orient outward for consistent inside test (source: `src/watpocket/main.cpp:compute_hull_data`).

### Core geometric structures
- `HullData.edges`: sorted/unique list of segment endpoints (`std::array<double,6>`) for draw output (source: `src/watpocket/main.cpp:HullData`, `compute_hull_data`).
- `HullData.halfspaces`: vector of oriented face planes used for classification (source: `src/watpocket/main.cpp:HullData`, `compute_hull_data`).
- No BVH/AABB/KD-tree/triangulation acceleration structures are currently used.

### Mapping between geometric entities and atoms/residues
- CA mapping is direct residue-to-atom index traversal over Chemfiles `Residue` atom indices (source: `src/watpocket/main.cpp:find_ca_atom_index`, `resolve_ca_points`).
- Water mapping returns residue IDs, not atom indices; duplicates are removed via `std::set` (source: `src/watpocket/main.cpp:find_waters_inside_hull`).

### Common pitfalls + debugging notes
- User must supply chain-qualified selectors when topology has multiple chains, else hard error (source: `src/watpocket/main.cpp:resolve_ca_points`).
- `-d/--draw` requires drawable structure extension; otherwise error (source: `src/watpocket/main.cpp:main`, `is_drawable_structure_path`).
- Boundary points count as inside by design (source: `src/watpocket/main.cpp:point_inside_or_on_hull`).
- Selection parser is strict integer parsing and fails on malformed tokens/spaces around empty fields (source: `src/watpocket/main.cpp:parse_int64`, `parse_selector_token`).

## 6. Analysis Kernels
### Analysis A: Convex hull from selected CA atoms
- Purpose and outputs:
  - Build 3D hull from user-selected residues and extract line segments + face halfspaces.
  - Outputs: `HullData.edges`, `HullData.halfspaces` (source: `src/watpocket/main.cpp:compute_hull_data`).
- Key files + symbols:
  - `src/watpocket/main.cpp`: `resolve_ca_points`, `are_all_collinear`, `are_all_coplanar`, `compute_hull_data`.
- Inputs required:
  - Structure frame with topology, valid residue IDs, and one `CA` per selected residue.
- Correctness notes:
  - Fails for `<4` or degenerate geometry; uses EPIK kernel and oriented-plane normalization.
- Performance notes:
  - `resolve_ca_points` scales with selected residues and lookup map operations.
  - `compute_hull_data` cost dominated by `convex_hull_3` + mesh traversals.
- Extension points:
  - Replace or augment hull with alpha-shape/AABB-based analyses while reusing CA extraction.

### Analysis B: Water oxygen inside-hull classification
- Purpose and outputs:
  - Identify water residues with oxygen atoms inside or on hull boundary; return sorted residue IDs.
  - Outputs printed to stdout and used in PyMOL selection (source: `src/watpocket/main.cpp:collect_water_oxygens`, `find_waters_inside_hull`, `main`).
- Key files + symbols:
  - `src/watpocket/main.cpp`: `collect_water_oxygens`, `point_inside_or_on_hull`, `find_waters_inside_hull`, `join_residue_ids`, `make_pymol_resi_selector`.
- Inputs required:
  - Single structure frame currently; trajectory path not yet implemented.
- Correctness notes:
  - Water definition is heuristic on residue + atom names.
  - Multiple oxygens in one residue are collapsed to one residue ID by `std::set`.
- Performance notes:
  - Full scan over topology residues and candidate atoms; each candidate tested against all hull planes.
- Extension points:
  - Chain-aware water output, alternative solvent definitions, per-frame accumulation for trajectories.

### Analysis C: PyMOL script generation (visual artifact)
- Purpose and outputs:
  - Emit executable PyMOL Python script loading structure, drawing hull as CGO lines, and highlighting `watpocket` waters.
  - Output file path comes from `-d/--draw` (source: `src/watpocket/main.cpp:write_pymol_script`, `main`).
- Key files + symbols:
  - `src/watpocket/main.cpp`: `write_pymol_script`, `python_quoted`, `make_pymol_resi_selector`.
- Inputs required:
  - Single input file with extension `.pdb`/`.cif`/`.mmcif` + computed hull + classified water IDs.
- Correctness notes:
  - Script creates fixed selection name `watpocket` and hides all solvent except selected waters.
- Performance notes:
  - Linear in number of hull edges written; negligible relative to IO/hull for typical sizes.
- Extension points:
  - Add optional faces/surfaces, per-frame snapshots, or additional named selections.

## 7. Parallel & Performance Model
### Parallelization strategy
- Runtime analysis code is single-threaded; there is no OpenMP/TBB/std::execution/MPI in watpocket core (source: `src/watpocket/main.cpp`).
- Build system supports parallel compilation via CMake generator/build tool, but that is build-time only.

### Determinism expectations and reduction policies
- Deterministic output ordering is enforced for water residue IDs (`std::set`) and hull edges (`std::sort` + `std::unique`) (source: `src/watpocket/main.cpp:compute_hull_data`, `find_waters_inside_hull`).
- Floating-point values in script are formatted with fixed precision 4 decimals (source: `src/watpocket/main.cpp:write_pymol_script`).

### Memory/layout strategy and caching
- Transient vectors/maps are allocated per invocation (`ResidueLookup`, point vectors, edge vectors, halfspaces); no persistent cache across runs (source: `src/watpocket/main.cpp`).
- Chemfiles frame is read once and held in memory for all analysis.

### I/O vs compute overlap
- No overlap or streaming pipeline exists; steps are strictly sequential in `main()`.

### Benchmark harness and profiling hooks
- No dedicated watpocket benchmark targets found.
- Template fuzz and sample-library tests exist but are not specific to watpocket analysis behavior (source: `fuzz_test/CMakeLists.txt`, `test/tests.cpp`, `test/constexpr_tests.cpp`).

## 8. Build, Test, Bench, and Tooling
### How to build and run
- Standard build:
  - `cmake -S . -B build`
  - `cmake --build build --target watpocket --parallel <N>`
- Project helper script:
  - `comp.sh` configures Debug with sanitizers/analyzers/cache disabled, then builds only `watpocket` target (source: `comp.sh`).
- Top-level requires CMake 3.28 (`CMakeLists.txt`) while presets advertise minimum 3.21 (`CMakePresets.json`).

### Test strategy
- CLI smoke tests:
  - `watpocket --help`
  - `watpocket --version` regex check (source: `test/CMakeLists.txt`).
- Unit tests currently target template `sample_library` factorial logic only (source: `test/tests.cpp`, `test/constexpr_tests.cpp`).
- No repository-native automated test currently validates hull/water logic on `tests/data/wcn` sample input; those are manual workflow artifacts (`run.sh`, `sal.py`).

### CI notes and platform caveats
- GitHub Actions matrix includes Linux/macOS/Windows with clang/gcc/msvc and Debug/Release variants (source: `.github/workflows/ci.yml`).
- CodeQL workflow builds C++ target on Ubuntu (source: `.github/workflows/codeql-analysis.yml`).
- Sanitizers and static analyzers are enabled by default in top-level non-maintainer builds unless explicitly disabled (source: `ProjectOptions.cmake`, `cmake/Sanitizers.cmake`).
- CPM bootstrap performs network download unless cached/offline-provided (source: `cmake/CPM.cmake`).

### Debugging/profiling cookbook grounded in repo tooling
- To minimize build latency for watpocket-only iteration:
  - Disable sanitizers/analyzers/cache similar to `comp.sh`.
  - Build target `watpocket` only.
- To validate packaging/version metadata path:
  - Run `watpocket --version` (value comes from generated configured header) (source: `configured_files/config.hpp.in`, `src/watpocket/main.cpp:main`).
- For geometry correctness debugging:
  - Use `-d/--draw` to emit script and inspect hull edges + selected waters in PyMOL (source: `src/watpocket/main.cpp:write_pymol_script`).

## 9. Change Playbooks (Practical Guides)
### Add a new analysis over trajectories
1. Implement 2-input branch in `main()` by replacing current throw with topology+trajectory frame loop (`chemfiles::Trajectory` for trajectory, `set_topology`/topology handling as needed).
2. Keep existing selector parsing and CA extraction reusable for each frame.
3. Decide output strategy (per-frame IDs, aggregated IDs, or time series file).
4. Add regression tests with known trajectory fixtures and expected outputs.
5. Preserve single-structure behavior and error contracts.

### Add a new CGAL-based geometry computation
1. Add a geometry helper next to `compute_hull_data()` in `src/watpocket/main.cpp` (or split into new `src/watpocket/*.cpp` module if refactoring).
2. Reuse `Point3` conversion from Chemfiles positions.
3. Explicitly define kernel and degeneracy behavior.
4. Add deterministic serialization for outputs (sorted stable ordering where applicable).
5. Wire optional visualization into PyMOL script branch if relevant.

### Add a new preprocessing step (unwrap/alignment)
1. Introduce explicit preprocessing function after frame read and before CA/water extraction.
2. Decide units and reference frame policy; if using UnitCell, document boundary conventions.
3. Ensure both CA points and water points receive the same transform.
4. Add checks for transformed coordinate validity and deterministic behavior.

### Extend selection language/ligand identification
1. Extend parser (`parse_selector_token`) or add new CLI option for richer selection semantics.
2. Update lookup structures (`ResidueLookup`) if new qualifiers are needed (segment/insertion code, etc.).
3. If introducing ligand logic, define residue/atom/property-based rules with clear precedence.
4. Keep multi-chain ambiguity checks explicit.

### Add a new output artifact (CSV/JSON/structure/trajectory)
1. Add output flag(s) in `main()`.
2. Create a pure serializer helper that accepts computed IDs/metrics.
3. Keep stdout summary backward-compatible where possible.
4. Add artifact validation tests (golden files).

## 10. Index: Symbols → Files
- `main` → `src/watpocket/main.cpp`: CLI orchestration and execution spine.
- `parse_selector_token` / `parse_residue_selectors` → `src/watpocket/main.cpp`: custom residue selector parser.
- `residue_chain_id` / `build_residue_lookup` → `src/watpocket/main.cpp`: topology indexing by chain/id.
- `find_ca_atom_index` / `resolve_ca_points` → `src/watpocket/main.cpp`: CA extraction and selector resolution.
- `are_all_collinear` / `are_all_coplanar` → `src/watpocket/main.cpp`: degeneracy checks.
- `compute_hull_data` → `src/watpocket/main.cpp`: convex hull, edge extraction, halfspace build.
- `point_inside_or_on_hull` → `src/watpocket/main.cpp`: halfspace inclusion test.
- `collect_water_oxygens` / `find_waters_inside_hull` → `src/watpocket/main.cpp`: solvent filtering and pocket classification.
- `write_pymol_script` → `src/watpocket/main.cpp`: PyMOL script emitter.
- `myproject_setup_dependencies` → `Dependencies.cmake`: dependency wiring (CPM + vendored Chemfiles/CGAL).
- `myproject_setup_options` / `myproject_local_options` → `ProjectOptions.cmake`: feature toggles, sanitizers/analyzers/cache integration.
- `myproject_enable_sanitizers` → `cmake/Sanitizers.cmake`: sanitizer compile/link flags.

## Appendix A: Files Read
- `README.md`
- `CMakeLists.txt`
- `Dependencies.cmake`
- `ProjectOptions.cmake`
- `CMakePresets.json`
- `comp.sh`
- `src/CMakeLists.txt`
- `src/watpocket/CMakeLists.txt`
- `src/watpocket/main.cpp`
- `src/sample_library/CMakeLists.txt`
- `src/sample_library/sample_library.cpp`
- `include/myproject/sample_library.hpp`
- `src/ftxui_sample/CMakeLists.txt`
- `src/ftxui_sample/main.cpp`
- `test/CMakeLists.txt`
- `test/tests.cpp`
- `test/constexpr_tests.cpp`
- `fuzz_test/CMakeLists.txt`
- `fuzz_test/fuzz_tester.cpp`
- `configured_files/CMakeLists.txt`
- `configured_files/config.hpp.in`
- `cmake/CPM.cmake`
- `cmake/Sanitizers.cmake`
- `cmake/Tests.cmake`
- `cmake/StandardProjectSettings.cmake`
- `cmake/Cache.cmake`
- `cmake/CompilerWarnings.cmake`
- `cmake/Linker.cmake`
- `README_building.md`
- `README_dependencies.md`
- `README_docker.md`
- `tests/data/wcn/run.sh`
- `tests/data/wcn/sal.py`
- `tests/data/wcn/resal.py`
- `.github/workflows/auto-clang-format.yml`
- `.github/workflows/ci.yml`
- `.github/workflows/codeql-analysis.yml`
- `.github/workflows/template-janitor.yml`
- `.github/workflows/wasm.yml`
- `.gitlab-ci.yml`
- `.codex/agents/mapper.md`
