# Codebase Map: CGAL (Convex_hull_3 Scoped)

Scope: this document is intentionally limited to the `Convex_hull_3` package and the dependencies it directly uses. It does not map all CGAL packages.

## 0. Quick Start for Contributors
- If you want to add a feature, start in `Convex_hull_3/include/CGAL/convex_hull_3.h` at the overloads of `CGAL::convex_hull_3(...)` and the internal pipeline rooted at `Convex_hull_3::internal::ch_quickhull_face_graph(...)`.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`convex_hull_3`, `ch_quickhull_face_graph`, `non_coplanar_quickhull_3`, `ch_quickhull_3_scan`).
- If you want to debug numerics/performance, focus first on `Is_on_positive_side_of_plane_3` and dual-halfspace traits.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`Is_on_positive_side_of_plane_3` specialization), `Convex_hull_3/include/CGAL/Convex_hull_3/dual/Convex_hull_traits_dual_3.h` (`Convex_hull_traits_dual_3`), `Convex_hull_3/include/CGAL/Convex_hull_3/dual/predicates.h`.
- If you want to refactor safely, preserve TDS face/vertex invariants (`info`, outside point lists, pending-facet iterator, border ring construction) and output adapter contracts (`copy_face_graph`, `clear`, `make_tetrahedron`).
  - Sources: `Convex_hull_3/include/CGAL/Convex_hull_face_base_2.h`, `Convex_hull_3/include/CGAL/Convex_hull_vertex_base_2.h`, `Convex_hull_3/include/CGAL/convex_hull_3.h`, `Convex_hull_3/include/CGAL/Convex_hull_3/internal/Indexed_triangle_set.h`.

## 1. Repository Overview

### High-Level Scoped Tree
- `Convex_hull_3/include/CGAL/`
  - Public entry points: `convex_hull_3.h`, `convex_hull_3_to_face_graph.h`, `convexity_check_3.h`, `Convex_hull_traits_3.h`, `Extreme_points_traits_adapter_3.h`.
  - Internal/dual: `Convex_hull_3/internal/Indexed_triangle_set.h`, `Convex_hull_3/dual/*`.
- `Convex_hull_3/examples/Convex_hull_3/`
  - Static hull, degenerate-object hull, graph/NP overloads, extreme points, dynamic hull bridge, halfspace intersection.
- `Convex_hull_3/test/Convex_hull_3/`
  - Degeneracy, dual-predicate equivalence tests, indexed triangle set output, halfspace intersection, default-traits behavior.
- `Convex_hull_3/benchmark/Convex_hull_3/`
  - Static vs dynamic vs incremental timing and predicate micro-benchmark.
- `Convex_hull_3/doc/Convex_hull_3/`
  - User-manual and API concept pages.

### Build Systems and Entrypoints
- CGAL top-level uses CMake and adds `Installation/` and `Documentation/doc/` from root `CMakeLists.txt`.
  - Source: `CMakeLists.txt`.
- `Convex_hull_3` is header-based; package-local CMake entrypoints are for examples/tests.
  - Sources: `Convex_hull_3/examples/Convex_hull_3/CMakeLists.txt`, `Convex_hull_3/test/Convex_hull_3/CMakeLists.txt`.
- Examples/tests are compiled as standalone single-source programs via `create_single_source_cgal_program(...)` after `find_package(CGAL REQUIRED)`.
  - Sources: `Convex_hull_3/examples/Convex_hull_3/CMakeLists.txt`, `Convex_hull_3/test/Convex_hull_3/CMakeLists.txt`.

### External Dependencies and Management (Scoped)
- Declared package dependencies are tracked in:
  - `Convex_hull_3/doc/Convex_hull_3/dependencies`
  - `Convex_hull_3/package_info/Convex_hull_3/dependencies`
- Effective code-level dependencies (from direct includes in `Convex_hull_3/include`) map primarily to packages:
  - `Convex_hull_2`, `Triangulation_3`, `TDS_2`, `BGL`, `Kernel_23`, `Cartesian_kernel`, `Filtered_kernel`, `Intersections_3`, `QP_solver`, `Number_types`, `Property_map`, `STL_Extension`, `Algebraic_foundations`, `HalfedgeDS`, `Stream_support`.
  - Evidence: direct include map from `Convex_hull_3/include/CGAL/*.h` and `Convex_hull_3/include/CGAL/Convex_hull_3/**/*`.

## 2. Architecture at a Glance

### Subsystem Diagram
- [assets/convex_hull_3_architecture.mmd](assets/convex_hull_3_architecture.mmd)

### Execution/Data-Flow Narrative
- Static path:
  - Public overloads of `CGAL::convex_hull_3(...)` normalize input and dispatch to `Convex_hull_3::internal::ch_quickhull_face_graph(...)` for 3D or to explicit degenerate branches.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`convex_hull_3` overloads, `ch_quickhull_face_graph`).
- Quickhull core:
  - Seed tetrahedron creation, per-face outside point assignment, iterative `farthest_outside_point -> find_visible_set -> star_hole -> partition_outside_sets` loop.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`non_coplanar_quickhull_3`, `ch_quickhull_3_scan`, `find_visible_set`, `partition_outside_sets`), `TDS_2/include/CGAL/Triangulation_data_structure_2.h` (`star_hole`).
- Coplanar fallback:
  - If all points are coplanar, projection traits select an axis plane and call `convex_hull_points_2` from `Convex_hull_2`, then triangulate the resulting polygon in target mesh.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`coplanar_3_hull`, `copy_ch2_to_face_graph`), `Convex_hull_2/include/CGAL/convex_hull_2.h` (`convex_hull_points_2`).
- Dual halfspace path:
  - `halfspace_intersection_3`/`halfspace_intersection_with_constructions_3` transform halfspaces to a dual hull problem, call `CGAL::convex_hull_3(...)`, and map back to primal.
  - Sources: `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_3.h`, `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_with_constructions_3.h`.
- Dynamic bridge path:
  - `convex_hull_3_to_face_graph(T, pm)` is a thin wrapper over `link_to_face_graph(T, T.infinite_vertex(), pm)` from `Triangulation_3`.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3_to_face_graph.h`, `Triangulation_3/include/CGAL/link_to_face_graph.h`.

### Key Cross-Cutting Concerns
- Numeric robustness:
  - Default trait selection and filtered predicate stack are central.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`Default_traits_for_Chull_3`, `Is_on_positive_side_of_plane_3`), `Convex_hull_3/include/CGAL/Convex_hull_traits_3.h`.
- Determinism:
  - Vertex base carries timestamps “for determinism of Compact_container iterators”.
  - Source: `Convex_hull_3/include/CGAL/Convex_hull_vertex_base_2.h`.
- Parallelism:
  - No OpenMP/TBB/CUDA/SYCL execution in this package code path; algorithms are single-threaded.
  - Evidence: no parallel runtime constructs in `Convex_hull_3/include`, examples, tests.
- Memory behavior:
  - Heavy use of `std::list` splice for outside-set reassignment; avoids point copies during quickhull scans.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`partition_outside_sets`, `non_coplanar_quickhull_3`, `ch_quickhull_3_scan`).
- Error handling:
  - Primarily assertions/preconditions and empty-result returns, not exception-rich API contracts.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h`, `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_3.h`.

## 3. Core Data Model

### Fundamental Types
- Primary geometric input/output types come from traits (`Point_3`, `Segment_3`, `Triangle_3`, custom `Plane_3`).
  - Source: `Convex_hull_3/include/CGAL/Convex_hull_traits_3.h` (`Convex_hull_traits_3`).
- `Convex_hull_traits_3::Plane_3` is represented as `Point_triple<R>` (three points), not `R::Plane_3`.
  - Source: `Convex_hull_3/include/CGAL/Convex_hull_traits_3.h` (`Point_triple`, `Convex_hull_traits_3::Plane_3`).
- Internal hull topology during quickhull uses `Triangulation_data_structure_2` with custom bases:
  - Face base: stores marker `info`, pending-list iterator `it`, and per-face outside points.
  - Vertex base: stores point, marker `info`, and timestamp.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`typedef Tds` in `ch_quickhull_face_graph`), `Convex_hull_3/include/CGAL/Convex_hull_face_base_2.h`, `Convex_hull_3/include/CGAL/Convex_hull_vertex_base_2.h`.

### Ownership and Lifetime Rules
- Input points are copied into a `std::list<Point_3>` for mutation and list-splice partitioning.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`convex_hull_3` overloads).
- Outside sets are moved between faces and temporary lists via `splice`, minimizing reallocations.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`partition_outside_sets`, `ch_quickhull_3_scan`).
- Output ownership is adapter-based:
  - Face-graph models via `copy_face_graph`,
  - output iterators via `Output_iterator_wrapper`,
  - indexed soup via `Indexed_triangle_set`.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h`, `Convex_hull_3/include/CGAL/Convex_hull_3/internal/Indexed_triangle_set.h`.

### Precision Strategy
- Default trait selection:
  - For floating `FT` + filtered kernels, default to `Convex_hull_traits_3<Kernel,...,Tag_true>`; else use kernel directly.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`Default_traits_for_Chull_3`).
- `Tag_true` specialization of side test uses staged filtering:
  - static filter with error bound,
  - interval arithmetic fallback,
  - exact arithmetic fallback with converted exact kernel.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`Is_on_positive_side_of_plane_3<Convex_hull_traits_3<...Tag_true>, std::true_type>`).
- Dual halfspace LP interior-point uses `Exact_field_selector<K::FT>::Type` for solving, but plane coefficients are fed as `double`.
  - Source: `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_interior_point_3.h` (`Interior_polyhedron_3::find`, `halfspace_intersection_interior_point_3`).

### Serialization / I/O Formats
- Package APIs are algorithmic and iterator/graph based; example I/O uses `.xyz` and `.off` via CGAL I/O helpers.
  - Sources: `Convex_hull_3/examples/Convex_hull_3/quickhull_3.cpp`, `graph_hull_3.cpp`, `extreme_indices_3.cpp`.

## 4. Subsystem Maps (one subsection per subsystem)

### 4.1 Static Quickhull Core
- Purpose:
  - Build convex hull for 3D point sets with degenerate handling.
- Key files:
  - `Convex_hull_3/include/CGAL/convex_hull_3.h`
  - `Convex_hull_3/include/CGAL/Convex_hull_face_base_2.h`
  - `Convex_hull_3/include/CGAL/Convex_hull_vertex_base_2.h`
- Key symbols:
  - `Convex_hull_3::internal::ch_quickhull_face_graph`
  - `Convex_hull_3::internal::non_coplanar_quickhull_3`
  - `Convex_hull_3::internal::ch_quickhull_3_scan`
  - `Convex_hull_3::internal::find_visible_set`
  - `Convex_hull_3::internal::partition_outside_sets`
  - `CGAL::convex_hull_3(...)` overload set
- Public API vs internal:
  - Public: overloads of `CGAL::convex_hull_3`.
  - Internal: all functions under `CGAL::Convex_hull_3::internal` and TDS-specific routines.
- Invariants and assumptions:
  - `Face::info` states (`0`, visited, border) are used as traversal/cleanup markers.
  - Each pending facet iterator `Face::it` must be maintained when faces enter/leave `pending_facets`.
  - Border edges are assembled into a ring before `star_hole`.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h`.
- Performance notes:
  - Hot path is repeated point-side classification and outside-set repartition.
  - Uses list splice to reduce copying of candidate points.
- Common bugs/pitfalls:
  - Violating marker reset logic in visible/border traversal can corrupt topology updates.
  - Changing orientation conventions in side tests can invert visible-set semantics.
- Extension points:
  - Add overloads/adapters around `convex_hull_3(...)` while preserving quickhull internal contract.

### 4.2 Traits and Filtered Predicates
- Purpose:
  - Provide stable traits for inexact construction kernels and concept-compliant functors.
- Key files:
  - `Convex_hull_3/include/CGAL/Convex_hull_traits_3.h`
  - `Convex_hull_3/include/CGAL/convex_hull_3.h`
- Key symbols:
  - `CGAL::Convex_hull_traits_3`
  - `CGAL::Point_triple`
  - `Convex_hull_3::internal::Default_traits_for_Chull_3`
  - `Convex_hull_3::internal::Is_on_positive_side_of_plane_3`
- Public API vs internal:
  - Public traits class: `Convex_hull_traits_3`.
  - Internal trait-selection/meta/predicate specialization in `convex_hull_3.h`.
- Invariants and assumptions:
  - Filtered path assumes Cartesian kernel behavior for specialized fast plane-side test.
  - `Protect_FPU_rounding`/interval protector usage must remain paired with interval checks.
- Performance notes:
  - Specialized side predicate avoids repeated plane constructions and amortizes cached representations.
- Common bugs/pitfalls:
  - Custom traits missing projection traits (`Traits_xy_3`, etc.) can break coplanar fallback.
  - Mis-specified `Has_filtered_predicates_tag` impacts default trait selection.
- Extension points:
  - Custom traits satisfying `ConvexHullTraits_3` can be injected into overloads.

### 4.3 Dual Halfspace Intersection
- Purpose:
  - Compute polyhedral halfspace intersections via dual convex hull.
- Key files:
  - `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_3.h`
  - `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_with_constructions_3.h`
  - `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_interior_point_3.h`
  - `Convex_hull_3/include/CGAL/Convex_hull_3/dual/Convex_hull_traits_dual_3.h`
  - `Convex_hull_3/include/CGAL/Convex_hull_3/dual/predicates.h`
- Key symbols:
  - `CGAL::halfspace_intersection_3`
  - `CGAL::halfspace_intersection_with_constructions_3`
  - `CGAL::halfspace_intersection_interior_point_3`
  - `Convex_hull_3::internal::build_primal_polyhedron`
  - `Convex_hull_3::internal::build_dual_polyhedron`
- Public API vs internal:
  - Public: the three halfspace functions.
  - Internal: dual predicate machinery and primal/dual reconstruction routines.
- Invariants and assumptions:
  - Halfspaces are lower halfspaces (`a x + b y + c z + d <= 0`).
  - `is_intersection_dim_3` asserts a 3D intersection configuration; boundedness remains a documented precondition of the API.
  - If provided, `origin` must be inside every halfspace.
- Performance notes:
  - `halfspace_intersection_with_constructions_3` explicitly constructs dual points (typically faster, less robust).
  - `halfspace_intersection_3` avoids explicit dual-point construction via dual traits (more robust path).
- Common bugs/pitfalls:
  - Disabling QP solver path (`CGAL_CH3_DUAL_WITHOUT_QP_SOLVER`) requires user-supplied interior point.
  - Degenerate or unbounded inputs violate preconditions and can trigger assertions/failures in validation checks.
- Extension points:
  - Add alternate interior-point strategy under same optional-origin contract.

### 4.4 Output Adapters and BGL Interoperability
- Purpose:
  - Emit hulls to heterogeneous outputs while reusing one core algorithm.
- Key files:
  - `Convex_hull_3/include/CGAL/convex_hull_3.h`
  - `Convex_hull_3/include/CGAL/Convex_hull_3/internal/Indexed_triangle_set.h`
- Key symbols:
  - `copy_face_graph`, `clear`, `make_tetrahedron` free functions
  - `Convex_hull_3::internal::Output_iterator_wrapper`
  - `Convex_hull_3::internal::Indexed_triangle_set`
- Public API surface vs internal:
  - Public overload supports `(vertices, faces)` indexed output and graph input.
  - Adapter glue is internal implementation detail.
- Invariants and assumptions:
  - Adapter types must satisfy enough of MutableFaceGraph-like operations used by helper overloads.
- Performance notes:
  - Indexed output writes directly into caller-provided containers and avoids graph construction.
- Common bugs/pitfalls:
  - Face index container element type must be mutable random-access triplets.
- Extension points:
  - Add new output adapter by providing `copy_face_graph/clear/make_tetrahedron` overloads.

### 4.5 Dynamic Hull Bridge (Triangulation_3 Dependency)
- Purpose:
  - Convert a dynamic triangulation hull boundary to a face graph.
- Key files:
  - `Convex_hull_3/include/CGAL/convex_hull_3_to_face_graph.h`
  - `Triangulation_3/include/CGAL/link_to_face_graph.h`
- Key symbols:
  - `CGAL::convex_hull_3_to_face_graph`
  - `CGAL::link_to_face_graph`
- Public API vs internal:
  - Public wrapper in `Convex_hull_3` delegates to `Triangulation_3` generic utility.
- Invariants and assumptions:
  - Uses `T.infinite_vertex()` and relies on triangulation incidence structure.
- Performance notes:
  - Conversion cost depends on incident cells around infinite vertex.
- Common bugs/pitfalls:
  - Dynamic triangulation vertices incident to infinite vertex are hull-adjacent, but not guaranteed one-to-one hull vertices in all interpretations (documented caveat in manual examples).
- Extension points:
  - Expose additional options (e.g., include/exclude infinite faces) by forwarding `link_to_face_graph` parameters.

### 4.6 Convexity Checking
- Purpose:
  - Postcondition and explicit validation of strong convexity.
- Key files:
  - `Convex_hull_3/include/CGAL/convexity_check_3.h`
- Key symbols:
  - `is_strongly_convex_3`, `is_locally_convex`, `all_points_inside`.
- Public API vs internal:
  - Public `is_strongly_convex_3` overloads; helper checks are internal-level utilities.
- Invariants and assumptions:
  - Requires consistent facet orientation and non-empty valid face graph.
- Performance notes:
  - Includes expensive geometric checks; full containment checks are not always part of default postcondition path.
- Common bugs/pitfalls:
  - Degenerate faces or inconsistent winding can cause false negatives.
- Extension points:
  - Add additional validation passes by composing existing helper functions.

## 5. Algorithms and Geometry Robustness

### Predicates / Constructions Used
- Static hull algorithm is Quickhull.
  - Source: `Convex_hull_3/doc/Convex_hull_3/Convex_hull_3.txt` (Static Convex Hull Construction section), `Convex_hull_3/include/CGAL/convex_hull_3.h`.
- Coplanar fallback delegates to 2D hull on projected traits.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`coplanar_3_hull`).
- Dual halfspace intersection uses convex hull in dual plus primal reconstruction.
  - Sources: `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_3.h`, `.../halfspace_intersection_with_constructions_3.h`.

### Handling Degeneracies
- Explicit branches for empty, one-point, two-point, collinear, coplanar, and 3-point non-collinear inputs.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`convex_hull_3(InputIterator,...,Object&,Traits)`, polygon-mesh overload).
- Tests cover degenerate behavior and expected return types.
  - Sources: `Convex_hull_3/test/Convex_hull_3/quickhull_degenerate_test_3.cpp`, `quickhull_test_3.cpp`, `test_quickhull_indexed_triangle_set_3.cpp`.

### Orientation / Winding Conventions
- Visibility classification depends on oriented side of per-face planes; face vertex order is chosen explicitly when building test planes.
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`find_visible_set`, initial tetrahedron setup and assertions).
- Convexity checks reconstruct facet triangles from halfedge circulation, assuming consistent orientation.
  - Source: `Convex_hull_3/include/CGAL/convexity_check_3.h`.

### Epsilon / Tolerance Policy
- For filtered Cartesian floating-point path, side predicate uses hardcoded static-filter bounds before interval/exact fallback.
  - Source: `Convex_hull_3/include/CGAL/convex_hull_3.h` (`static_filtered`).
- No global epsilon constant object is shared across package; tolerance behavior is local to predicates.
  - Inference from inspected headers.

### Determinism Policy
- Single-threaded control flow yields deterministic execution order for a fixed input order and kernel behavior.
- Vertex timestamps support deterministic container-iterator behavior in TDS vertices.
  - Source: `Convex_hull_3/include/CGAL/Convex_hull_vertex_base_2.h`.
- Dual reconstruction stores key->descriptor maps in unordered maps, but reconstruction logic queries by key and does not iterate these maps for output ordering.
  - Source: `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_3.h` (`build_primal_polyhedron`), `.../halfspace_intersection_with_constructions_3.h` (`build_dual_polyhedron`).

## 6. Parallel & HPC Model
- Threading model:
  - No OpenMP/TBB/std::execution/CUDA/HIP/SYCL execution paths in `Convex_hull_3` package code.
- Scheduling/synchronization:
  - Not applicable; algorithms are sequential.
- Memory locality strategy:
  - Uses pointer-rich dynamic structures (`Triangulation_data_structure_2`, lists, maps) optimized for topological updates rather than contiguous SIMD-friendly batches.
- Vectorization/SIMD:
  - No explicit SIMD intrinsics.
- GPU offload boundaries:
  - None.
- HPC-relevant note:
  - Performance-critical arithmetic paths are predicate-centric (static/interval/exact fallback), not parallel decomposition.

## 7. Build, Test, Bench, and Tooling

### How to Build + Run Tests (Scoped)
- Examples:
  - Configure/build under `Convex_hull_3/examples/Convex_hull_3/` (uses `find_package(CGAL REQUIRED)`).
  - Source: `Convex_hull_3/examples/Convex_hull_3/CMakeLists.txt`.
- Tests:
  - Configure/build under `Convex_hull_3/test/Convex_hull_3/`; CMake glob builds each `*.cpp` as a target.
  - Source: `Convex_hull_3/test/Convex_hull_3/CMakeLists.txt`.

### Benchmark Harness Structure
- Comparative benchmark:
  - Static quickhull, Delaunay+link-to-face-graph, and incremental hull variants.
  - Source: `Convex_hull_3/benchmark/Convex_hull_3/compare_different_approach.cpp`.
- Predicate micro-benchmark:
  - Compares kernel orientation, static optimized side test, and interval version.
  - Source: `Convex_hull_3/benchmark/Convex_hull_3/is_on_positive_side.cpp`.

### CI Notes (Scoped)
- No package-local CI config in `Convex_hull_3`; CI orchestration is repository-level.

### Debugging/Profiling Tips Grounded in Repo Tooling
- Use `Convex_hull_3/benchmark/Convex_hull_3/is_on_positive_side.cpp` to isolate side-predicate costs before changing quickhull internals.
- Toggle postcondition cost via `CGAL_CH_NO_POSTCONDITIONS` and expensive checks via `CGAL_CHECK_EXPENSIVE` (manual note).
  - Sources: `Convex_hull_3/include/CGAL/convex_hull_3.h` (postcondition include guard), `Convex_hull_3/doc/Convex_hull_3/PackageDescription.txt` (Assertions section).
- For halfspace debugging, verify interior point path (`halfspace_intersection_interior_point_3`) and macro `CGAL_CH3_DUAL_WITHOUT_QP_SOLVER` behavior.
  - Sources: `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_3.h`, `.../halfspace_intersection_with_constructions_3.h`.

## 8. Change Playbooks (Practical Guides)

### Add a New Algorithm
- Goal: add a new static hull variant while preserving current API behavior.
- Steps:
  - Add/encapsulate algorithm core under `CGAL::Convex_hull_3::internal` in `Convex_hull_3/include/CGAL/convex_hull_3.h`.
  - Route from selected overload(s) in same file, keeping degenerate dispatch behavior unchanged.
  - Ensure output adapters still work (`copy_face_graph`, `clear`, `make_tetrahedron` overloads in `convex_hull_3.h` and `Convex_hull_3/internal/Indexed_triangle_set.h`).
  - Add regression coverage in `Convex_hull_3/test/Convex_hull_3/` using existing patterns from `quickhull_test_3.cpp` and `quickhull_degenerate_test_3.cpp`.

### Add a New Geometry Primitive
- Goal: allow new point-like key types or wrappers.
- Steps:
  - Implement/compose a traits type satisfying `ConvexHullTraits_3` concept requirements used by `convex_hull_3`.
  - Use pattern from `Extreme_points_traits_adapter_3` to forward predicates through property maps if points are indirect.
  - Validate coplanar projection traits (`Traits_xy_3`, `Traits_yz_3`, `Traits_xz_3`) for fallback path.
  - Sources: `Convex_hull_3/include/CGAL/Convex_hull_traits_3.h`, `Convex_hull_3/include/CGAL/Extreme_points_traits_adapter_3.h`, `Convex_hull_3/include/CGAL/convex_hull_3.h` (`coplanar_3_hull`).

### Add a New Backend (e.g., CUDA)
- Current state:
  - No backend abstraction or parallel dispatch seams exist in this package.
- Practical integration seam:
  - Keep public overload signatures unchanged in `convex_hull_3.h`; add a policy/tag dispatch layer before entering current internal functions.
  - Preserve deterministic and robustness semantics by matching predicate contracts from `Convex_hull_traits_3` and dual traits.
  - Start with read-only offload candidates (predicate batches), then re-integrate with sequential topology updates (`star_hole`).
  - Key files: `Convex_hull_3/include/CGAL/convex_hull_3.h`, `Convex_hull_3/include/CGAL/Convex_hull_traits_3.h`, `TDS_2/include/CGAL/Triangulation_data_structure_2.h`.

### Refactor a Core Type Safely
- Focus type: `Convex_hull_face_base_2` and `Convex_hull_vertex_base_2`.
- Safety checklist:
  - Preserve `info()` semantics used by visibility and border logic.
  - Preserve `Face::points` and `Face::it` fields used by pending-facet management.
  - Preserve vertex timestamp API (`Has_timestamp`, `time_stamp`, `set_time_stamp`).
  - Re-run tests touching degeneracy, dual behavior, and indexed output:
    - `quickhull_degenerate_test_3.cpp`
    - `quickhull_test_3.cpp`
    - `test_halfspace_intersections.cpp`
    - `test_quickhull_indexed_triangle_set_3.cpp`

## 9. Index: Symbols → Files
- `CGAL::convex_hull_3(...)` → `Convex_hull_3/include/CGAL/convex_hull_3.h` (public overloads; dispatch and adaptation).
- `Convex_hull_3::internal::ch_quickhull_face_graph` → `Convex_hull_3/include/CGAL/convex_hull_3.h` (quickhull entry core).
- `Convex_hull_3::internal::ch_quickhull_3_scan` → `Convex_hull_3/include/CGAL/convex_hull_3.h` (iterative expansion loop).
- `Convex_hull_3::internal::find_visible_set` → `Convex_hull_3/include/CGAL/convex_hull_3.h` (visible/border extraction).
- `Convex_hull_3::internal::partition_outside_sets` → `Convex_hull_3/include/CGAL/convex_hull_3.h` (outside-point repartitioning).
- `CGAL::Convex_hull_traits_3` → `Convex_hull_3/include/CGAL/Convex_hull_traits_3.h` (default robust traits wrapper).
- `Convex_hull_3::internal::Default_traits_for_Chull_3` → `Convex_hull_3/include/CGAL/convex_hull_3.h` (default trait selection policy).
- `Convex_hull_3::internal::Is_on_positive_side_of_plane_3` → `Convex_hull_3/include/CGAL/convex_hull_3.h` (critical side predicate, robust specialization).
- `CGAL::Extreme_points_traits_adapter_3` → `Convex_hull_3/include/CGAL/Extreme_points_traits_adapter_3.h` (property-map based adapter).
- `CGAL::extreme_points_3(...)` → `Convex_hull_3/include/CGAL/convex_hull_3.h` (extreme-point extraction path).
- `CGAL::halfspace_intersection_3` → `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_3.h` (robust dual path).
- `CGAL::halfspace_intersection_with_constructions_3` → `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_with_constructions_3.h` (explicit dual-point path).
- `CGAL::halfspace_intersection_interior_point_3` → `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_interior_point_3.h` (LP interior point).
- `CGAL::Convex_hull_3::Convex_hull_traits_dual_3` → `Convex_hull_3/include/CGAL/Convex_hull_3/dual/Convex_hull_traits_dual_3.h` (dual traits with filtered variants).
- `CGAL::convex_hull_3_to_face_graph` → `Convex_hull_3/include/CGAL/convex_hull_3_to_face_graph.h` (dynamic bridge).
- `CGAL::link_to_face_graph` → `Triangulation_3/include/CGAL/link_to_face_graph.h` (triangulation-to-face-graph conversion).
- `CGAL::is_strongly_convex_3` → `Convex_hull_3/include/CGAL/convexity_check_3.h` (convexity checker).

---

### Appendix A: Declared vs Effective Dependencies

Declared in `Convex_hull_3/doc/Convex_hull_3/dependencies`:
- `Manual`, `Kernel_23`, `STL_Extension`, `Algebraic_foundations`, `Circulator`, `Stream_support`, `Convex_hull_2`, `Convex_hull_d`, `Triangulation_3`, `Polyhedron`, `Surface_mesh`, `BGL`.

Declared in `Convex_hull_3/package_info/Convex_hull_3/dependencies` (packaging closure):
- `Algebraic_foundations`, `Arithmetic_kernel`, `BGL`, `CGAL_Core`, `Cartesian_kernel`, `Circulator`, `Convex_hull_2`, `Convex_hull_3`, `Distance_2`, `Distance_3`, `Filtered_kernel`, `HalfedgeDS`, `Hash_map`, `Installation`, `Intersections_2`, `Intersections_3`, `Interval_support`, `Kernel_23`, `Modular_arithmetic`, `Number_types`, `Profiling_tools`, `Property_map`, `QP_solver`, `Random_numbers`, `STL_Extension`, `Stream_support`, `TDS_2`, `Triangulation_3`.

Effective direct include-level deps observed in `Convex_hull_3/include`:
- `Convex_hull_2`, `Triangulation_3`, `TDS_2`, `BGL`, `Kernel_23`, `Cartesian_kernel`, `Filtered_kernel`, `Intersections_3`, `QP_solver`, `Number_types`, `Property_map`, `STL_Extension`, `Algebraic_foundations`, `HalfedgeDS`, `Stream_support`, `Installation` (license/warning wrappers).

### Appendix B: Representative Use Paths
- [assets/convex_hull_3_workflows.mmd](assets/convex_hull_3_workflows.mmd)

Representative workflows traced in source:
- Static quickhull to face graph: `Convex_hull_3/examples/Convex_hull_3/quickhull_3.cpp`.
- Any-dimension object output: `Convex_hull_3/examples/Convex_hull_3/quickhull_any_dim_3.cpp`.
- Halfspace intersection: `Convex_hull_3/examples/Convex_hull_3/halfspace_intersection_3.cpp`.
- Dynamic triangulation bridge: `Convex_hull_3/examples/Convex_hull_3/dynamic_hull_3.cpp`.

### Appendix C: Files Read (Curated)
- `CMakeLists.txt`
- `Convex_hull_3/doc/Convex_hull_3/dependencies`
- `Convex_hull_3/doc/Convex_hull_3/Convex_hull_3.txt`
- `Convex_hull_3/doc/Convex_hull_3/PackageDescription.txt`
- `Convex_hull_3/package_info/Convex_hull_3/dependencies`
- `Convex_hull_3/examples/Convex_hull_3/CMakeLists.txt`
- `Convex_hull_3/test/Convex_hull_3/CMakeLists.txt`
- `Convex_hull_3/include/CGAL/convex_hull_3.h`
- `Convex_hull_3/include/CGAL/Convex_hull_traits_3.h`
- `Convex_hull_3/include/CGAL/convexity_check_3.h`
- `Convex_hull_3/include/CGAL/convex_hull_3_to_face_graph.h`
- `Convex_hull_3/include/CGAL/Extreme_points_traits_adapter_3.h`
- `Convex_hull_3/include/CGAL/Convex_hull_face_base_2.h`
- `Convex_hull_3/include/CGAL/Convex_hull_vertex_base_2.h`
- `Convex_hull_3/include/CGAL/Convex_hull_3/internal/Indexed_triangle_set.h`
- `Convex_hull_3/include/CGAL/Convex_hull_3/dual/Convex_hull_traits_dual_3.h`
- `Convex_hull_3/include/CGAL/Convex_hull_3/dual/predicates.h`
- `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_3.h`
- `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_with_constructions_3.h`
- `Convex_hull_3/include/CGAL/Convex_hull_3/dual/halfspace_intersection_interior_point_3.h`
- `Triangulation_3/include/CGAL/link_to_face_graph.h`
- `TDS_2/include/CGAL/Triangulation_data_structure_2.h`
- `Convex_hull_2/include/CGAL/convex_hull_2.h`
- `Convex_hull_3/examples/Convex_hull_3/quickhull_3.cpp`
- `Convex_hull_3/examples/Convex_hull_3/quickhull_any_dim_3.cpp`
- `Convex_hull_3/examples/Convex_hull_3/halfspace_intersection_3.cpp`
- `Convex_hull_3/examples/Convex_hull_3/dynamic_hull_3.cpp`
- `Convex_hull_3/examples/Convex_hull_3/graph_hull_3.cpp`
- `Convex_hull_3/examples/Convex_hull_3/extreme_indices_3.cpp`
- `Convex_hull_3/examples/Convex_hull_3/dynamic_hull_SM_3.cpp`
- `Convex_hull_3/benchmark/Convex_hull_3/compare_different_approach.cpp`
- `Convex_hull_3/benchmark/Convex_hull_3/is_on_positive_side.cpp`
- `Convex_hull_3/test/Convex_hull_3/quickhull_degenerate_test_3.cpp`
- `Convex_hull_3/test/Convex_hull_3/quickhull_test_3.cpp`
- `Convex_hull_3/test/Convex_hull_3/test_halfspace_intersections.cpp`
- `Convex_hull_3/test/Convex_hull_3/test_extreme_points.cpp`
- `Convex_hull_3/test/Convex_hull_3/test_quickhull_indexed_triangle_set_3.cpp`
- `Convex_hull_3/test/Convex_hull_3/test_ch_3_ambiguity.cpp`
- `Convex_hull_3/test/Convex_hull_3/issue_8954.cpp`
- `Convex_hull_3/test/Convex_hull_3/quick_hull_default_traits.cpp`
- `Convex_hull_3/test/Convex_hull_3/ch3_dual_test_equal_3.cpp`
- `Convex_hull_3/test/Convex_hull_3/ch3_dual_test_has_on_positive_side_3.cpp`
- `Convex_hull_3/test/Convex_hull_3/ch3_dual_test_less_signed_distance_to_plane_3.cpp`
