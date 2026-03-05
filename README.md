# watpocket

`watpocket` is a C++ command-line tool that reads a molecular structure, selects residues by residue number, extracts their C-alpha (`CA`) atoms, and computes a 3D convex hull using CGAL.

## Current Feature Set (v1)

- Single-frame structure input via Chemfiles (`.pdb`, `.cif`, `.mmcif` tested path)
- Required residue selection with `--resnums`
- Convex hull generation via CGAL `convex_hull_3`
- Optional draw output with `-d/--draw`:
  - `.py`: PyMOL script with hull edges as CGO lines
  - `.pdb`: hull-only PDB (`ANA` atoms + `CONECT` edges)
- Trajectory mode (`<topology> <trajectory.nc>`) with per-frame water-pocket analysis and CSV output

## CLI

```bash
watpocket <structure> --resnums <selectors> [-d output.py|output.pdb]
watpocket <topology> <trajectory.nc> --resnums <selectors> -o output.csv [-d hull_models.pdb]
```

### Input Rules

- With one positional input, it is treated as a structure file.
  - `parm7/.prmtop` is not supported in single-input mode (no coordinates); use trajectory mode instead.
- With two positional inputs, they are interpreted as `<topology> <trajectory>`.
  - `<trajectory>` currently supports NetCDF only (`.nc`).
  - `<topology>` can be structure (`.pdb/.cif/.mmcif`) or Amber topology (`.parm7/.prmtop`).
  - In trajectory mode, topology is used to define atom/residue identity and must be atom-count compatible with the trajectory.
  - If atom counts differ, `watpocket` exits with an error.

### Residue Selector Rules

- `--resnums` takes comma-separated selectors.
- Supported selector forms:
  - `12,15,18`
  - `A:12,A:15,B:18`
- If the input structure contains more than one chain, selectors must be chain-qualified (for example `A:12`).
- Every selected residue must contain exactly one atom named `CA`.
- For `parm7` topologies:
  - residue IDs are 1-based residue order (`1..NRES`);
  - chain-qualified selectors require `RESIDUE_CHAINID` in the topology;
  - if `RESIDUE_CHAINID` is missing, using `A:12`-style selectors is an error.

### Geometry Rules

The program raises an error if selected C-alpha points are:

- fewer than 4 points,
- collinear,
- coplanar.

## `--draw` Output

When `-d/--draw` is provided, output format is chosen by mode and filename extension:

- `.py`: writes a Python script for PyMOL that:
  - loads the input structure,
  - creates a CGO object with hull edges (`LINES` only),
  - applies default styling for readability.
  - requires input structure extension `.pdb`, `.cif`, or `.mmcif`.
  - single-structure mode only.

- `.pdb`: writes a hull-only PDB where:
  - each convex-hull vertex is an `ATOM` record,
  - atom element is `C`
  - residue name is `ANA`,
  - convex-hull edges are encoded with `CONECT` records,
  - if waters are found inside the hull, full water residues (all atoms) are appended as `ATOM` records
  - single-structure mode: one hull in one PDB file
  - trajectory mode: one hull per successful frame using `MODEL ... ENDMDL`, with inside-hull waters included per model

Trajectory mode accepts `--draw` only for `.pdb` paths. `.py` is rejected.

## Trajectory CSV Output

When running in trajectory mode (`<topology> <trajectory.nc>`), `watpocket` computes results independently for each frame and writes CSV:

- Header: `frame,resnums,total_count`
- `frame`: 1-based frame index
- `resnums`: water residue numbers separated by one space (quoted CSV field, empty as `""`)
- `total_count`: number of water residues inside the hull for that frame

Output destination:

- `-o/--output <path>`: required in trajectory mode; CSV is always written to this file

Optional trajectory draw output:

- `-d/--draw <path>.pdb`: writes trajectory hulls as multi-model PDB (`MODEL` records)

Frame handling:

- if one frame fails hull/water computation, that frame is skipped
- a warning is written to `stderr`
- remaining frames continue processing

After trajectory processing, `watpocket` prints summary statistics to stdout:

- min/max/mean/median waters per frame
- top 5 water `resnums` by presence in pocket, with fraction of frames present

## Dependencies

- Vendored in repository (no per-build download for these):
  - `external/cgal`
  - `external/chemfiles`
- CPM is still enabled for other dependencies (for example `CLI11`).

## Build

Use the existing CMake flow and presets from this repository:

```bash
cmake -S . -B build
cmake --build build -j
```

## Example

```bash
watpocket protein.pdb --resnums A:12,A:15,A:18,A:26 -d hull.py
```

Then in PyMOL:

```python
run hull.py
```

Trajectory mode example:

```bash
watpocket topology.pdb traj.nc --resnums 164,128,160,55,167,61,42,65,66 -o pocket_waters.csv
```

Trajectory mode with draw PDB example:

```bash
watpocket topology.parm7 traj.nc --resnums 164,128,160,55,167,61,42,65,66 -o pocket_waters.csv -d hull_models.pdb
```

`parm7` topology example:

```bash
watpocket topology.parm7 traj.nc --resnums 164,128,160,55,167,61,42,65,66 -o pocket_waters.csv
```

## Development

Compile, preferably with python bindings:

```bash
cmake -B./build --preset release -DWATPOCKET_ENABLE_PYTHON_BINDINGS=ON
cmake --build ./build --preset release
```

If you compiled with python bindings, install them in your current python environment and check that they are locatable:

```bash
cmake --install ./build --component pythoninstall
python -c "import watpocket; print(watpocket.__file__)"
```

### Release a New Version

`watpocket` Python releases are wheel-only and versioned from the git tag at `HEAD`.

Install release tooling in your environment first:

```bash
uv venv
source .venv/bin/activate
uv pip install -e ".[dev]"
```

1. Ensure your working tree is clean and choose the release version `X.Y.Z`.
2. Create and push an annotated tag:

```bash
git tag -a X.Y.Z -m "Release X.Y.Z"
git push origin X.Y.Z
```

3. Build wheels only (no upload):

```bash
scripts/pypi_release_wheels.sh --build-only
```

4. Upload to TestPyPI (recommended first):

```bash
export TWINE_PASSWORD="<testpypi-token>"
scripts/pypi_release_wheels.sh --testpypi
```

5. Upload to PyPI:

```bash
export TWINE_PASSWORD="<pypi-token>"
scripts/pypi_release_wheels.sh
```

Notes:
- Accepted release tags are `X.Y.Z` no `vX.Y.Z`
- The script uploads `dist/watpocket-<version>-*.whl` only (no source distribution).
- Default Twine username is `__token__`; override with `TWINE_USERNAME` if needed.
