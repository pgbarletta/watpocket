# watpocket

`watpocket` is a C++ command-line tool that reads a molecular structure, selects residues by residue number, extracts their C-alpha (`CA`) atoms, and computes a 3D convex hull using CGAL.

## Current Feature Set (v1)

- Single-frame structure input via Chemfiles (`.pdb`, `.cif`, `.mmcif` tested path)
- Required residue selection with `--resnums`
- Convex hull generation via CGAL `convex_hull_3`
- Optional PyMOL script output with `-d/--draw` to render hull edges as CGO lines

## CLI

```bash
watpocket <structure> --resnums <selectors> [-d output.py]
watpocket <topology> <trajectory> --resnums <selectors>
```

### Input Rules

- With one positional input, it is treated as a structure file.
- With two positional inputs, they are interpreted as `<topology> <trajectory>`, but this mode is not implemented yet in v1.

### Residue Selector Rules

- `--resnums` takes comma-separated selectors.
- Supported selector forms:
  - `12,15,18`
  - `A:12,A:15,B:18`
- If the input structure contains more than one chain, selectors must be chain-qualified (for example `A:12`).
- Every selected residue must contain exactly one atom named `CA`.

### Geometry Rules

The program raises an error if selected C-alpha points are:

- fewer than 4 points,
- collinear,
- coplanar.

## `--draw` PyMOL Script Output

When `-d/--draw` is provided, `watpocket` writes a Python script for PyMOL that:

- loads the input structure,
- creates a CGO object with hull edges (`LINES` only),
- applies default styling for readability.

In draw mode, the input must be `.pdb`, `.cif`, or `.mmcif`.

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
