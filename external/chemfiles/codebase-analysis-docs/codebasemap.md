# Codebase Map: chemfiles

## 0. Quick Start for Contributors

### Add a new format: starting points + checklist
- Start from the format interfaces in `include/chemfiles/Format.hpp` (`chemfiles::Format`, `chemfiles::TextFormat`, `format_metadata<T>()`).
- Implement a format class under `src/formats/` and expose metadata through `format_metadata<YourFormat>()`; metadata is validated at registration (`src/FormatMetadata.cpp:FormatMetadata::validate`, `include/chemfiles/FormatMetadata.hpp`).
- Register it in `FormatFactory::FormatFactory()` (`src/FormatFactory.cpp:FormatFactory::FormatFactory`) using `add_format<YourFormat>()` (`include/chemfiles/FormatFactory.hpp:FormatFactory::add_format`).
- If memory I/O is supported, provide constructor `(std::shared_ptr<MemoryBuffer>, File::Mode, File::Compression)` so `SupportsMemoryIO` path is selected (`include/chemfiles/FormatFactory.hpp:SupportsMemoryIO`, `include/chemfiles/FormatFactory.hpp:FormatFactory::add_format`).
- Add tests in `tests/formats/` and integration tests through `tests/trajectory.cpp`; tests are auto-discovered by `tests/CMakeLists.txt`.

### Debug a parsing issue: starting points
- Trace entry from `Trajectory` open/read to format dispatch (`src/Trajectory.cpp:Trajectory::Trajectory`, `src/Trajectory.cpp:Trajectory::read`, `src/Trajectory.cpp:Trajectory::read_at`).
- Verify format guess and compression parsing (`src/guess_format.cpp:guess_format`, `src/Trajectory.cpp:file_open_info::parse`).
- For text formats, inspect `TextFormat` frame-position caching and `forward()` behavior (`src/Format.cpp:TextFormat::scan_all`, `src/Format.cpp:TextFormat::read_at`).
- For format-specific records, inspect parser functions in target file (for example `src/formats/PDB.cpp:read_ATOM`, `src/formats/PDB.cpp:read_CRYST1`, `src/formats/PDB.cpp:read_CONECT`).

### Improve I/O performance: starting points
- Text path hotspots: `TextFile::readline()` buffering, dynamic buffer growth, and memmove (`src/File.cpp:TextFile::readline`).
- Binary path hotspots: mmap windowing and remap sync (`src/files/BinaryFile.cpp:CHEMFILES_MMAP_SIZE`, `src/files/BinaryFile.cpp:BinaryFile::remap_file`).
- Compressed coordinates: XDR compression/decompression loops (`src/files/XDRFile.cpp:read_gmx_compressed_floats`, `src/files/XDRFile.cpp:write_gmx_compressed_floats`).
- Random access indexing for trajectories: offset pre-scan in XTC/TRR (`src/formats/XTC.cpp:determine_frame_offsets`, `src/formats/TRR.cpp:determine_frame_offsets`).

### Refactor safely: invariants + ABI notes
- `Frame` invariant: topology/positions/velocities sizes must stay aligned (`src/Frame.cpp:Frame::size`, `src/Frame.cpp:Frame::resize`, `src/Frame.cpp:Frame::set_topology`).
- Connectivity canonical ordering and uniqueness are relied on by angle/dihedral derivation (`src/Connectivity.cpp:Bond::Bond`, `src/Connectivity.cpp:Angle::Angle`, `src/Connectivity.cpp:Connectivity::recalculate`).
- `UnitCell` shape/matrix constraints are strict (negative lengths/angles invalid, determinant checks, shape conversion guards) (`src/UnitCell.cpp`).
- ABI/export boundary is explicit through generated export header and hidden symbols by default (`CMakeLists.txt:generate_export_header`, `CXX_VISIBILITY_PRESET hidden`, `VISIBILITY_INLINES_HIDDEN`).
- C API error surface depends on `CHFL_ERROR_CATCH` status mapping and thread-local last error (`include/chemfiles/capi/utils.hpp:CHFL_ERROR_CATCH`, `src/capi/misc.cpp:CAPI_LAST_ERROR`).

## 1. Repository Overview

### Directory tree (high-level)
- `include/chemfiles/`: public C++ API (`Frame`, `Trajectory`, `Topology`, `Selection`, `FormatMetadata`, etc.) and C API headers under `include/chemfiles/capi/`.
- `src/`: implementation split into `formats/`, `files/`, `selections/`, and `capi/`.
- `tests/`: C++/C API tests, format-specific tests, doctests, lints (`tests/CMakeLists.txt`).
- `examples/`: C and C++ examples (`examples/cpp/select.cpp`, `examples/README.md`).
- `external/`: vendored dependencies unpacked at configure time (`external/CMakeLists.txt`).
- `doc/`: documentation build sources (enabled with `CHFL_BUILD_DOCUMENTATION`).

### Build systems and entrypoints
- Root build is CMake (`CMakeLists.txt`) with project version from `VERSION` and options for tests/docs/system deps.
- Core build structure:
  - `chemfiles_objects` OBJECT library from `src/**.cpp`.
  - `chemfiles` main library composed from objects + external objects.
  - install/export via CMake package config under `lib/cmake/chemfiles`.
- Standards: C++17 and C99 (`target_compile_features(... cxx_std_17 c_std_99)` in `CMakeLists.txt`).

### External dependencies and management
- Compression: zlib, lzma/xz, bzip2 (system or vendored via `CHFL_SYSTEM_*` options) (`external/CMakeLists.txt`).
- Other vendored components: fmt, pugixml, tng, VMD molfiles, mmtf-cpp, optional gemmi (`CHFL_DISABLE_GEMMI`) (`external/CMakeLists.txt`).
- NetCDF3/XDR support is implemented in-repo under `src/files/Netcdf3File.cpp` and `src/files/XDRFile.cpp`.

### Language bindings (if any)
- In-tree public APIs are C++ (`include/chemfiles.hpp`) and C (`include/chemfiles.h`).
- C API bridge implementation is in `src/capi/`.
- No Python/Rust binding source directories are present in this repository tree scan; README references distribution packaging that includes Python interface externally (`README.md`).

## 2. Architecture at a Glance

- Architecture diagram: `codebase-analysis-docs/assets/architecture.mmd`
- Format dispatch diagram: `codebase-analysis-docs/assets/format-dispatch.mmd`
- Read path data-flow diagram: `codebase-analysis-docs/assets/read-path.mmd`

### End-to-end data flow
- Open flow: `Trajectory(path, mode, format)` parses mode/format/compression, optionally guesses from extension, resolves registered format, then constructs concrete format instance (`src/Trajectory.cpp:file_open_info::parse`, `src/Trajectory.cpp:Trajectory::Trajectory`, `src/guess_format.cpp`, `src/FormatFactory.cpp:FormatFactory::by_name`).
- Read flow: `Trajectory::read/read_at` creates `Frame`, delegates to format, then applies optional topology/cell override (`src/Trajectory.cpp:Trajectory::read`, `src/Trajectory.cpp:Trajectory::read_at`, `src/Trajectory.cpp:Trajectory::post_read`).
- Write flow: `Trajectory::write` validates mode, optionally clones frame for topology/cell overrides, and delegates to format writer (`src/Trajectory.cpp:Trajectory::write`).

### Cross-cutting concerns
- Error model: C++ exceptions (`FileError`, `FormatError`, etc.) and C API status mapping (`include/chemfiles/capi/utils.hpp:CHFL_ERROR_CATCH`).
- Units/conventions: internal positions in Angstrom, velocities in Angstrom/ps (`include/chemfiles/Frame.hpp`); format adapters convert as needed (XTC/TRR/TPR use nm scaling in format code).
- Portability: explicit endianness handling and conditional mmap/stdio paths (`src/files/BinaryFile.cpp`, `include/chemfiles/files/BinaryFile.hpp`).
- Performance: buffered text I/O (`src/File.cpp`), mmap windowing (`src/files/BinaryFile.cpp`), and indexed random access in some binary formats (`src/formats/XTC.cpp`, `src/formats/TRR.cpp`, `src/formats/DCD.cpp`).

## 3. Core Data Model

### Frame
- `Frame` owns topology, positions, optional velocities, unit cell, index, and property map (`include/chemfiles/Frame.hpp`).
- Units:
  - positions: Angstrom (`Frame::positions` docs)
  - velocities: Angstrom/ps (`Frame::velocities` docs)
  - `distance` in Angstrom, angle/dihedral geometry functions over wrapped vectors (`src/Frame.cpp:Frame::distance`, `src/Frame.cpp:Frame::angle`, `src/Frame.cpp:Frame::dihedral`, `src/Frame.cpp:Frame::out_of_plane`).
- Size alignment invariant enforced by internal asserts and checked operations (`src/Frame.cpp:Frame::size`, `src/Frame.cpp:Frame::set_topology`, `src/Frame.cpp:Frame::resize`).
- Bond inference uses VdW radii + distance heuristic and post-filters problematic H-H links (`src/Frame.cpp:Frame::guess_bonds`).

### Topology
- `Topology` stores atoms, connectivity, residues, and residue mapping (`include/chemfiles/Topology.hpp`, `src/Topology.cpp`).
- Index safety checks are explicit in bond add/remove/order lookup (`src/Topology.cpp:Topology::add_bond`, `src/Topology.cpp:Topology::remove_bond`, `src/Topology.cpp:Topology::bond_order`).
- Residue membership is unique per atom (`src/Topology.cpp:add_residue`).

### Connectivity semantics
- `Bond`, `Angle`, `Dihedral`, `Improper` normalize ordering and reject invalid repeated atoms (`src/Connectivity.cpp` constructors).
- Derived angles/dihedrals/impropers are lazy-recomputed from bonds when stale (`src/Connectivity.cpp:Connectivity::angles`, `src/Connectivity.cpp:Connectivity::dihedrals`, `src/Connectivity.cpp:Connectivity::impropers`, `src/Connectivity.cpp:Connectivity::recalculate`).
- Bond orders are kept index-aligned with sorted bond container (`src/Connectivity.cpp:Connectivity::add_bond`, `src/Connectivity.cpp:Connectivity::remove_bond`, `src/Connectivity.cpp:Connectivity::bond_order`).

### UnitCell
- Shapes: `ORTHORHOMBIC`, `TRICLINIC`, `INFINITE` (`include/chemfiles/UnitCell.hpp`).
- Length/angle validation and matrix determinant constraints are enforced (`src/UnitCell.cpp:check_lengths`, `src/UnitCell.cpp:check_angles`, `src/UnitCell.cpp:UnitCell::UnitCell(Matrix3D)`).
- Wrapping uses shape-specific behavior (`src/UnitCell.cpp:UnitCell::wrap`).
- Lengths are Angstrom, angles are degrees at API boundary (`include/chemfiles/UnitCell.hpp`).

### Property/metadata system
- `Property` is a tagged union (`BOOL`, `DOUBLE`, `STRING`, `VECTOR3D`) with strict typed accessors (`include/chemfiles/Property.hpp`, `src/Property.cpp`).
- `property_map::get<kind>` returns `nullopt` on absent keys and warns on type mismatch (`src/Property.cpp:property_map::get`).
- Format capability metadata is represented by `FormatMetadata` booleans (`positions`, `velocities`, `unit_cell`, `atoms`, `bonds`, `residues`) (`include/chemfiles/FormatMetadata.hpp`).

### Selection/query system
- Contexts: atom/pair/three/four/bond/angle/dihedral (`include/chemfiles/Selection.hpp:Context`).
- Evaluation strategy:
  - atom/pair/three/four contexts iterate combinatorially (`src/Selection.cpp:evaluate_atoms`, `evaluate_pairs`, `evaluate_three`, `evaluate_four`)
  - bond/angle/dihedral contexts iterate topology-derived connectivity (`src/Selection.cpp:evaluate_bonds`, `evaluate_angles`, `evaluate_dihedrals`).
- Parser provides math and geometry functions (`distance`, `angle`, `dihedral`, `out_of_plane`) (`src/selections/parser.cpp`).

## 4. Format & I/O System

### Format registry and dispatch
- Registry singleton lives in `FormatFactory` (`src/FormatFactory.cpp:FormatFactory::get`).
- Registration validates metadata and enforces unique name/extension (`src/FormatFactory.cpp:register_format`, `src/FormatMetadata.cpp:FormatMetadata::validate`).
- Format guess uses extension and optional compression suffix; `.cif` can be sniffed into `.mmcif` by scanning tags on read/append (`src/guess_format.cpp`).
- Supported built-in formats are explicitly registered in constructor (`src/FormatFactory.cpp:FormatFactory::FormatFactory`).

### Reader/Writer abstraction
- Base interface: `Format::read_at`, `read`, `write`, `size` (`include/chemfiles/Format.hpp`).
- Default unimplemented methods throw format errors (`src/Format.cpp:Format::read_at`, `src/Format.cpp:Format::read`, `src/Format.cpp:Format::write`).
- `TextFormat` adds `forward()`-based indexing with frame position cache (`src/Format.cpp:TextFormat::scan_all`, `src/Format.cpp:TextFormat::read_at`).
- `Trajectory` enforces mode correctness and frame bounds before delegating (`src/Trajectory.cpp:Trajectory::pre_read`, `src/Trajectory.cpp:Trajectory::write`, `src/Trajectory.cpp:Trajectory::check_opened`).

### Shared I/O utilities
- `TextFile` supports plain/gzip/bzip2/xz + memory-backed stream with buffering (`src/File.cpp:TextFile::TextFile`, `src/File.cpp:TextFile::readline`).
- `BinaryFile` provides typed read/write for big/little-endian subclasses with mmap (POSIX) or stdio fallback (`include/chemfiles/files/BinaryFile.hpp`, `src/files/BinaryFile.cpp`).
- `XDRFile` layers GROMACS-specific opaque/string/compressed-float support over big-endian binary I/O (`include/chemfiles/files/XDRFile.hpp`, `src/files/XDRFile.cpp`).
- `Netcdf3File` is an internal NetCDF3 reader/writer with dimensions/variables/record layout/padding logic (`src/files/Netcdf3File.cpp`).
- `TNGFile` wraps TNG C API with RAII and status-to-exception conversion (`include/chemfiles/files/TNGFile.hpp`, `src/files/TNGFile.cpp`).

### Per-format responsibilities (selected major formats)
- `PDB` (`src/formats/PDB.cpp`): positions, unit cell, atoms, bonds, residues; record-level parsing/writing for `ATOM/HETATM`, `CRYST1`, `CONECT`, `TER`, `MODEL/ENDMDL`, `END` (`src/formats/PDB.cpp:get_record`, `src/formats/PDB.cpp:read_ATOM`, `src/formats/PDB.cpp:read_CRYST1`, `src/formats/PDB.cpp:read_CONECT`, `src/formats/PDB.cpp:PDBFormat::write`).
- `XYZ` (`src/formats/XYZ.cpp`): positions, optional velocities, atoms, optional unit cell via comment conventions; text `forward()` indexing.
- `GRO` (`src/formats/GRO.cpp`): positions/velocities/unit cell/residues with width checks and nm-scale handling in reader/writer.
- `DCD` (`src/formats/DCD.cpp`): binary trajectory positions + unit cell; endianness/record marker detection and index-based random seek.
- `XTC` (`src/formats/XTC.cpp`): compressed positions + time + unit cell; nm↔Angstrom conversions and frame-offset indexing.
- `TRR` (`src/formats/TRR.cpp`): positions/velocities/forces + cell + time; nm conversions and frame-offset indexing.
- `Amber NetCDF/Restart` (`src/formats/AmberNetCDF.cpp`): coordinates/velocities/cell/time with unit-attribute scaling and strict variable/dimension validation.
- `TPR` (`src/formats/TPR.cpp`): read-only topology + coordinates/velocities/cell from GROMACS input; nm scaling.
- `TNG` (`src/formats/TNG.cpp`): read-only in chemfiles metadata; reads positions/velocities/cell/step/time via TNG wrapper.
- `Molfile` adapters (`src/formats/Molfile.cpp`): plugin-backed TRJ/PSF/Molden are read-only in metadata.

### Representative use paths (from tests/examples)
1. Iterative trajectory read:
   - `Trajectory(path).read()` loop (`tests/trajectory.cpp` basic read cases).
   - Call flow: `Trajectory::read` → `Format::read`/`TextFormat::read` → format parser fills `Frame` → `Trajectory::post_read` applies overrides (`src/Trajectory.cpp`, `src/Format.cpp`, `src/formats/*.cpp`).
2. Random-access read:
   - `Trajectory::read_at(index)` in tests (`tests/trajectory.cpp`, `tests/formats/xtc.cpp`, `tests/formats/pdb.cpp`, `tests/formats/amber-netcdf.cpp`).
   - Indexed backend examples:
     - text: `TextFormat::read_at` seeks cached offsets (`src/Format.cpp`),
     - XTC/TRR: precomputed frame offsets (`src/formats/XTC.cpp:determine_frame_offsets`, `src/formats/TRR.cpp:determine_frame_offsets`),
     - DCD: header/frame-size based seek math (`src/formats/DCD.cpp:DCDFormat::read_at`).
3. Trajectory write/append:
   - `Trajectory(tmp, 'w').write(frame)` and append paths (`tests/trajectory.cpp`, `tests/formats/dcd.cpp`, `tests/formats/xtc.cpp`).
   - Call flow: `Trajectory::write` mode checks → format `write` implementation → size/index update in trajectory (`src/Trajectory.cpp:Trajectory::write`).
4. Topology/cell override workflow:
   - `Trajectory::set_topology(...)` / `set_cell(...)` before read/write (`tests/trajectory.cpp`).
   - Override injection point is centralized in `Trajectory::post_read` and `Trajectory::write` clone path (`src/Trajectory.cpp`).
5. Selection/query workflow:
   - `Selection(...).evaluate(frame)` and context-specific queries (`tests/selection.cpp`, `examples/cpp/select.cpp`).
   - Call flow: parse string to AST (`src/selections/lexer.cpp`, `src/selections/parser.cpp`) → evaluate over frame/topology contexts (`src/Selection.cpp`).

## 5. Subsystem Maps (one subsection per subsystem)

### 5.1 Public API / ABI Boundary
- Purpose: stable C++ and C entry surfaces.
- Key files: `include/chemfiles.hpp`, `include/chemfiles.h`, `include/chemfiles/capi/*.h`, `src/capi/*.cpp`, `CMakeLists.txt`.
- Key symbols: `chemfiles::Trajectory`, `chemfiles::Frame`, C API `chfl_*` functions, `CHFL_EXPORT`.
- Public vs internal: public headers under `include/chemfiles`; implementation mostly under `src/`.
- Invariants: C API statuses map exceptions via `CHFL_ERROR_CATCH`; last error is thread-local (`include/chemfiles/capi/utils.hpp`, `src/capi/misc.cpp`).
- Performance notes: C API allocates shared-owned arrays for metadata export (`src/capi/misc.cpp:chfl_formats_list`).
- Common pitfalls: forgetting export annotations or changing data layout assumptions (for example `Vector3D` standard layout in `include/chemfiles/types.hpp`).
- Extension points: add C API wrappers in `src/capi/` once C++ API additions stabilize.

### 5.2 Dispatch & Registry
- Purpose: map names/extensions to concrete format constructors.
- Key files: `include/chemfiles/FormatFactory.hpp`, `src/FormatFactory.cpp`, `src/guess_format.cpp`, `include/chemfiles/FormatMetadata.hpp`.
- Key symbols: `FormatFactory::by_name`, `by_extension`, `register_format`, `format_metadata<T>()`, `guess_format`.
- Public vs internal: external callers only see format names/metadata (`formats_list`); registry internals are private.
- Invariants: unique format name/extension, metadata fields validated (`src/FormatFactory.cpp`, `src/FormatMetadata.cpp`).
- Performance notes: lookups are linear over registered formats; list is small enough to keep simple.
- Common pitfalls: extension collisions and metadata trimming/URL validation failures at startup.
- Extension points: register new format in constructor and add metadata specialization.

### 5.3 Text I/O Stack
- Purpose: efficient line-oriented parsing/writing for text formats.
- Key files: `include/chemfiles/File.hpp`, `src/File.cpp`, `include/chemfiles/Format.hpp`, `src/Format.cpp`, text format files in `src/formats/*.cpp`.
- Key symbols: `TextFile::readline`, `TextFormat::forward`, `TextFormat::scan_all`, `TextFormat::read_at`.
- Public vs internal: public users see `Trajectory`; text internals are format-private.
- Invariants: `forward()` must return consistent frame start offsets; cached positions drive `read_at`.
- Performance notes: buffer starts at 8192 bytes and grows on long lines; uses memmove + refill loop (`src/File.cpp:TextFile::readline`).
- Common pitfalls: append+gzip special-case in `scan_all`; line ending handling (`\r\n`) in reader.
- Extension points: implement `forward/read_next/write_next` in new `TextFormat` subclasses.

### 5.4 Binary I/O Stack
- Purpose: typed binary reads/writes, endianness conversion, compressed coordinate decoding.
- Key files: `include/chemfiles/files/BinaryFile.hpp`, `src/files/BinaryFile.cpp`, `include/chemfiles/files/XDRFile.hpp`, `src/files/XDRFile.cpp`, `src/files/Netcdf3File.cpp`, `src/files/TNGFile.cpp`.
- Key symbols: `BigEndianFile`, `LittleEndianFile`, `XDRFile::read_gmx_compressed_floats`, `Netcdf3File::add_record`.
- Public vs internal: completely internal behind format implementations.
- Invariants: record markers/offsets/types must match format assumptions (DCD/XTC/TRR/NetCDF readers enforce this with throws).
- Performance notes: mmap window is capped (100 MiB) and remapped as cursor advances (`src/files/BinaryFile.cpp`); compressed XDR paths reuse buffers.
- Common pitfalls: large-file boundary checks, platform-specific behavior when mmap unavailable.
- Extension points: add helpers for new binary encodings under `src/files/` before format-level code.

### 5.5 Core Chemistry Model
- Purpose: canonical representation of atoms/connectivity/residues/cell/properties.
- Key files: `include/chemfiles/Frame.hpp`, `src/Frame.cpp`, `include/chemfiles/Topology.hpp`, `src/Topology.cpp`, `include/chemfiles/Connectivity.hpp`, `src/Connectivity.cpp`, `include/chemfiles/UnitCell.hpp`, `src/UnitCell.cpp`, `include/chemfiles/Property.hpp`, `src/Property.cpp`.
- Key symbols: `Frame::set_topology`, `Frame::guess_bonds`, `Topology::add_residue`, `Connectivity::recalculate`, `UnitCell::set_shape`, `property_map::get`.
- Public vs internal: classes are public; helper algorithms are internal.
- Invariants: no duplicate atoms per bond/angle/dihedral/improper; residue atom uniqueness; cell shape consistency.
- Performance notes: lazy derived connectivity avoids recomputation until requested.
- Common pitfalls: topology-frame size mismatches; invalid residue mappings after atom removals.
- Extension points: add new per-atom/residue/frame properties using `property_map` and format adapters.

### 5.6 Selection Engine
- Purpose: parse and evaluate declarative atom/topology queries.
- Key files: `include/chemfiles/Selection.hpp`, `src/Selection.cpp`, `src/selections/lexer.cpp`, `src/selections/parser.cpp`, `src/selections/NumericValues.cpp`.
- Key symbols: `Selection::evaluate`, `Tokenizer::tokenize`, `Parser::parse`.
- Public vs internal: `Selection`/`Match` are public; AST/tokenizer internals stay private.
- Invariants: context variable bounds (`#1..#4`) and selection grammar constraints enforced at parse time.
- Performance notes: `PAIR/THREE/FOUR` contexts are O(N^2/3/4) brute-force loops (`src/Selection.cpp`).
- Common pitfalls: complex selections can become combinatorially expensive; parser function map currently wires `"log"` to `sqrt` callable (`src/selections/parser.cpp` numeric function table).
- Extension points: add selectors/functions in parser tables and corresponding AST nodes.

### 5.7 Format Implementations
- Purpose: encode/decode individual chemistry formats into `Frame` model.
- Key files: `src/formats/*.cpp` plus matching headers in `include/chemfiles/formats/`.
- Key symbols: `format_metadata<...>()`, format class `read/read_at/write/size` implementations.
- Public vs internal: internal; exposed through registry names only.
- Invariants: each format must keep frame atom count consistency and map units to internal conventions.
- Performance notes: many binary formats build offset indices once for fast `read_at` (XTC/TRR); DCD uses fixed frame-size arithmetic.
- Common pitfalls: optional fields (time/cell/topology) vary by format and need clear defaults.
- Extension points: add new format source + metadata + registration + tests.

### 5.8 Build, Tests, and CI
- Purpose: portability and regression safety.
- Key files: `CMakeLists.txt`, `external/CMakeLists.txt`, `tests/CMakeLists.txt`, `.github/workflows/tests.yml`, `.github/workflows/docs.yml`.
- Key symbols: CMake options (`CHFL_BUILD_TESTS`, `CHFL_SYSTEM_*`, `CHFL_DISABLE_GEMMI`, `CHFL_TESTS_USE_VALGRIND`).
- Public vs internal: internal project tooling.
- Invariants: tests rely on pinned external test-data commit/hash (`tests/CMakeLists.txt`).
- Performance notes: CI runs a broad matrix including valgrind and 32-bit build variants.
- Common pitfalls: local tests require network download of tests-data unless already cached in build dir.
- Extension points: add format regression tests in `tests/formats/` and CI covers them automatically.

## 6. Conventions, Units, and Correctness

### Units policy and conversion points
- Internal model:
  - coordinates in Angstrom (`include/chemfiles/Frame.hpp:positions` docs)
  - velocities in Angstrom/ps (`include/chemfiles/Frame.hpp:velocities` docs)
  - unit-cell lengths in Angstrom and angles in degrees (`include/chemfiles/UnitCell.hpp`).
- Geometry APIs:
  - `Frame::distance` returns Angstrom (`include/chemfiles/Frame.hpp` docs)
  - angular geometry functions operate in radians (selection uses `deg2rad/rad2deg` utilities in parser; tests validate angle comparisons in radians) (`src/selections/parser.cpp`, `tests/selection.cpp`).
- Conversion examples:
  - XTC/TRR/TPR convert nm to Angstrom with factor 10 on read and inverse on write where supported (`src/formats/XTC.cpp`, `src/formats/TRR.cpp`, `src/formats/TPR.cpp`).
  - TNG computes distance scale factor to Angstrom from TNG exponent metadata (`src/formats/TNG.cpp`).
  - Amber NetCDF scales distance/velocity/time from units attributes and `scale_factor` attributes (`src/formats/AmberNetCDF.cpp:scale_for_distance`, `src/formats/AmberNetCDF.cpp:scale_for_velocity`, `src/formats/AmberNetCDF.cpp:scale_for_time`, `src/formats/AmberNetCDF.cpp:AmberNetCDFBase::get_variable`).

### Format-specific conventions
- PDB:
  - fixed-column extraction via `substr` offsets for coordinates, residue ids, altloc, chain ids, etc. (`src/formats/PDB.cpp:read_ATOM`, `src/formats/PDB.cpp:read_CRYST1`, `src/formats/PDB.cpp:read_CONECT`).
  - hybrid36 is used for serial/residue fields (`src/formats/PDB.cpp` call sites, `src/parse.cpp:decode_hybrid36`, `src/parse.cpp:encode_hybrid36`).
  - parser handles `TER`, `MODEL/ENDMDL`, `END` variations and warnings for malformed records (`src/formats/PDB.cpp`).
- Binary formats:
  - DCD validates Fortran record markers and derives endianness/marker width from header checks (`src/formats/DCD.cpp:DCDFormat::read_header`, `src/formats/DCD.cpp:DCDFormat::read_marker`).
  - XDR-based formats use explicit opaque/string/packed-float routines (`src/files/XDRFile.cpp`).

### Robustness strategy
- Invalid structural assumptions throw typed errors (for example out-of-bounds atom indexes, invalid cell angles, malformed record sections).
- Non-fatal irregularities may emit warnings and continue (for example some PDB quirks, UnitCell warning on suspicious zero lengths).
- File mode and state checks are centralized in `Trajectory` (`src/Trajectory.cpp:Trajectory::pre_read`, `src/Trajectory.cpp:Trajectory::check_opened`, `src/Trajectory.cpp:Trajectory::write`).

### Determinism and reproducibility
- Canonical ordering in connectivity types (`Bond/Angle/Dihedral/Improper`) makes bond graph derivatives deterministic for same inputs (`src/Connectivity.cpp`).
- Some binary writers involve quantization/compression (XTC/TRR), so roundtrips are format-precision dependent (validated in format tests such as `tests/formats/xtc.cpp`).

## 7. Performance Model

### Where performance matters
- Text parsing loop: repeated `readline()` + per-line parsing in text formats (`src/File.cpp`, `src/formats/PDB.cpp`, `src/formats/XYZ.cpp`, `src/formats/GRO.cpp`).
- Binary decode loops over all atoms/coordinates (`src/formats/XTC.cpp`, `src/formats/TRR.cpp`, `src/formats/DCD.cpp`, `src/formats/AmberNetCDF.cpp`).
- Offset indexing scans for random access (`src/formats/XTC.cpp:determine_frame_offsets`, `src/formats/TRR.cpp:determine_frame_offsets`).

### Memory ownership and move/copy patterns
- `Trajectory::write` clones frame only when topology/cell overrides are active, avoiding unnecessary copies otherwise (`src/Trajectory.cpp:Trajectory::write`).
- `Frame` intentionally hides mutable topology reference to preserve storage consistency (`include/chemfiles/Frame.hpp` comments).
- `NumericValues` in selections uses malloc/realloc growth strategy for numeric expression buffers (`src/selections/NumericValues.cpp`).

### Profiling hooks or benchmark harness
- No dedicated benchmark directory/harness was found in this repo layout.
- Practical profiling targets are test executables and real trajectory workloads (`tests/CMakeLists.txt`, `tests/formats/*`).

### Practical optimization tips grounded in code
- Prefer `read_at` for indexed formats that precompute offsets (XTC/TRR/DCD) instead of rescanning.
- Avoid repeated `Trajectory::size()` calls in tight loops for text formats where full scan may be required (`include/chemfiles/Format.hpp:size` comment, `src/Format.cpp:scan_all`).
- Reuse `Frame`/buffers in writer loops and avoid property churn when not needed.
- For C API-heavy loops, minimize cross-boundary calls and fetch bulk data where possible.

## 8. Build, Test, Bench, and Tooling

### How to build + run tests
- Typical local flow from docs:
  - `cmake ..`
  - `make`
  - `cmake -DCHFL_BUILD_TESTS=ON .`
  - `ctest`
  (from `Contributing.md`, `README.md`).
- CMake options include dependency toggles, tests, docs, warnings, and optional gemmi disable (`CMakeLists.txt`, `external/CMakeLists.txt`).

### Format test coverage strategy
- Format-specific checks are under `tests/formats/` (e.g., XTC, DCD, PDB, Amber NetCDF).
- Integration behavior lives in `tests/trajectory.cpp` (read/read_at/write, topology/cell overrides, compression parsing, threading).
- Selection language is extensively exercised in `tests/selection.cpp`.

### Fuzzing or corpus tests
- No dedicated fuzz target was found.
- Regression corpus is test-data downloaded by pinned commit/hash (`tests/CMakeLists.txt:TESTS_DATA_GIT`, `file(DOWNLOAD ...)`).

### CI notes and platform caveats
- CI matrix covers Linux/macOS/Windows, gcc/clang/MSVC/MinGW, static/shared variants, system deps, 32-bit build, valgrind, older toolchain container (`.github/workflows/tests.yml`).
- Documentation job builds `doc_html` on Ubuntu (`.github/workflows/docs.yml`).

## 9. Change Playbooks (Practical Guides)

### Add a new text format reader/writer
1. Create new format class deriving `TextFormat` and implement `forward()` plus `read_next` and/or `write_next` (`include/chemfiles/Format.hpp`).
2. Add `format_metadata<YourFormat>()` with capabilities and extension.
3. Register in `FormatFactory::FormatFactory()`.
4. Add tests in `tests/formats/` and integration checks in `tests/trajectory.cpp`.
5. Validate random access semantics (`read_at`) via `forward` offsets.

### Add a new binary format reader/writer
1. Reuse `BinaryFile`, `BigEndianFile`/`LittleEndianFile`, and optionally `XDRFile`/`Netcdf3File` helpers (`include/chemfiles/files/*`).
2. Implement strict header/record validation early in constructor/read path.
3. Decide indexing strategy: pre-scan frame offsets (XTC/TRR) or fixed-size seek arithmetic (DCD).
4. Register metadata capabilities carefully (`write`/`memory` flags especially).
5. Add roundtrip and malformed-input tests.

### Add a new property metadata field end-to-end
1. Add producer/consumer points in format code (frame, atom, residue, topology property maps).
2. Use `Property` typed APIs and avoid silent type reinterpretation (`src/Property.cpp`).
3. Extend tests to verify both C++ and C API exposure if relevant.

### Add/modify selection language features
1. Lexer changes in `src/selections/lexer.cpp` if new tokens are needed.
2. Parser rule/function-table updates in `src/selections/parser.cpp`.
3. AST/evaluation behavior validation via `Selection::evaluate` contexts in `src/Selection.cpp`.
4. Add focused tests in `tests/selection.cpp` covering context constraints and error messages.

### Refactor I/O utilities safely
1. Preserve `TextFile::readline` semantics around EOF, CRLF trimming, and buffer growth (`src/File.cpp`).
2. Preserve mmap/non-mmap parity in `BinaryFile` behavior (`src/files/BinaryFile.cpp`).
3. Keep exception types and messages stable where tests/assertions rely on them.
4. Re-run wide format suite (`tests/formats/*`) and trajectory integration tests.

## 10. Index: Symbols → Files

- `chemfiles::Trajectory` → `include/chemfiles/Trajectory.hpp`, `src/Trajectory.cpp`.
- `guess_format` → `src/guess_format.cpp`.
- `chemfiles::Format` / `chemfiles::TextFormat` → `include/chemfiles/Format.hpp`, `src/Format.cpp`.
- `chemfiles::FormatFactory` → `include/chemfiles/FormatFactory.hpp`, `src/FormatFactory.cpp`.
- `chemfiles::FormatMetadata` → `include/chemfiles/FormatMetadata.hpp`, `src/FormatMetadata.cpp`.
- `chemfiles::TextFile` → `include/chemfiles/File.hpp`, `src/File.cpp`.
- `chemfiles::BinaryFile`, `BigEndianFile`, `LittleEndianFile` → `include/chemfiles/files/BinaryFile.hpp`, `src/files/BinaryFile.cpp`.
- `chemfiles::XDRFile` → `include/chemfiles/files/XDRFile.hpp`, `src/files/XDRFile.cpp`.
- `chemfiles::netcdf3::Netcdf3File` / `Netcdf3Builder` → `include/chemfiles/files/Netcdf3File.hpp`, `src/files/Netcdf3File.cpp`.
- `chemfiles::TNGFile` → `include/chemfiles/files/TNGFile.hpp`, `src/files/TNGFile.cpp`.
- `chemfiles::Frame` → `include/chemfiles/Frame.hpp`, `src/Frame.cpp`.
- `chemfiles::Topology` → `include/chemfiles/Topology.hpp`, `src/Topology.cpp`.
- `chemfiles::Bond` / `Angle` / `Dihedral` / `Improper` / `Connectivity` → `include/chemfiles/Connectivity.hpp`, `src/Connectivity.cpp`.
- `chemfiles::UnitCell` → `include/chemfiles/UnitCell.hpp`, `src/UnitCell.cpp`.
- `chemfiles::Property` / `property_map` → `include/chemfiles/Property.hpp`, `src/Property.cpp`.
- `chemfiles::Selection` / `Match` → `include/chemfiles/Selection.hpp`, `src/Selection.cpp`.
- Selection tokenizer/parser internals → `src/selections/lexer.cpp`, `src/selections/parser.cpp`, `src/selections/NumericValues.cpp`.
- C API error bridge (`CHFL_ERROR_CATCH`, `chfl_last_error`) → `include/chemfiles/capi/utils.hpp`, `src/capi/misc.cpp`.
- Major formats:
  - `PDBFormat` → `src/formats/PDB.cpp`
  - `XYZFormat` → `src/formats/XYZ.cpp`
  - `GROFormat` → `src/formats/GRO.cpp`
  - `DCDFormat` → `src/formats/DCD.cpp`
  - `XTCFormat` → `src/formats/XTC.cpp`
  - `TRRFormat` → `src/formats/TRR.cpp`
  - `TPRFormat` → `src/formats/TPR.cpp`
  - `TNGFormat` → `src/formats/TNG.cpp`
  - `AmberTrajectory` / `AmberRestart` → `src/formats/AmberNetCDF.cpp`
  - `Molfile<TRJ/PSF/MOLDEN>` → `src/formats/Molfile.cpp`

## Appendix A: Files Read

- `CMakeLists.txt`
- `external/CMakeLists.txt`
- `tests/CMakeLists.txt`
- `.github/workflows/tests.yml`
- `.github/workflows/docs.yml`
- `README.md`
- `Contributing.md`
- `include/chemfiles.hpp`
- `include/chemfiles.h`
- `include/chemfiles/Trajectory.hpp`
- `src/Trajectory.cpp`
- `include/chemfiles/Format.hpp`
- `src/Format.cpp`
- `include/chemfiles/FormatFactory.hpp`
- `src/FormatFactory.cpp`
- `include/chemfiles/FormatMetadata.hpp`
- `src/FormatMetadata.cpp`
- `src/guess_format.cpp`
- `include/chemfiles/File.hpp`
- `src/File.cpp`
- `include/chemfiles/files/BinaryFile.hpp`
- `src/files/BinaryFile.cpp`
- `include/chemfiles/files/XDRFile.hpp`
- `src/files/XDRFile.cpp`
- `include/chemfiles/files/Netcdf3File.hpp`
- `src/files/Netcdf3File.cpp`
- `include/chemfiles/files/TNGFile.hpp`
- `src/files/TNGFile.cpp`
- `include/chemfiles/Frame.hpp`
- `src/Frame.cpp`
- `include/chemfiles/Topology.hpp`
- `src/Topology.cpp`
- `include/chemfiles/Connectivity.hpp`
- `src/Connectivity.cpp`
- `include/chemfiles/UnitCell.hpp`
- `src/UnitCell.cpp`
- `include/chemfiles/Property.hpp`
- `src/Property.cpp`
- `include/chemfiles/types.hpp`
- `include/chemfiles/parse.hpp`
- `src/parse.cpp`
- `include/chemfiles/Selection.hpp`
- `src/Selection.cpp`
- `include/chemfiles/selections/NumericValues.hpp`
- `src/selections/NumericValues.cpp`
- `src/selections/lexer.cpp`
- `src/selections/parser.cpp`
- `include/chemfiles/capi/types.h`
- `include/chemfiles/capi/utils.hpp`
- `include/chemfiles/capi/shared_allocator.hpp`
- `src/capi/misc.cpp`
- `src/formats/XYZ.cpp`
- `src/formats/PDB.cpp`
- `src/formats/GRO.cpp`
- `src/formats/DCD.cpp`
- `src/formats/XTC.cpp`
- `src/formats/TRR.cpp`
- `src/formats/AmberNetCDF.cpp`
- `src/formats/TPR.cpp`
- `src/formats/TNG.cpp`
- `src/formats/Molfile.cpp`
- `src/formats/CIF.cpp`
- `src/formats/mmCIF.cpp`
- `src/formats/LAMMPSTrajectory.cpp`
- `src/formats/LAMMPSData.cpp`
- `src/formats/CML.cpp`
- `src/formats/CSSR.cpp`
- `src/formats/MMTF.cpp`
- `src/formats/MOL2.cpp`
- `src/formats/SDF.cpp`
- `src/formats/SMI.cpp`
- `src/formats/Tinker.cpp`
- `tests/trajectory.cpp`
- `tests/selection.cpp`
- `tests/formats/xtc.cpp`
- `tests/formats/dcd.cpp`
- `tests/formats/pdb.cpp`
- `tests/formats/amber-netcdf.cpp`
- `examples/README.md`
- `examples/cpp/select.cpp`
