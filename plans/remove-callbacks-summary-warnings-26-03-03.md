# Remove Callbacks, Store Skip Warnings in Summary - 26-03-03

## 1. Problem Framing
- Goal: remove trajectory callback mechanisms entirely and move skipped-frame warning reporting into `TrajectorySummary` returned by `analyze_trajectory_files`.
- Required behavior:
  - No callback API in public or internal interfaces.
  - On frame-local analysis failure, record warning text keyed by frame number in `TrajectorySummary`.
  - Add `has_skipped_frames` flag in `TrajectorySummary`.
  - If all processed frames fail, `analyze_trajectory_files` throws (no summary returned).
  - Fatal setup/read errors (file existence/open/topology mismatch/frame-read failures) remain immediate throws.
  - CLI prints recorded warnings after `analyze_trajectory_files` returns.
- In scope:
  - API changes in `include/watpocket/watpocket.hpp`.
  - Core trajectory loop changes in `src/watpocket_lib/watpocket.cpp`.
  - CLI warning-print behavior changes in `src/watpocket/main.cpp`.
  - Documentation sync in `codebase-analysis-docs/codebasemap.md` and `codebase-analysis-docs/assets/data-flow.mmd`.
- Out of scope:
  - Python bindings implementation.
  - Geometry algorithm changes (CGAL hull semantics).
  - Chemfiles format/topology support expansion.
- Assumptions:
  - C++23 remains available.
  - `TrajectorySummary` can break source compatibility.
  - Unordered warning storage is acceptable; display order can be normalized in CLI if needed.

## 2. Success Criteria (Measurable)
- Correctness:
  - `TrajectorySummary` includes `std::unordered_map<std::size_t, std::string> skipped_frame_warnings` and `bool has_skipped_frames`.
  - For every skipped frame, map contains exactly one entry at that frame key with text:
    `"Warning: skipping frame " + std::to_string(frame) + ": " + error`.
  - `has_skipped_frames == !skipped_frame_warnings.empty()`.
  - If `frames_processed > 0` and `frames_successful == 0`, function throws `watpocket::Error`.
  - Fatal setup/read/topology mismatch errors still throw immediately (unchanged class of failures).
- Performance:
  - No callback dispatch overhead in frame loop.
  - Map insertion only on failed frames.
- Maintainability:
  - Public callback API removed from header.
  - CLI warning output derived solely from returned summary.
  - Docs reflect no callback path.

## 3. Architecture Sketch
- Public API:
  - Remove callback-related public types/functions from `watpocket.hpp` (`on_trajectory_*`, callback-spec templates, callback sink declarations).
  - Keep only one trajectory entrypoint:
    - `TrajectorySummary analyze_trajectory_files(topology, trajectory, selectors, csv_output, draw_output_pdb)`.
  - Extend `TrajectorySummary`:
    - `bool has_skipped_frames = false;`
    - `std::unordered_map<std::size_t, std::string> skipped_frame_warnings;`
- Library core flow:
  - Per-frame compute/writer block catches frame-local compute errors and records warnings in map.
  - Successful frames update stats and presence counts.
  - After loop, if `successful_frames == 0`, throw `Error("all trajectory frames failed analysis")` (or equivalent detailed message).
- CLI flow:
  - After successful return, iterate `summary.skipped_frame_warnings` and print warnings to `stderr`.
  - Optional deterministic output in CLI: copy map entries to vector and sort by frame before printing.

## 4. Work Breakdown Structure

### Phase 0: API and Data Model Design
- Task 0.1: finalize `TrajectorySummary` field additions and removal list for callback API.
  - Files: `include/watpocket/watpocket.hpp`.
  - Exit criteria: single callback-free trajectory API surface.

### Phase 1: Header/API Refactor
- Task 1.1: remove callback API artifacts.
  - Remove callback helper templates and callback sink declarations.
  - Remove callback-based overloads.
- Task 1.2: add summary fields (`has_skipped_frames`, warning map).
  - Include `<unordered_map>` in header.
  - Exit criteria: public header compiles and exposes new summary contract.

### Phase 2: Library Core Refactor
- Task 2.1: simplify `analyze_trajectory_files` internals to callback-free flow.
  - Remove callback paths and callback exception control flags.
  - Record frame-local warning messages in `summary` construction state.
- Task 2.2: implement throw-on-all-fail.
  - After frame loop, throw if no successful frame.
  - Ensure this throw is not triggered on setup/read fatal errors (already thrown earlier).
- Task 2.3: set flags and summary fields consistently.
  - Populate `frames_*`, metrics, warning map, and `has_skipped_frames`.

### Phase 3: CLI Behavior Update
- Task 3.1: remove callback registration and warning callback function from CLI.
- Task 3.2: print warnings from summary after successful return.
  - Optionally sort by frame for deterministic display.

### Phase 4: Documentation + Validation
- Task 4.1: update architecture/data-flow docs to remove callback nodes and add summary-warning path.
- Task 4.2: compile and run full local tests (`ctest --test-dir build`).
- Task 4.3: add/adjust tests for:
  - warning map population,
  - `has_skipped_frames`,
  - throw-on-all-fail semantics.

## 5. Testing & Verification Plan
- Unit/API tests:
  - Add/extend tests in `test/watpocket_api_tests.cpp`:
    - Partial-failure trajectory: map contains failed frame keys/messages; success still returns summary.
    - All-failure trajectory: function throws `watpocket::Error`.
    - Fatal topology/read mismatch still throws before loop or at frame read boundary.
- Integration/CLI tests:
  - Verify warnings are printed from summary after function return.
  - Verify no callback symbol usage remains in CLI.
- Regression:
  - Existing API tests and integration benchmark tests continue to pass.

## 6. Performance Engineering Plan
- Baseline:
  - Measure current trajectory run on WCN test data (wall time).
- Expected impact:
  - Slight simplification in hot path due to removed callback dispatch.
  - Additional unordered_map insertion only for skipped frames.
- Guardrails:
  - Do not allocate/copy warning strings for successful frames.
  - Keep map reserve optional and lightweight; skip if unknown failure rate.

## 7. Risk Register + Mitigations
- Risk: unordered_map iteration nondeterminism causes unstable CLI warning order.
  - Likelihood: high; Impact: medium.
  - Mitigation: sort entries by frame before printing in CLI.
- Risk: throw-on-all-fail hides per-frame diagnostics from caller.
  - Likelihood: medium; Impact: medium.
  - Mitigation: include failed-frame count and representative message in thrown error text.
- Risk: API break for current callback users.
  - Likelihood: certain; Impact: accepted.
  - Mitigation: document migration and remove references consistently.
- Risk: docs drift.
  - Likelihood: medium; Impact: medium.
  - Mitigation: update docs in same change set.

## 8. Rollout / Migration Plan
- Migration strategy:
  - Immediate hard cut to callback-free API.
  - Existing callback call sites must be rewritten to post-process `TrajectorySummary` warnings.
- Developer migration note:
  - Old: pass warning callback and handle warnings during processing.
  - New: call `analyze_trajectory_files(...)`, then inspect/print `summary.skipped_frame_warnings` if no throw.

## 9. Deliverables Checklist
- [x] Callback APIs removed from public header and implementation.
- [x] `TrajectorySummary` extended with warning map + `has_skipped_frames`.
- [x] Core trajectory loop records frame-local warnings in summary state.
- [x] Throw-on-all-fail behavior implemented.
- [x] CLI warning output moved to summary post-processing.
- [x] Codebase docs updated (`codebasemap.md`, `assets/data-flow.mmd`).
- [x] Tests updated/added and passing via `ctest --test-dir build`.
