# 26-03-01 PointSoA Full Migration

## 1) Problem Framing

### Goal
Replace AoS point handling (`std::vector<Point3>` and direct CGAL point usage patterns) with a SoA design centered on `watpocket::PointSoA` (`double` coordinates), while keeping CGAL fully hidden from user-facing APIs so downstream users do not need CGAL headers/types to compile against watpocket.

### In Scope
- Introduce public SoA point types and lightweight views for read-only and mutable access.
- Migrate library, CLI, and tests away from AoS point arrays.
- Refactor geometry boundaries so CGAL headers/types stay in private implementation units.
- Add internal index-based adapters/property maps for CGAL convex hull calls.
- Convert Chemfiles frame positions to SoA using reusable buffers.

### Out of Scope
- Benchmarking and performance-report deliverables.
- API backward-compatibility adapters.
- Non-`double` scalar support.
- GPU or parallel algorithm redesign.

### Assumptions
- Build system remains CMake and primary binary remains `watpocket`.
- Current single-threaded execution model remains unchanged.
- Existing hull semantics must be preserved (boundary points are inside).

## 2) Public API Contract (Detailed)

This section defines exact behavior for the new public SoA types.

### `watpocket::PointSoA` (owning)
- Representation:
  - Owns three coordinate arrays: `x`, `y`, `z` (all `std::vector<double>`).
  - Invariant: `x.size() == y.size() == z.size()`.
- Core operations:
  - `size() -> std::size_t`: number of points.
  - `empty() -> bool`.
  - `clear()`: clears all three arrays, invariant preserved.
  - `resize(n)`: resizes all three arrays to `n`.
  - `reserve(n)`: reserves capacity in all three arrays (best effort; each array independently).
  - `push_back(xv, yv, zv)`: appends one point.
  - `point(i) -> std::array<double,3>`: bounds-checked accessor (`throw watpocket::Error` on invalid index).
  - `set_point(i, xv, yv, zv)`: bounds-checked mutator (`throw watpocket::Error` on invalid index).
- Exception and safety contract:
  - All mutating operations preserve invariant on failure.
  - Bounds errors are reported via `watpocket::Error`.
  - No CGAL/Chemfiles types in signature.
- Complexity:
  - `size/empty`: O(1)
  - `clear/resize/reserve/push_back`: amortized vector complexity.
  - `point/set_point`: O(1)

### `watpocket::PointSoAView` (non-owning read-only)
- Representation:
  - Non-owning view over three contiguous `double` buffers + length.
  - Does not manage lifetime; caller owns backing storage.
- Invariants:
  - All three buffers must be non-null when length > 0.
  - All three buffers represent at least `length` elements.
- Core operations:
  - `size()`, `empty()`, `point(i)`.
- Error contract:
  - Invalid index -> `watpocket::Error`.
  - Constructor/factory validates null/length consistency.

### `watpocket::PointSoAMutableView` (non-owning mutable)
- Same as read-only view plus `set_point(i, xv, yv, zv)`.
- Same lifetime and bounds constraints as `PointSoAView`.

### Invalidation Rules
- Any `PointSoA` operation that reallocates (`reserve`, growth through `push_back`, `resize` growth) invalidates raw pointers/spans/views derived from the old buffers.
- Views remain valid only while backing storage is unchanged and alive.

### Header Boundary Rule
- `include/watpocket/watpocket.hpp` must remain std-only and must not include CGAL or Chemfiles headers/types.

## 3) Architecture and Abstraction Ownership

### Ownership of Abstractions
- Public data abstractions (`PointSoA`, views, result structs) are owned by `include/watpocket/watpocket.hpp`.
- Geometry adaptation abstractions (SoA -> CGAL property map/index adapter) are owned by private implementation in `src/watpocket_lib/`.
- Convex hull/classification algorithms remain in private implementation and consume SoA views internally.

### Internal Boundary Layout
- Public header:
  - `include/watpocket/watpocket.hpp`
- Private geometry bridge:
  - `src/watpocket_lib/point_soa_cgal_adapter.hpp` (new, private)
  - `src/watpocket_lib/point_soa_cgal_adapter.cpp` (new, private)
- Main analysis path:
  - `src/watpocket_lib/watpocket.cpp`

### Dataflow (Target)
Chemfiles frame/topology -> reusable `PointSoA` buffers -> SoA views -> private CGAL adapter/property map -> hull/classification internals -> public result structs.

## 4) Work Breakdown Structure (File-Level)

### Phase A: Public Type Introduction
Task A1: Add SoA types and contracts to public API
- Files:
  - `include/watpocket/watpocket.hpp`
  - `test/watpocket_api_tests.cpp`
- Changes:
  - Add `PointSoA`, `PointSoAView`, `PointSoAMutableView` declarations and docs.
  - Keep all signatures std-only.
  - Add API tests for invariants, bounds checks, and view behavior.
- Acceptance:
  - `cmake --build build --target watpocket_api_tests`
  - `ctest --test-dir build -R "^api\\."`

Task A2: Replace public AoS-facing signatures
- Files:
  - `include/watpocket/watpocket.hpp`
  - `src/watpocket_lib/watpocket.cpp`
  - `src/watpocket/main.cpp` (if any call-site adaptation needed)
- Changes:
  - Replace remaining point-array signatures with SoA/view-based parameters/returns where applicable.
  - Ensure draw helpers continue to use CGAL-free public structs.
- Acceptance:
  - `cmake --build build --target watpocket watpocket_api_tests`

### Phase B: Private Geometry Bridge
Task B1: Introduce SoA-to-CGAL adapter/property map
- Files:
  - `src/watpocket_lib/point_soa_cgal_adapter.hpp` (new)
  - `src/watpocket_lib/point_soa_cgal_adapter.cpp` (new)
  - `src/watpocket_lib/watpocket.cpp`
- Changes:
  - Implement index-based point access for CGAL convex hull on top of SoA views.
  - Keep all CGAL includes confined to private headers/sources.
- Acceptance:
  - Build succeeds.
  - Degenerate hull behavior unchanged in tests (collinear/coplanar/too-few points errors).

Task B2: Remove internal `std::vector<Point3>` construction in main path
- Files:
  - `src/watpocket_lib/watpocket.cpp`
- Changes:
  - Replace AoS point extraction and downstream hull calls with SoA+view path.
  - Keep inclusion semantics unchanged for inside-or-on hull checks.
- Acceptance:
  - Existing CLI behavior tests pass.
  - Existing API tests pass.

### Phase C: Chemfiles Extraction Migration
Task C1: Frame-to-SoA extraction
- Files:
  - `src/watpocket_lib/watpocket.cpp`
- Changes:
  - Replace AoS point extraction helpers with direct filling of reusable `PointSoA` buffers.
  - Reuse buffers per frame in trajectory loop.
  - Preserve current validation and error messages.
- Acceptance:
  - Structure and trajectory CLI tests pass:
    - `ctest --test-dir build -R "^cli\\."`

Task C2: Callback/result integrity verification
- Files:
  - `src/watpocket_lib/watpocket.cpp`
  - `test/watpocket_api_tests.cpp`
- Changes:
  - Ensure frame callbacks and summary stats are unchanged in behavior.
  - Add tests for deterministic ordered outputs where applicable.
- Acceptance:
  - `ctest --test-dir build -R "^(api|cli)\\."`

### Phase D: Cleanup and Documentation
Task D1: Delete AoS dead code and tighten invariants
- Files:
  - `src/watpocket_lib/watpocket.cpp`
  - `include/watpocket/watpocket.hpp`
  - tests touching old AoS paths
- Changes:
  - Remove unused AoS helpers/types.
  - Ensure all SoA boundary checks use consistent `watpocket::Error` messages.
- Acceptance:
  - Full configured test run passes.

Task D2: Update project docs
- Files:
  - `codebase-analysis-docs/codebasemap.md`
  - `codebase-analysis-docs/assets/*.mmd` (if architecture/dataflow diagrams changed)
  - plan/docs files referencing old AoS flow
- Changes:
  - Reflect new SoA dataflow and private CGAL adapter boundary.
- Acceptance:
  - Docs and code are aligned in the same change set.

## 5) Testing and Verification Plan (Executable)

### Unit/API Tests
- Add/extend tests in `test/watpocket_api_tests.cpp` for:
  - `PointSoA` invariant preservation across `clear/resize/push_back`.
  - View bounds checking and mutability behavior.
  - Error-path checks for invalid indices and malformed views.

### Integration/CLI Tests
- Reuse existing CTest suite in `test/CMakeLists.txt`:
  - help/version checks
  - draw mode extension validation
  - trajectory mode required options and draw validation
  - package consumer build/run test (`watpocket_package_consumer`)

### Geometry Regression Checks
- Preserve failure modes:
  - fewer than 4 points
  - collinear points
  - coplanar points
- Preserve inclusion behavior:
  - ON boundary counts as inside.

### Header Boundary Verification
- Keep `package_consumer.cpp` style compile check as CGAL leakage guard.
- Add explicit CI check that public headers compile without direct CGAL includes.

### Determinism Checks
- Re-run selected structure and trajectory cases and compare:
  - sorted residue-id outputs
  - CSV row ordering and counts

## 6) Risk Register and Mitigations

- SoA invariant drift:
  - Risk: x/y/z sizes diverge due to partial updates.
  - Mitigation: centralized mutators and invariant checks at boundary constructors/functions.

- CGAL leakage through headers:
  - Risk: accidental CGAL type/include reaches public header.
  - Mitigation: enforce std-only public header rule and consumer compile test.

- Behavior drift in hull inclusion:
  - Risk: adapter/materialization changes classification semantics.
  - Mitigation: explicit regression tests for boundary-inside behavior and degeneracy errors.

- Chemfiles extraction mismatch:
  - Risk: atom-index extraction into SoA misaligns points.
  - Mitigation: selector-driven tests in both structure and trajectory paths with known fixtures.

## 7) Deliverables Checklist

- Public SoA types and views with explicit contracts in `watpocket.hpp`.
- Private SoA-to-CGAL adapter/property map implementation.
- Full library/CLI migration away from AoS point arrays.
- Updated API and CLI tests passing.
- Updated codebase docs and diagrams reflecting SoA dataflow and CGAL-private boundary.
