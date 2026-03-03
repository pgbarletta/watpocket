# Codebase Map: watpocket
## 0. Quick Start for Contributors
### Add a new analysis: starting points + checklist
- Primary implementation file is `src/watpocket_lib/watpocket.cpp`; most watpocket logic (I/O, selection, callbacks, and writers) lives there, while CGAL hull operations are now isolated in `src/watpocket_lib/point_soa_cgal_adapter.cpp`. The CLI binary `src/watpocket/main.cpp` is a thin wrapper over the library API, and exported reader APIs provide non-CLI access to atom-indexed `PointSoA` extraction (source: `src/watpocket_lib/watpocket.cpp`, `src/watpocket_lib/point_soa_cgal_adapter.cpp`, `src/watpocket/main.cpp:main`, `include/watpocket/watpocket.hpp`).
- Checklist:
  - Define CLI interface in `src/watpocket/main.cpp` using CLI11 (`app.add_option` / `app.add_flag`) (source: `src/watpocket/main.cpp:main`).
  - Keep selector parsing and residue resolution consistent across topology backends (`parse_residue_selectors`, CA resolution helpers) (source: `src/watpocket_lib/watpocket.cpp:parse_residue_selectors`, `src/watpocket_lib/watpocket.cpp:resolve_ca_atom_indices`).
  - If the user asks for an implementation plan, follow planner-agent guidance in `.codex/agents/planner.md` before drafting the plan (source: `AGENTS.md`).
  - For trajectory features, keep two-input mode constraints explicit: NetCDF (`.nc`) is supported now; topology can be chemfiles-readable structure or Amber `parm7/prmtop`; XTC/DCD are still planned (source: `src/watpocket/main.cpp:main`, `is_netcdf_trajectory_path`, `is_parm7_topology_path`).
  - Define output path(s): stdout, draw artifact via `--draw` (`.py` or `.pdb`), trajectory CSV (`-o`), and trajectory draw PDB models when `--draw` is set in trajectory mode.
  - Add/extend tests (CLI smoke + `watpocket_api_tests`; no watpocket functional regression tests yet) (source: `test/CMakeLists.txt`).

### Add/modify protein/ligand selection semantics
- Current program does not implement explicit protein-vs-ligand classification; it operates on residue selectors and water residue heuristics (source: `src/watpocket_lib/watpocket.cpp:parse_residue_selectors`, water heuristics in `src/watpocket_lib/watpocket.cpp`).
- Selection semantics are custom CSV tokens (`12` or `A:12`) rather than `chemfiles::Selection` language (source: selector token parsing in `src/watpocket_lib/watpocket.cpp`).
- Multi-chain inputs enforce chain-qualified selectors (source: `src/watpocket_lib/watpocket.cpp:resolve_ca_atom_indices`).

### Debug PBC / units / alignment issues
- No PBC unwrap/rewrap/alignment is currently implemented in watpocket; coordinates are consumed as read from Chemfiles frames (source: `src/watpocket_lib/watpocket.cpp`).
- `chemfiles::UnitCell` and frame step/time are not read or used, so unit/PBC issues are currently latent integration gaps, not active transforms (source: `src/watpocket_lib/watpocket.cpp`).

### Debug CGAL robustness issues
- Kernel is `CGAL::Exact_predicates_inexact_constructions_kernel` (EPIK), giving exact orientation predicates with inexact constructed coordinates (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp`).
- Robustness gates before hull build: `<4 points`, `all collinear`, `all coplanar` are hard errors (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`).
- Point classification treats boundary points as inside by rejecting only positive-side plane evaluations (source: `src/watpocket_lib/watpocket.cpp:detail::point_inside_or_on_hull`).

### Improve performance (I/O vs geometry) starting points
- Structure mode reads one frame; trajectory mode streams all frames and performs hull/classification per frame, so runtime scales with frame count and water candidate count (source: `src/watpocket_lib/watpocket.cpp:analyze_trajectory_files`).
- Build-time cost is dominated by dependency targets and test executables when building whole tree; `comp.sh` already constrains build to target `watpocket` and disables heavy tooling/sanitizers (source: `comp.sh`, `Dependencies.cmake`, `test/CMakeLists.txt`, `ProjectOptions.cmake`).

### Refactor safely: invariants and guardrails
- Keep single binary deliverable `watpocket` intact (`add_executable(watpocket ...)`) while keeping the reusable library target `watpocket_lib` exported as `watpocket::watpocket` (source: `src/watpocket/CMakeLists.txt`, `src/watpocket_lib/CMakeLists.txt`, `CMakeLists.txt:myproject_package_project`).
- Preserve vendored dependency model for CGAL and Chemfiles (`external/cgal`, `external/chemfiles`) while leaving CPM enabled for other packages (source: `Dependencies.cmake`).
- Do not silently relax selector invariants: unique residue match, exactly one CA per selected residue, and multi-chain qualification rules (source: `src/watpocket_lib/watpocket.cpp:resolve_ca_atom_indices`, `src/watpocket_lib/watpocket.cpp:find_ca_atom_index`).

## 1. Repository Overview
### Directory tree (high-level)
```text
.
├── CMakeLists.txt
├── Dependencies.cmake
├── ProjectOptions.cmake
├── cmake/                     # shared CMake modules (sanitizers, warnings, linker, CPM bootstrap)
├── configured_files/          # template for generated config header
├── include/watpocket/         # public watpocket library headers (installable API)
├── plans/                     # implementation/refactor plans (Markdown)
├── src/
│   ├── watpocket_lib/         # watpocket library target (implementation)
│   ├── watpocket/             # production CLI binary
├── test/                      # template tests + CLI smoke tests
├── tests/data/wcn/            # watpocket sample data + script examples
├── external/
│   ├── chemfiles/             # vendored dependency
│   └── cgal/                  # vendored dependency
├── .codex/agents/             # Codex agent prompts (planner/mapper/refactorer)
└── .github/workflows/         # CI, CodeQL, formatting, wasm
```

### Build systems and entrypoints
- Build system is CMake; top-level adds `configured_files`, `src`, and optionally `test` (source: `CMakeLists.txt`).
- Runtime entrypoint for project deliverable is `src/watpocket/main.cpp` (`main`) (source: `src/watpocket/main.cpp:main`).
- `src/CMakeLists.txt` currently builds `watpocket_lib` and `watpocket` (source: `src/CMakeLists.txt`).
- For VSCode, the `vscode-release`/`vscode-watpocket` CMake presets configure into `./build` and build only the `watpocket` target with sanitizers/analyzers disabled (matches `comp.sh`) (source: `CMakePresets.json`, `comp.sh`).

### How chemfiles and CGAL are discovered/linked
- CGAL: `find_package(CGAL CONFIG REQUIRED PATHS external/cgal NO_DEFAULT_PATH)` (source: `Dependencies.cmake`).
- Chemfiles: vendored subdirectory `add_subdirectory(external/chemfiles ...)` with tests/docs disabled (source: `Dependencies.cmake`).
- CPM remains enabled and currently fetches Catch2 + CLI11 if absent (source: `Dependencies.cmake`, `cmake/CPM.cmake`).
- CPM bootstrap always performs a `file(DOWNLOAD ...)` for `CPM.cmake` unless provided via cache location/environment path (source: `cmake/CPM.cmake`).

### Configuration surfaces (CLI flags, config files, env vars)
- CLI (`watpocket`):
  - positional `inputs` with arity 1..2
  - `--resnums` required
  - `-d,--draw` optional draw output path:
    - single-structure mode: `.py` PyMOL or `.pdb` hull
    - trajectory mode: `.pdb` only, written as per-frame `MODEL` blocks
  - `-o,--output` required CSV path in trajectory mode (CSV is file-only)
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
  EXT[External caller\n(public library API)] --> READ[Exported point readers\n`read_*_points_by_atom_indices`]
  CLI[CLI / Argument Parsing\n`main()` + CLI11] --> MODE{1 or 2 inputs}
  READ --> IOREAD[Open structure or topology+NetCDF frame]
  IOREAD --> SOAREAD[`fill_points_from_atom_indices`]
  MODE -->|1 input| IO1[Read structure frame]
  IO1 --> SEL1[Resolve CA indices]
  SEL1 --> SOA1[Fill reusable PointSoA]
  SOA1 --> GEO1[Private CGAL adapter hull]
  GEO1 --> ANA1[Water inside-hull]
  ANA1 --> OUT1[stdout summary + optional draw artifact]
  OUT1 --> DRAW[optional `--draw`\n`.py` PyMOL or `.pdb` hull]
  MODE -->|2 inputs| TOP[Load topology\n(chemfiles or parm7 parser)]
  TOP --> CACHE[Cache CA/water atom indices]
  MODE -->|2 inputs| TRAJ[Read NetCDF trajectory frames]
  TRAJ --> FRAME[Per-frame PointSoA hull + water classification]
  CACHE --> FRAME
  FRAME --> CSV[CSV rows to required `-o` file]
  FRAME --> TDRAW[Optional trajectory draw PDB\n`--draw *.pdb` with MODEL blocks]
  FRAME --> STATS[Trajectory stats to stdout\nmin/max/mean/median + top 5 waters]
```

### Data-flow diagram (frame lifecycle)
```mermaid
flowchart TD
  CLI[watpocket CLI\n`src/watpocket/main.cpp`] --> MODE{1 or 2 positional args?}
  EXT[External caller\n(public API)] --> RAPI1[Library: `read_structure_points_by_atom_indices(...)`]
  EXT --> RAPI2[Library: `read_trajectory_points_by_atom_indices(...)`]
  RAPI1 --> RIO1[Chemfiles: open + read first frame]
  RIO1 --> RPTS1[`fill_points_from_atom_indices(...)`]
  RAPI2 --> RTOP[Load topology atom count\n(structure or parm7/prmtop)]
  RAPI2 --> RTRJ[Open NetCDF trajectory + validate frame range]
  RTRJ --> RFRAME[Read requested frame (1-based API)]
  RTOP --> RFRAME
  RFRAME --> RPTS2[`fill_points_from_atom_indices(...)`]

  MODE -->|1| API1[Library: `watpocket::analyze_structure_file(path, selectors)`]
  API1 --> IO1[Chemfiles: open + read first frame]
  IO1 --> SEL1[Resolve CA indices]
  SEL1 --> SOA1[Fill `PointSoA` CA buffer]
  SOA1 --> GEO1[Private CGAL adapter:\nconvex hull + halfspaces]
  GEO1 --> ANA1[Collect + classify waters inside hull]
  ANA1 --> RES1[`StructureAnalysisResult`\n(hull + water ids + PDB water atoms)]
  RES1 --> OUT1[CLI prints stdout summary]
  RES1 --> DRAW1{`-d/--draw` set?}
  DRAW1 -->|.py| PY[Library: `write_pymol_draw_script(...)`]
  DRAW1 -->|.pdb| PDB[Library: `write_hull_pdb(...)`]

  MODE -->|2| API2[Library: `watpocket::analyze_trajectory_files(topology, trajectory, ...)`]
  API2 --> TOP{Topology backend}
  TOP -->|parm7/prmtop| P7[Parse parm7 topology]
  TOP -->|structure file| CF[Chemfiles: read topology frame]
  P7 --> CACHE[Cache CA indices + water refs]
  CF --> CACHE
  API2 --> SAN[Sanity check: traj has frames + atom-count matches]
  SAN --> LOOP[Per-frame loop: read frame]
  CACHE --> LOOP
  LOOP --> FRAME[CA indices -> PointSoA -> hull -> water classification]
  FRAME -->|ok| CSV[Write CSV row (if enabled)]
  FRAME -->|ok| TDRAW[Append MODEL to trajectory draw PDB (if enabled)]
  FRAME -->|ok| CB[Callback: `on_frame` (if provided)]
  FRAME -->|error| WARN[Callback: `on_warning`; skip frame]
  LOOP --> STATS[Accumulate stats]
  STATS --> RES2[`TrajectorySummary`]
  RES2 --> OUT2[CLI prints stats + processed counts]
```

### End-to-end data flow narrative
- Inputs are parsed with CLI11 in `main()`.
- Non-CLI callers can directly obtain CA or arbitrary atom subsets through `read_structure_points_by_atom_indices(...)` and `read_trajectory_points_by_atom_indices(...)` without exposing Chemfiles/CGAL in the public header.
- For 1 positional file, the CLI calls `watpocket::analyze_structure_file(...)`; the library uses Chemfiles to read one frame and runs structure-mode analysis + optional draw writing.
- For 2 positional files, the CLI calls `watpocket::analyze_trajectory_files(...)`; the library loads topology once (via Chemfiles for structure files or custom parser for `parm7/prmtop`), caches CA/water atom indices, streams NetCDF frames, and writes one CSV row per successful frame.
- Residue selectors are parsed and mapped to residue objects using topology-derived lookup tables; each selected residue must map to exactly one residue and exactly one `CA` atom.
- CA coordinates are extracted into `watpocket::PointSoA` buffers and passed as `PointSoAView` to the private CGAL adapter.
- The private adapter (`src/watpocket_lib/point_soa_cgal_adapter.cpp`) computes `CGAL::Surface_mesh` hull geometry and inward-oriented halfspaces.
- Water oxygen atoms are identified from residue/atom name heuristics and tested against inward halfspace coefficients.
- Outputs include structure-mode stdout results plus optional draw artifact selected by `--draw` extension (`.py` or `.pdb`), and trajectory-mode CSV (`frame,resnums,total_count`) to required `-o` path, optional trajectory draw PDB (`MODEL` blocks) when `--draw` is `.pdb`, with inside-hull waters appended as regular `ATOM` records, plus stdout trajectory statistics (min/max/mean/median waters per successful frame and top 5 waters by frame presence). (source: `src/watpocket/main.cpp:main`, `src/watpocket_lib/watpocket.cpp:analyze_structure_file`, `analyze_trajectory_files`, `write_pymol_draw_script`, `write_hull_pdb`, `include/watpocket/watpocket.hpp`, `src/watpocket/main.cpp:write_trajectory_statistics`)

### Representative workflows (module-level call traces)
1. Single-structure, analysis-only (no `-d`)
   - Example form: `watpocket protein.pdb --resnums A:12,A:15,A:18,A:26` (source pattern: `README.md`).
   - Call spine:
```text
main
  -> watpocket::parse_residue_selectors
  -> watpocket::analyze_structure_file
  -> stdout summary + select line
```
2. Single-structure, draw mode (`--draw` extension-based)
   - Example script path shown in repo workflow data (`tests/data/wcn/run.sh` calling `-d sal.py`) (source: `tests/data/wcn/run.sh`).
   - Additional branch:
```text
...same as above...
  -> inspect draw output extension
  -> `.py`: watpocket::write_pymol_draw_script
  -> `.pdb`: watpocket::write_hull_pdb
```
3. Topology + trajectory invocation (NetCDF, multi-backend topology)
   - Form: `watpocket <topology.{pdb|cif|mmcif|parm7|prmtop}> <trajectory.nc> --resnums ... -o output.csv [-d hull_models.pdb]`.
   - Call spine:
```text
main
  -> watpocket::parse_residue_selectors
  -> require `-o/--output` path
  -> validate optional `--draw` (trajectory accepts `.pdb` only)
  -> watpocket::analyze_trajectory_files (CSV + optional draw PDB + callbacks)
  -> write_trajectory_statistics (CLI formatting)
```

### Cross-cutting concerns
- Correctness (units/PBC): no coordinate transforms or unit conversion implemented; correctness assumes input coordinates are directly suitable for hull geometry (source: `src/watpocket_lib/watpocket.cpp`).
- Geometry robustness: explicit degeneracy guards + exact-predicate kernel + face-orientation correction in the private adapter (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`).
- Determinism: uses deterministic containers/sorting for output IDs and deduplicated edges (`std::set`) (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`, `src/watpocket_lib/watpocket.cpp:find_waters_inside_hull`).
- Performance: single-threaded loops; trajectory mode streams frames sequentially and performs hull/classification per frame (source: `src/watpocket_lib/watpocket.cpp:analyze_trajectory_files`).
- Error reporting: fatal errors throw `watpocket::Error` (caught and printed by the CLI), while trajectory frame-local compute failures emit warnings via callback and are skipped (source: `src/watpocket_lib/watpocket.cpp:analyze_trajectory_files`, `src/watpocket/main.cpp:main`).

## 3. Domain Data Model
### Chemical view
- `watpocket::ResidueSelector` encodes requested residue as optional chain + residue id from `--resnums` CSV tokens (source: `include/watpocket/watpocket.hpp`, `src/watpocket_lib/watpocket.cpp:parse_selector_token`).
- `ResidueLookup` indexes topology residues by `(chain,id)` and by `id` only, and tracks the set of observed chains (source: `src/watpocket_lib/watpocket.cpp:ResidueLookup`, `build_residue_lookup`).
- `Parm7Topology` stores only topology data needed by watpocket trajectory mode: atom names, residue labels, residue pointers/ranges, atom-to-residue map, and optional `RESIDUE_CHAINID` values (source: `src/watpocket_lib/watpocket.cpp:Parm7Topology`, `parse_parm7_topology`).
- Water proxy model is oxygen atom refs (`resid`, `atom_index`) built from topology residues whose resname is in `{HOH,WAT,TIP3,TIP3P,SPC,SPCE}` and atom name in `{O,OW}` (source: `src/watpocket_lib/watpocket.cpp:collect_water_oxygen_refs`).
- Ligand-specific identity rules are not present in current implementation.

### Trajectory view
- CLI allows either one structure input or two files (`<topology> <trajectory>`); two-file path currently supports NetCDF (`.nc`) trajectories with topology from either a chemfiles-readable structure or `parm7/prmtop`. Single-input mode rejects `parm7/prmtop` explicitly because these files do not provide coordinates (source: `src/watpocket/main.cpp:main`).
- Runtime processing model:
  - structure mode: library reads one frame and runs hull/water classification once.
  - trajectory mode: library caches CA indices/water refs from topology, then loops over frames, recomputes hull + water classification per frame, writes CSV rows on successful frames, optionally writes draw `MODEL` blocks when `--draw *.pdb` is provided, warns/skips on frame-local compute failures, and returns aggregate stats for the CLI to print (source: `src/watpocket_lib/watpocket.cpp:analyze_structure_file`, `analyze_trajectory_files`).
- Frame/time metadata are currently ignored.

### Geometry view
- Public point representation: `watpocket::PointSoA` + `PointSoAView`/`PointSoAMutableView` (source: `include/watpocket/watpocket.hpp`).
- Kernel: `CGAL::Exact_predicates_inexact_constructions_kernel` in private adapter code (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp`).
- Primitives:
  - `CGAL::Surface_mesh<Point_3>` for hull (private)
  - inward halfspaces stored as coefficient arrays `[a,b,c,d]` (source: `src/watpocket_lib/point_soa_cgal_adapter.hpp`).
- Mapping:
  - residue selector -> unique residue -> CA atom index -> `PointSoA`
  - classified water oxygen points -> residue IDs -> structure stdout/draw or trajectory CSV outputs (source: `src/watpocket_lib/watpocket.cpp:resolve_ca_atom_indices`, `collect_water_oxygen_refs`, `find_waters_inside_hull`, `write_csv_row`).

### Metadata/properties
- Chain ID extraction uses residue string properties `chainid` first, then `chainname`, else empty string (source: `src/watpocket_lib/watpocket.cpp:residue_chain_id`).
- Drawability filter uses filename extensions `.pdb`, `.cif`, `.mmcif` (source: `src/watpocket/main.cpp:is_drawable_structure_path`).

## 4. Input, Selection, and Preprocessing
### How selection is defined
- Selection parser is custom token logic, not `chemfiles::Selection`:
  - split CSV by comma
  - parse each token as `RESID` or `CHAIN:RESID`
  - reject malformed values (source: `include/watpocket/watpocket.hpp`, `src/watpocket_lib/watpocket.cpp:parse_residue_selectors`, `parse_selector_token`, `parse_int64`).
- Matching behavior:
  - chemfiles topology: multichain inputs require explicit chain-qualified selectors
  - parm7 topology: residue IDs are 1..NRES; chain-qualified selectors are allowed only if `RESIDUE_CHAINID` section exists
  - missing residue or non-unique residue match is an error
  - each selected residue must contain exactly one `CA` atom (source: `src/watpocket_lib/watpocket.cpp:resolve_ca_atom_indices`, `find_ca_atom_index`).

### Coordinate preprocessing (PBC/alignment/units)
- No wrap/unwrap/reimage/alignment/centering stage currently exists; positions are consumed as-is from `frame.positions()` and copied into reusable `PointSoA` buffers (source: `src/watpocket_lib/watpocket.cpp:fill_points_from_atom_indices`).
- No explicit unit conversion is applied in code.

### Invariants after preprocessing
- Selector list is non-empty and syntactically valid (source: `include/watpocket/watpocket.hpp`, `src/watpocket_lib/watpocket.cpp:parse_residue_selectors`).
- Selected points correspond to exactly one CA atom per requested residue (source: `src/watpocket_lib/watpocket.cpp:resolve_ca_atom_indices`, `find_ca_atom_index`).
- Before geometry build: at least four points, non-collinear, non-coplanar (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`).

## 5. Geometry System (CGAL)
### Library boundary diagram (chemfiles/parm7 parser ↔ adapters ↔ CGAL)
```mermaid
flowchart LR
  subgraph CHEM[chemfiles]
    T[Trajectory]
    F[Frame]
    Top[Topology + Residue]
    Pos[positions()]
    Prop[Residue properties\n`chainid`/`chainname`]
  end

  subgraph PARM7[parm7 text parser]
    PFile[parm7 file]
    PSec[%FLAG/%FORMAT sections]
    PTopo[Minimal topology:\natom names, residue labels,\nresidue pointers, chain ids]
  end

  subgraph LIB[watpocket library (`src/watpocket_lib/watpocket.cpp`)]
    Parse[Selector parsing\n`parse_residue_selectors()`]
    Lookup[Residue maps\n`build_residue_lookup()`]
    CA[CA extraction\n`resolve_ca_atom_indices()` + `fill_points_from_atom_indices()`]
    Water[Water refs + materialization\n`collect_water_oxygen_refs()`]
    Classify[Hull membership\n`detail::point_inside_or_on_hull()`]
  end

  subgraph ADAPT[private CGAL bridge (`src/watpocket_lib/point_soa_cgal_adapter.cpp`)]
    Bridge[SoA index/property-map adapter]
    CHull[`detail::compute_hull_data()`]
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
  PFile --> PSec --> PTopo

  Top --> Lookup
  PTopo --> Lookup
  Pos --> CA
  Parse --> CA
  Pos --> Water
  Water --> Classify

  CA --> Bridge --> CHull
  CHull --> Ker
  Ker --> CH --> Mesh --> Plane --> Classify
```

### Which CGAL components are used and why
- `CGAL::convex_hull_3`: computes 3D convex hull from transformed index iterators backed by an SoA property-map adapter (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`).
- `CGAL::Surface_mesh`: stores hull topology/geometry for extracting edges and faces (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp`).
- `CGAL::collinear` / plane checks: preflight degeneracy checks (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp`).

### Kernel configuration and robustness strategy
- Kernel choice is EPIK (exact predicates, inexact constructions), balancing robustness and speed for orientation/side tests (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp`).
- Degeneracy handling is explicit and fail-fast with clear errors before hull call (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`).
- Face orientation normalization:
  - build each face plane and ensure it is oriented inward by testing a guaranteed interior point (centroid of selected CA points) and flipping if needed; this avoids false failures due to inexact plane constructions under EPIK (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`).
  - halfspaces are exported as `[a,b,c,d]` coefficients and evaluated in watpocket core without CGAL types (source: `src/watpocket_lib/point_soa_cgal_adapter.hpp`, `src/watpocket_lib/watpocket.cpp`).

### Core geometric structures
- `HullGeometry.edges`: deduplicated list of segment endpoints (`std::array<double,6>`) for draw output (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`, `include/watpocket/watpocket.hpp`).
- Inward-oriented face halfspaces are stored internally as coefficient arrays for classification (source: `src/watpocket_lib/point_soa_cgal_adapter.hpp`, `src/watpocket_lib/watpocket.cpp`).
- No BVH/AABB/KD-tree/triangulation acceleration structures are currently used.

### Mapping between geometric entities and atoms/residues
- CA mapping uses topology backend-specific traversal:
  - chemfiles path: iterate `Residue` atom indices
  - parm7 path: iterate residue atom ranges from `RESIDUE_POINTER` (source: `src/watpocket_lib/watpocket.cpp:find_ca_atom_index`, `resolve_ca_atom_indices`).
- Water mapping returns residue IDs, not atom indices; duplicates are removed via `std::set` (source: `src/watpocket_lib/watpocket.cpp:find_waters_inside_hull`).

### Common pitfalls + debugging notes
- Chemfiles path: user must supply chain-qualified selectors when topology has multiple chains, else hard error.
- Parm7 path: chain-qualified selectors error if `RESIDUE_CHAINID` is absent (source: `src/watpocket_lib/watpocket.cpp:resolve_ca_atom_indices`).
- `parm7/prmtop` is trajectory-topology-only input; using it as the sole positional argument is a hard error (source: `src/watpocket/main.cpp:main`).
- `-d/--draw` output extension must be `.py` or `.pdb`; in trajectory mode only `.pdb` is allowed, while `.py` additionally requires single-structure PDB/CIF input (source: `src/watpocket/main.cpp:main`, `is_drawable_structure_path`, `is_pymol_draw_output_path`, `is_pdb_draw_output_path`).
- `-o/--output` is required in trajectory mode and invalid in single-structure mode (source: `src/watpocket/main.cpp:main`).
- Boundary points count as inside by design (source: `src/watpocket_lib/watpocket.cpp:detail::point_inside_or_on_hull`).
- Selection parser is strict integer parsing and fails on malformed tokens/spaces around empty fields (source: `src/watpocket_lib/watpocket.cpp:parse_int64`, `parse_selector_token`).

## 6. Analysis Kernels
### Analysis A: Convex hull from selected CA atoms
- Purpose and outputs:
  - Build 3D hull from user-selected residues and extract line segments + face halfspaces.
  - Outputs: `HullGeometry.edges` and private halfspace coefficients (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`).
- Key files + symbols:
  - `src/watpocket_lib/watpocket.cpp`: `resolve_ca_atom_indices`, `fill_points_from_atom_indices`.
  - `src/watpocket_lib/point_soa_cgal_adapter.cpp`: `compute_hull_data`.
- Inputs required:
  - Valid topology metadata (chemfiles frame or parsed parm7) to resolve CA indices, plus frame coordinates for each analyzed step.
- Correctness notes:
  - Fails for `<4` or degenerate geometry; uses EPIK kernel and oriented-plane normalization.
- Performance notes:
  - `resolve_ca_atom_indices` scales with selected residues and lookup map operations.
  - `compute_hull_data` cost dominated by `convex_hull_3` + mesh traversals.
- Extension points:
  - Replace or augment hull with alpha-shape/AABB-based analyses while reusing CA extraction.

### Analysis B: Water oxygen inside-hull classification
- Purpose and outputs:
  - Identify water residues with oxygen atoms inside or on hull boundary; return sorted residue IDs.
  - Outputs are printed for structure mode by the CLI and serialized to CSV per frame for trajectory mode by the library; trajectory mode additionally returns aggregate stats for the CLI to print after all frames (source: `src/watpocket_lib/watpocket.cpp:collect_water_oxygen_refs`, `find_waters_inside_hull`, `write_csv_row`, `summarize_trajectory`, `src/watpocket/main.cpp:write_trajectory_statistics`).
- Key files + symbols:
  - `src/watpocket_lib/watpocket.cpp`: `collect_water_oxygen_refs`, `detail::point_inside_or_on_hull`, `find_waters_inside_hull`, `write_csv_row`.
- Inputs required:
  - Structure frame (single mode) or topology-derived atom refs + trajectory frame loop (NetCDF mode), where topology may come from chemfiles or parsed parm7.
- Correctness notes:
  - Water definition is heuristic on residue + atom names.
  - Multiple oxygens in one residue are collapsed to one residue ID by `std::set`.
- Performance notes:
  - Water oxygen refs are cached once from topology in trajectory mode; each frame still tests candidates against all hull planes.
- Extension points:
  - Chain-aware water output and support for additional trajectory formats (XTC/DCD).

### Analysis C: Draw artifact generation (visual artifact)
- Purpose and outputs:
  - Emit output controlled by `-d/--draw` extension:
    - single-structure `.py`: PyMOL Python script loading structure, drawing hull as CGO lines, and highlighting `watpocket` waters.
    - single-structure `.pdb`: hull PDB where vertices are `ANA` atoms, edges are `CONECT` bonds, and inside-hull waters are appended.
    - trajectory `.pdb`: multi-model hull PDB where each successful frame contributes one `MODEL ... ENDMDL` block with inside-hull waters appended.
  - Output file path comes from `-d/--draw`; trajectory mode accepts `.pdb` only (source: `src/watpocket/main.cpp:main`, `src/watpocket_lib/watpocket.cpp:write_pymol_draw_script`, `write_hull_pdb`, `write_hull_pdb_model`).
- Key files + symbols:
  - `src/watpocket_lib/watpocket.cpp`: `write_pymol_draw_script`, `write_hull_pdb`, `write_hull_pdb_model`, `write_hull_pdb_records`, `collect_water_atoms_for_pdb`, `write_pdb_atom_record`, `python_quoted`, `make_pymol_resi_selector`.
- Inputs required:
  - `.py`: single input file with extension `.pdb`/`.cif`/`.mmcif` + computed hull + classified water IDs.
  - single-structure `.pdb`: computed hull (`HullData.vertices` + `HullData.bonds`).
  - trajectory `.pdb`: per-frame computed hull (`HullData.vertices` + `HullData.bonds`) with frame-index model serial.
- Correctness notes:
  - `.py` script creates fixed selection name `watpocket` and hides all solvent except selected waters.
  - `.pdb` output uses fixed-column PDB `ATOM` records (coordinates in columns 31-54), 1-based atom serials, deduplicated adjacency for `CONECT` (hull bonds), and appends any inside-hull water residues as standard atom records.
  - Trajectory-mode `.pdb` writes one model per successful frame; skipped frames emit warnings to stderr and no model.
- Performance notes:
  - Linear in hull edge count for `.py` and hull vertex+edge count for `.pdb`; trajectory `.pdb` adds sequential write overhead per successful frame.
- Extension points:
  - Add optional faces/surfaces, per-frame snapshots, or additional named selections.

## 7. Parallel & Performance Model
### Parallelization strategy
- Runtime analysis code is single-threaded; there is no OpenMP/TBB/std::execution/MPI in watpocket core (source: `src/watpocket_lib/watpocket.cpp`).
- Build system supports parallel compilation via CMake generator/build tool, but that is build-time only.

### Determinism expectations and reduction policies
- Deterministic output ordering is enforced for water residue IDs (`std::set`) and hull edges (`std::set` in adapter) (source: `src/watpocket_lib/point_soa_cgal_adapter.cpp:compute_hull_data`, `src/watpocket_lib/watpocket.cpp:find_waters_inside_hull`).
- Floating-point values in script are formatted with fixed precision 4 decimals (source: `src/watpocket_lib/watpocket.cpp:write_pymol_draw_script`).

### Memory/layout strategy and caching
- Transient vectors/maps are allocated per invocation (`ResidueLookup`, edge vectors, halfspaces); trajectory mode caches CA atom indices and water oxygen atom refs and reuses a `PointSoA` CA buffer per frame (source: `src/watpocket_lib/watpocket.cpp`, `src/watpocket_lib/point_soa_cgal_adapter.cpp`).
- Structure mode reads one frame; trajectory mode reads one frame at a time, emits CSV rows incrementally, and accumulates frame-level summary counters for final statistics.

### I/O vs compute overlap
- No overlap exists, but trajectory mode is frame-streaming and strictly sequential in `main()`.

### Benchmark harness and profiling hooks
- No dedicated watpocket benchmark targets found.
- Tests are currently focused on CLI smoke contracts and public API sanity checks (source: `test/CMakeLists.txt`, `test/watpocket_api_tests.cpp`, `test/package_consumer.cpp`).

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
  - `watpocket --version` regex check
  - single-structure draw artifacts:
    - `--draw` rejects unknown output extension
    - `--draw` with `.pdb` writes hull PDB output containing inside-hull waters when present
  - trajectory draw artifacts:
    - trajectory `--draw .pdb` writes model PDB output containing inside-hull waters when present
    - trajectory `--draw .py` is rejected
  - trajectory-mode contract:
    - missing `-o` in trajectory mode should fail
  - `parm7` guardrails:
    - single-input `parm7` should fail with trajectory-mode guidance
    - chain-qualified selectors on `parm7` without `RESIDUE_CHAINID` should fail (source: `test/CMakeLists.txt`).
- Unit tests cover selector parsing, public `PointSoA`/view contracts, and exported point-reader APIs (structure + trajectory success/failure contracts, determinism, empty/duplicate index behavior) in `watpocket_api_tests`, alongside CLI smoke tests (source: `test/CMakeLists.txt`, `test/watpocket_api_tests.cpp`).
- Draw output tests include content validation through `test/verify_draw_pdb_contains_waters.cmake`, asserting that generated draw PDBs contain water atoms for the WCN sample configuration.

### CI notes and platform caveats
- GitHub Actions matrix includes Linux/macOS/Windows with clang/gcc/msvc and Debug/Release variants (source: `.github/workflows/ci.yml`).
- CodeQL workflow builds C++ target on Ubuntu (source: `.github/workflows/codeql-analysis.yml`).
- Linux CI jobs install `libboost-dev`, and macOS CI jobs install Homebrew `boost`, before CMake configure so vendored CGAL's required `find_package(Boost 1.74)` succeeds on GitHub-hosted runners (source: `.github/workflows/ci.yml`, `.github/workflows/codeql-analysis.yml`, `external/cgal/Installation/cmake/modules/CGAL_SetupBoost.cmake`).
- In `ci.yml`, tests/coverage execution and Codecov upload run on non-Windows runners only; Windows jobs now stop after configure/build/package steps (source: `.github/workflows/ci.yml`).
- Sanitizers and static analyzers are enabled by default in top-level non-maintainer builds unless explicitly disabled (source: `ProjectOptions.cmake`, `cmake/Sanitizers.cmake`).
- CPM bootstrap performs network download unless cached/offline-provided (source: `cmake/CPM.cmake`).

### Debugging/profiling cookbook grounded in repo tooling
- To minimize build latency for watpocket-only iteration:
  - Disable sanitizers/analyzers/cache similar to `comp.sh`.
  - Build target `watpocket` only.
- To validate packaging/version metadata path:
  - Run `watpocket --version` (value comes from generated configured header) (source: `configured_files/config.hpp.in`, `src/watpocket/main.cpp:main`).
- For geometry correctness debugging:
  - Use `-d/--draw` to emit script (`.py`, single-structure) or model PDB (`.pdb`, including trajectory mode) for viewer inspection (source: `src/watpocket/main.cpp:main`, `src/watpocket_lib/watpocket.cpp:write_pymol_draw_script`, `write_hull_pdb`, `write_hull_pdb_model`).

## 9. Change Playbooks (Practical Guides)
### Add a new analysis over trajectories
1. Keep `src/watpocket/main.cpp` as CLI parsing + mode gating only.
2. Implement the new trajectory analysis in the library path (`src/watpocket_lib/watpocket.cpp`) and keep CGAL-specific geometry in `src/watpocket_lib/point_soa_cgal_adapter.cpp`.
3. If adding new formats, expand the CLI gating (`is_netcdf_trajectory_path`) and the library reader/writer logic in lockstep.
4. Keep selector parsing, CA index caching, and water-ref caching reusable for each frame.
5. Preserve current CLI error/warning strings (tests treat these as contracts).

### Add a new CGAL-based geometry computation
1. Add geometry helpers in `src/watpocket_lib/point_soa_cgal_adapter.cpp` to keep CGAL private.
2. Reuse `PointSoA`/`PointSoAView` extraction from Chemfiles positions.
3. Explicitly define kernel and degeneracy behavior.
4. Add deterministic serialization for outputs (sorted stable ordering where applicable).
5. Wire optional visualization into the draw writers if relevant (`write_pymol_draw_script`, PDB writers).

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
- Public API surface (`watpocket::Error`, `PointSoA`/views, `ResidueSelector`, results, callbacks, exported point readers) → `include/watpocket/watpocket.hpp`.
- Public API implementation:
  - `watpocket::build_version` / `parse_residue_selectors` / `read_structure_points_by_atom_indices` / `read_trajectory_points_by_atom_indices` / `analyze_structure_file` / `analyze_trajectory_files` / `write_pymol_draw_script` / `write_hull_pdb` → `src/watpocket_lib/watpocket.cpp`.
- Internal library helpers:
  - `parse_parm7_sections` / `parse_parm7_topology` → `src/watpocket_lib/watpocket.cpp`.
  - `residue_chain_id` / `build_residue_lookup` → `src/watpocket_lib/watpocket.cpp`.
  - `find_ca_atom_index` / `resolve_ca_atom_indices` / `resolve_ca_points` / `fill_points_from_atom_indices` → `src/watpocket_lib/watpocket.cpp`.
  - `compute_hull_data` / `point_inside_or_on_hull` (private CGAL bridge) → `src/watpocket_lib/point_soa_cgal_adapter.cpp`.
  - `collect_water_oxygen_refs` / `find_waters_inside_hull` → `src/watpocket_lib/watpocket.cpp`.
  - `write_csv_header` / `write_csv_row` / `summarize_trajectory` → `src/watpocket_lib/watpocket.cpp`.
  - `write_hull_pdb_model` / `write_hull_pdb_records` / `collect_water_atoms_for_pdb` → `src/watpocket_lib/watpocket.cpp`.
- CLI-only formatting helpers:
  - `write_trajectory_statistics` → `src/watpocket/main.cpp`.
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
- `src/watpocket_lib/CMakeLists.txt`
- `src/watpocket_lib/watpocket.cpp`
- `src/watpocket_lib/point_soa_cgal_adapter.hpp`
- `src/watpocket_lib/point_soa_cgal_adapter.cpp`
- `include/watpocket/watpocket.hpp`
- `test/watpocket_api_tests.cpp`
- `test/package_consumer.cpp`
- `test/CMakeLists.txt`
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
- `.github/workflows/wasm.yml`
- `.gitlab-ci.yml`
- `.codex/agents/mapper.md`
