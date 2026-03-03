# Nanobind Python Bindings Plan (26-03-03)

## 1) Problem Framing

### Goal
Add Python bindings for `watpocket` using `nanobind`, packaged via `scikit-build-core`, while preserving existing CMake/CTest workflows.

### In Scope
- Add `pyproject.toml` with `scikit-build-core` backend.
- Build a single extension module (module name: `watpocket`).
- Support Python `>=3.10`.
- Expose all currently exported C++ API functions except callback plumbing for trajectory analysis.
- Add at least one Python integration test using WCN data with `complex_wcn.parm7 + 1.nc`.
- Reuse CTest to run Python integration tests.

### Out of Scope
- Removing callback support from C++ core in this change (assume this will happen separately).
- Designing a multi-module Python package layout.
- Publishing to PyPI/conda.
- New trajectory formats beyond existing `.nc` flow.

### Assumptions
- Linux/macOS build environments are primary targets.
- Existing CMake project remains the source of truth for native builds.
- Python tests may rely on `pytest` in dev/test environment.
- Existing WCN fixtures remain available in `test/data/wcn`.
- Initial binding mode targets CPython ABI (no `STABLE_ABI` in first iteration) to keep Python 3.10+ coverage simple.

## 2) Success Criteria (Measurable)

### Correctness
- Python API returns equivalent results to C++ API for representative calls:
  - selector parsing
  - structure/trajectory point extraction
  - structure and trajectory analysis
  - draw writers
- Python exceptions are raised on C++ `watpocket::Error`.
- WCN integration test on `1.nc` passes and CSV exactly matches benchmark `test/data/wcn/1.nc.csv`.

### Performance
- No regression to existing CLI/integration test runtime greater than 10% due to Python build hooks.
- Python integration test completes in under 2 seconds on current fixture size.

### Resource
- No additional large fixture files required.
- Python wheel build should not duplicate core algorithms in separate code paths.

### Portability
- `pip install .` works for Python `>=3.10` on supported dev platform(s).
- Existing C++ CLI build (`watpocket` target) remains functional.

### Maintainability
- Binding source isolated under a dedicated module directory.
- C++ API-to-Python mapping documented.
- Python tests are registered in CTest (single test entrypoint).

## 3) Architecture Sketch

### Modules and Responsibilities
- `watpocket_lib` (existing): core C++ analysis engine and exported API.
- `python extension` (new, nanobind): thin bindings over exported API.
- `scikit-build-core` (new packaging backend): drives CMake build for wheel/sdist.
- `pytest integration` (new): end-to-end Python validation using WCN fixtures.

### Data Flow
- Python call -> nanobind wrapper -> exported C++ API -> existing chemfiles/CGAL pipeline -> Python-visible return values or files.

### Nanobind Features and Conventions
- Keep module naming consistent:
  - CMake target: `nanobind_add_module(watpocket ...)`
  - Entry macro: `NB_MODULE(watpocket, m)`
  - Python import: `import watpocket`
- Use nanobind CMake discovery best practice:
  - `execute_process(COMMAND "${Python_EXECUTABLE}" -m nanobind --cmake_dir ...)`
  - `find_package(nanobind CONFIG REQUIRED)`
- Use explicit nanobind STL casters in binding source for used types (string, vector, optional, array, pair, tuple).
- Bind exception as module-native Python exception (`watpocket.Error`) via nanobind exception support.
- For non-owning view-returning APIs (`PointSoAView`, `PointSoAMutableView`), use nanobind lifetime policies (`reference_internal`/`keep_alive`) so view objects do not outlive owners.
- Generate `.pyi` stubs with `nanobind_add_stub(...)` (or `python -m nanobind.stubgen`) and treat stub import/signature validity as a CI check.
- Keep a single extension module (no `NB_SHARED`/cross-module domain complexity in first iteration).

### Binding Boundary
- Bind public types used by exported functions:
  - `ResidueSelector`, `PointSoA`, `PointSoAView`, `PointSoAMutableView`
  - `HullGeometry`, `PdbAtomRecord`
  - `StructureAnalysisResult`, `TrajectoryFrameResult`, `TrajectoryWaterPresence`, `TrajectorySummary`
- Bind functions:
  - `build_version`
  - `parse_residue_selectors`
  - `read_structure_points_by_atom_indices`
  - `read_trajectory_points_by_atom_indices`
  - `analyze_structure_file`
  - `analyze_trajectory_files` (without exposing callback arguments)
  - `write_pymol_draw_script`
  - `write_hull_pdb`

### Error Handling
- Translate `watpocket::Error` into a dedicated Python exception type (e.g., `watpocket.Error`) mapped from the C++ class.

## 4) Work Breakdown Structure (WBS)

### Phase 0: Packaging Scaffold
1. Add Python packaging metadata.
   - Changes:
     - `pyproject.toml` with `scikit-build-core`, `nanobind`, Python `>=3.10`.
   - Validation:
     - `pip wheel .` (or `python -m build`) produces wheel.
   - Exit criteria:
     - Wheel metadata reports correct module and Python requirements.

2. Add CMake toggle/target for Python bindings.
   - Changes:
     - Top-level and/or dedicated CMake file for extension target (e.g., `src/python/CMakeLists.txt`).
     - `find_package(Python3 COMPONENTS Interpreter Development.Module REQUIRED)`.
     - nanobind discovery (`python -m nanobind --cmake_dir`, then `find_package(nanobind CONFIG REQUIRED)`).
     - `nanobind_add_module(watpocket ...)` target linking to `watpocket_lib`.
     - keep default nanobind ABI mode initially (no `STABLE_ABI` in phase 1 due Python 3.10 baseline).
   - Validation:
     - CMake configure succeeds with Python enabled.
   - Exit criteria:
     - Shared extension module is produced by build.

### Phase 1: Binding Implementation
1. Create nanobind module source.
   - Changes:
     - New file (e.g., `src/python/bindings.cpp`).
     - `NB_MODULE(watpocket, m)` with explicit argument names (`"arg"_a`) on public functions.
     - include required nanobind caster headers for all bound STL/standard types.
     - avoid exposing raw pointer buffer methods (`x_data/y_data/z_data`) in Python surface; expose safe high-level APIs only.
   - Validation:
     - Module imports in Python (`import watpocket`).
   - Exit criteria:
     - All in-scope functions and data structures are callable/constructible from Python.

2. Callback-free trajectory binding.
   - Changes:
     - Provide wrapper overload for `analyze_trajectory_files` that omits callbacks and uses default/empty callback struct internally.
   - Validation:
     - Python call can generate trajectory CSV from WCN data.
   - Exit criteria:
     - Function is exposed and stable without callback API.

3. Exception mapping.
   - Changes:
     - Bind `watpocket::Error` to Python exception class with nanobind exception registration.
     - ensure translated messages remain unchanged from C++ when possible.
   - Validation:
     - Negative tests trigger Python exception with meaningful message.
   - Exit criteria:
     - Error surface matches C++ failure semantics.

4. Lifetime policy hardening for view types.
   - Changes:
     - Use nanobind return/lifetime policies for `PointSoA::view()` and `PointSoA::mutable_view()`.
   - Validation:
     - Python tests verify view validity while owner is alive and expected failure behavior after invalid use patterns.
   - Exit criteria:
     - No dangling-reference behavior in nominal Python usage.

### Phase 2: Python Test Integration via CTest
1. Add Python tests.
   - Changes:
     - New test file(s), e.g., `test/python/test_watpocket_bindings.py`.
     - Integration test on `complex_wcn.parm7 + 1.nc`.
   - Validation:
     - Python test compares generated CSV to `test/data/wcn/1.nc.csv`.
   - Exit criteria:
     - Test passes consistently on clean build tree.

2. Register Python tests in CTest.
   - Changes:
     - `test/CMakeLists.txt`: add `add_test(...)` invoking `Python3_EXECUTABLE -m pytest ...`.
   - Validation:
     - `ctest -R python` runs Python tests.
   - Exit criteria:
     - Python integration test is part of standard CTest execution.

3. Add stub generation/check.
   - Changes:
     - Add `nanobind_add_stub(...)` target for `watpocket.pyi` generation.
     - Add CI/local check that generated stubs are importable and synchronized.
   - Validation:
     - Stub generation target succeeds in build tree.
   - Exit criteria:
     - Python signatures are discoverable and reproducible.

### Phase 3: Hardening and Docs
1. Add user/developer documentation.
   - Changes:
     - README section for Python build/install/test commands.
     - Document current callback omission in Python API.
   - Validation:
     - Fresh environment can follow docs to run test.
   - Exit criteria:
     - Docs align with build and test behavior.

2. Repo map synchronization.
   - Changes:
     - Update `codebase-analysis-docs/codebasemap.md` with Python packaging/binding/test topology.
   - Validation:
     - New files and flow are discoverable in map.
   - Exit criteria:
     - docs/code state stay in sync.

## 5) Testing & Verification Plan

### Unit-Level Python Tests
- Import smoke test (`import watpocket`).
- Function-level tests:
  - `build_version()` returns non-empty string.
  - `parse_residue_selectors("164,128")` returns expected parsed objects.
  - `read_structure_points_by_atom_indices(...)` shape/order checks.
  - `PointSoA.view()`/`mutable_view()` lifetime policy checks.

### Integration Test (Required)
- Use:
  - topology: `test/data/wcn/complex_wcn.parm7`
  - trajectory: `test/data/wcn/1.nc`
  - resnums: `164,128,160,55,167,61,42,65,66`
- Execute Python `analyze_trajectory_files(...)` with CSV output path.
- Compare output file with `test/data/wcn/1.nc.csv` byte-for-byte.

### Regression
- Keep existing C++ CLI/API tests running unchanged.
- Run full `ctest --output-on-failure` after Python test registration.
- Run stub-generation target and validate generated `watpocket.pyi`.

## 6) Performance Engineering Plan

### Baseline
- Record current `ctest` runtime before Python integration.

### Measurement
- Measure:
  - incremental build time after adding binding target
  - Python integration test runtime

### Guardrails
- Binding layer remains thin (no duplicated geometry/IO logic).
- Avoid conversions that copy large coordinate arrays unless required.

## 7) Risk Register + Mitigations

1. ABI/linking mismatch between extension and `watpocket_lib`.
- Likelihood: Medium
- Impact: High
- Mitigation: Link extension directly to `watpocket_lib`; keep one compiler/runtime toolchain per build.
- Detection: import failure or unresolved symbols in test.

2. nanobind STL/path conversion gaps for `std::filesystem::path`.
- Likelihood: Medium
- Impact: Medium
- Mitigation: provide explicit wrapper signatures accepting `std::string` for paths if needed.
- Detection: compile-time binding errors or runtime type mismatch.

3. Python callback omission drift.
- Likelihood: Low
- Impact: Medium
- Mitigation: explicitly document callback omission in Python API and bind callback-free overload only.
- Detection: API review checklist and test coverage.

4. CTest/Python environment inconsistency.
- Likelihood: Medium
- Impact: Medium
- Mitigation: use `find_package(Python3 ...)` and `Python3_EXECUTABLE` in `add_test`.
- Detection: CI/local `ctest` failures specific to Python tests.

5. View lifetime bugs from non-owning pointer-backed C++ views.
- Likelihood: Medium
- Impact: High
- Mitigation: use nanobind `reference_internal`/`keep_alive` and avoid raw-pointer APIs in Python.
- Detection: targeted lifetime tests and ASan-enabled local runs.

6. Module naming mismatch (`NB_MODULE` vs CMake target/import name).
- Likelihood: Low
- Impact: High
- Mitigation: enforce `watpocket` parity across CMake target, macro, and import tests.
- Detection: import failure during Python smoke test.

## 8) Rollout / Migration Plan

1. Introduce Python bindings behind explicit build path first.
2. Land tests and CTest registration in same change.
3. Keep existing CLI/C++ APIs untouched.
4. Document supported Python API and known temporary exclusions (callbacks).

No C++ API breaking changes are required for this phase.

## 9) Deliverables Checklist

- [ ] `pyproject.toml` using `scikit-build-core`, Python `>=3.10`
- [ ] CMake wiring for nanobind extension target `watpocket`
- [ ] nanobind bindings exposing all exported functions except callback surface
- [ ] Python tests including WCN `1.nc` integration CSV comparison
- [ ] CTest registration for Python tests
- [ ] nanobind stub generation/check (`watpocket.pyi`)
- [ ] Documentation updates (usage + constraints)
- [ ] `codebase-analysis-docs/codebasemap.md` update

## Proposed Execution Sequence

1. Packaging + CMake target
2. Bind core structs/functions + exception mapping
3. Add Python tests (unit + WCN integration on `1.nc`)
4. Register tests in CTest
5. Run full verification and document
