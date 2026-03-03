# parallel-analyze-trajectory-files - 26-03-03

## 1. Problem Framing
- Goal: Extend `analyze_trajectory_files(...)` with an optional `num_threads` parameter (default `1`) and implement parallel frame analysis when `num_threads > 1`, while preserving serial-equivalent outputs.
- In scope:
  - Refactor `src/watpocket_lib/watpocket.cpp` so `analyze_trajectory_files(...)` becomes a small orchestration function with clearly scoped helpers.
  - Add `num_threads` to public C++ API (`include/watpocket/watpocket.hpp`), CLI interface (`src/watpocket/main.cpp`), and Python binding (`src/python/bindings.cpp`).
  - Enforce input validation: invalid thread counts must throw `watpocket::Error`.
  - Preserve CSV ordering and summary semantics to match serial behavior.
  - Update docs required by repo policy: `codebase-analysis-docs/codebasemap.md` and architecture/data-flow `.mmd` assets when concurrency/data-flow changes.
- Out of scope:
  - Changing hull/water algorithms, selector semantics, file format support, or output schemas.
  - SIMD/GPU/MPI work.
- Assumptions:
  - C++17/20 toolchain already used in project build.
  - `chemfiles::Trajectory` frame reads remain on one reader thread; parallelism is compute-side after frame acquisition.
  - CGAL hull computation is thread-safe when each thread uses independent frame-local data.

## 2. Success Criteria (Measurable)
- Correctness:
  - For the same input, `num_threads=1` and `num_threads>1` produce identical CSV rows (same frame order and counts), identical `TrajectorySummary` metrics, and identical skipped-frame warning mapping text.
  - Existing API and CLI error contracts remain intact, with added explicit error for invalid `num_threads` (`0`).
- Performance:
  - On representative `test/data/wcn/*.nc`, `num_threads=2` should not regress wall-clock runtime versus `num_threads=1`.
  - Add a simple benchmark measurement path (timed integration run or temporary profiling harness) to compare 1 vs 2 threads.
- Resource:
  - Memory growth bounded by in-flight frame task buffers (target: O(num_threads) additional frame data).
- Portability:
  - Compiles and tests pass on existing supported toolchains in this repo.
- Maintainability:
  - `analyze_trajectory_files(...)` reduced to orchestration logic plus helper calls; helper responsibilities are single-purpose and testable.

## 3. Architecture Sketch
- Modules/responsibilities:
  - `analyze_trajectory_files(...)`: orchestration only.
  - New validation helper: input/path/extension/thread-count checks.
  - New topology-prep helper: resolve topology atom count, CA indices, water refs.
  - New output-init helper: open CSV/draw streams and write headers.
  - New frame-analysis helper(s): process one frame into a structured result (`success`/`warning`, water IDs, optional hull/water atoms for draw).
  - New aggregation/output helper: deterministic ordered emission of CSV/draw/statistics from per-frame results.
- Dataflow (parallel mode):
  - Reader loop assigns frame number and acquires frame.
  - Worker tasks compute hull + inside-water IDs independently.
  - Ordered commit stage writes CSV/draw strictly by frame number and aggregates summary counters.
- Concurrency model:
  - `num_threads == 1`: existing serial path semantics (no worker pool usage).
  - `num_threads > 1`: bounded worker pool or async task queue with at most `num_threads` active compute tasks.
  - Shared mutable outputs (`ofstream`, summary maps) are only touched in ordered commit stage (single thread), avoiding lock-heavy IO.
- Memory model:
  - Frame-local `PointSoA` and hull/water intermediate data per task.
  - Central ordered result buffer keyed by frame index for deterministic commit.
- Error handling/logging:
  - Frame-local failures converted to existing skipped warnings.
  - Fatal setup/IO errors remain throwing `watpocket::Error`.

## 4. Work Breakdown Structure (WBS)

### Phase 0: Baseline and Refactor Design
- Task 0.1: Capture current behavior baseline
  - Changes: none or temporary measurement notes in `plans/`.
  - Validation: run relevant API/CLI/integration tests to lock current expected outputs.
  - Performance: record baseline runtime for `num_threads=1` equivalent.
  - Exit: baseline results documented for comparison.
- Task 0.2: Define helper boundaries and data structs
  - Changes: design sketch in plan + implementation TODO mapping to files.
  - Validation: ensure no helper crosses unrelated concerns.
  - Exit: approved decomposition before code edits.

### Phase 1: Public API and Interface Plumbing
- Task 1.1: Extend C++ API signature
  - Changes:
    - `include/watpocket/watpocket.hpp`: add trailing `std::size_t num_threads = 1` to `analyze_trajectory_files(...)`.
    - `src/watpocket_lib/watpocket.cpp`: update definition.
  - Validation: compile + API tests.
  - Exit: existing callers compile unchanged.
- Task 1.2: Extend CLI surface
  - Changes:
    - `src/watpocket/main.cpp`: add `--threads` option defaulting to `1`; pass through to `analyze_trajectory_files(...)`; invalid values throw.
  - Validation: CLI tests for default and invalid thread counts.
  - Exit: CLI exposes thread control with backward-compatible default behavior.
- Task 1.3: Extend Python binding
  - Changes:
    - `src/python/bindings.cpp`: add `num_threads` kwarg default `1` in `analyze_trajectory_files` binding and pass-through.
    - Python integration test updates (`test/python/integration_wcn_1nc.py`) for explicit non-default coverage.
  - Validation: Python integration test pass.
  - Exit: API parity across C++/CLI/Python.

### Phase 2: Internal Refactor of `analyze_trajectory_files`
- Task 2.1: Extract setup helpers
  - Changes in `src/watpocket_lib/watpocket.cpp`:
    - `validate_trajectory_analysis_inputs(...)`
    - `prepare_trajectory_topology_context(...)`
    - `open_trajectory_outputs(...)`
  - Validation: unit/API tests unaffected.
  - Exit: setup logic removed from main function body.
- Task 2.2: Extract per-frame compute helper
  - Changes:
    - `analyze_single_trajectory_frame(...)` returning a result struct (success/warning + payload).
  - Validation: deterministic equivalence tests for selected frames.
  - Exit: frame compute isolated and reusable by serial/parallel dispatch.
- Task 2.3: Extract ordered commit/aggregation helper
  - Changes:
    - `commit_frame_result_in_order(...)`
    - `finalize_trajectory_summary(...)`
  - Validation: summary counts and warning map match previous behavior.
  - Exit: output/summary mutation centralized.

### Phase 3: Parallel Execution Path
- Task 3.1: Implement execution strategy switch
  - Changes:
    - In `analyze_trajectory_files(...)`, branch on `num_threads`:
      - `1`: serial dispatch using shared helpers.
      - `>1`: bounded parallel dispatch.
  - Validation: equivalence tests serial vs parallel.
  - Performance: compare runtime for 1 vs 2 threads.
  - Exit: parallel path active only when requested.
- Task 3.2: Deterministic ordered output
  - Changes:
    - Buffer task results by frame number; commit sequentially (1..N).
  - Validation: CSV and draw PDB ordering exactly matches serial output.
  - Exit: deterministic contract guaranteed.
- Task 3.3: Thread-count validation
  - Changes:
    - Central check that `num_threads == 0` throws `watpocket::Error`.
  - Validation: dedicated tests for invalid values through C++/CLI/Python surfaces.
  - Exit: invalid thread counts always fail fast.

### Phase 4: Hardening, Tests, and Documentation
- Task 4.1: Expand test suite
  - Changes:
    - `test/watpocket_api_tests.cpp`: add cases for `num_threads=1`, `num_threads=2`, invalid `0`, and output equivalence.
    - `test/CMakeLists.txt`: add/adjust CLI test cases for `--threads` behavior.
    - Python integration test update for `num_threads`.
  - Validation: full test run passes.
  - Exit: regression coverage for threading behavior.
- Task 4.2: Documentation sync (mandatory)
  - Changes:
    - `codebase-analysis-docs/codebasemap.md` update sections for API signature, trajectory concurrency model, CLI/Python options.
    - `codebase-analysis-docs/assets/data-flow.mmd` and potentially `architecture.mmd` to depict optional parallel frame compute + ordered commit.
  - Validation: docs reflect actual implementation boundaries and interfaces.
  - Exit: code and docs are synchronized in same change set.

## 5. Testing and Verification Plan
- Unit/API tests:
  - Validate default behavior remains serial (`num_threads` omitted or `1`).
  - Validate invalid `num_threads=0` throws from all entry points.
  - Validate skipped-frame warning behavior unchanged.
- Integration tests:
  - Existing WCN trajectory CSV benchmarks with `num_threads=1` and `num_threads=2` (expect identical outputs).
  - If draw output is enabled in test, verify stable MODEL order and content.
- Determinism tests:
  - Run same parallel test multiple times; compare CSV byte-for-byte.
  - Compare `TrajectorySummary` and warning-map entries between serial and parallel runs.
- Sanitizers:
  - Run ASan/UBSan where available.
  - Run TSan on trajectory tests if build supports it to catch races in new parallel path.

## 6. Performance Engineering Plan
- Baseline:
  - Measure current serial runtime on `test/data/wcn/1.nc` and optionally larger internal trajectory.
- Profiling:
  - Use coarse timers around frame read, hull compute, and output commit sections.
- Hotspot hypotheses:
  - Hull compute + water classification dominate; file IO and ordered commit likely secondary.
- Optimization ladder:
  - Start with safe task parallelism over frame compute.
  - Keep serial reader/ordered writer to preserve determinism.
  - If contention appears, reduce allocation churn in frame result buffers.
- Guardrails:
  - No optimization accepted without equality checks against serial outputs.

## 7. Risk Register and Mitigations
- Race conditions in shared structures
  - Likelihood: medium; Impact: high.
  - Mitigation: single-threaded commit stage; frame-local task state only.
  - Detection: TSan + repeated deterministic comparisons.
- Nondeterministic ordering
  - Likelihood: medium; Impact: high.
  - Mitigation: strict frame-number commit order.
  - Detection: byte-for-byte CSV/PDB comparisons vs serial.
- Oversubscription/perf regression on small workloads
  - Likelihood: medium; Impact: medium.
  - Mitigation: default `num_threads=1`; bounded queue; benchmark threshold checks.
- Exception propagation complexity from worker tasks
  - Likelihood: medium; Impact: medium.
  - Mitigation: normalize worker errors to frame warnings; keep fatal setup errors on main thread.
- API drift across interfaces
  - Likelihood: low; Impact: medium.
  - Mitigation: update C++ header, CLI option, Python binding, and tests in one PR.

## 8. Rollout and Migration Plan
- Backward compatibility:
  - Preserve existing call patterns by adding trailing optional parameter with default `1`.
- Incremental rollout:
  - Step 1: Signature + plumbing + no behavior change.
  - Step 2: Refactor helpers under serial mode.
  - Step 3: Enable parallel mode behind `num_threads>1`.
- Validation gates:
  - Gate merge on passing equivalence tests (serial vs parallel) and docs sync.

## 9. Deliverables Checklist
- Code changes:
  - Refactored `analyze_trajectory_files(...)` with helper decomposition.
  - Optional `num_threads` support across C++/CLI/Python.
- Benchmarks/perf checks:
  - Baseline and post-change runtime comparison (1 vs 2 threads).
- Tests:
  - Added/updated API, CLI, and Python tests for thread parameter + equivalence + invalid input.
- Docs:
  - Updated `codebase-analysis-docs/codebasemap.md`.
  - Updated `codebase-analysis-docs/assets/*.mmd` for revised trajectory flow/concurrency.
- Release notes:
  - Optional changelog note describing new `num_threads` parameter and deterministic output guarantee.
