# 26-02-27 Trajectory Draw Model PDB

## 1. Problem Framing

### Goal
Add trajectory-mode draw support so that when the user runs:

- `watpocket <topology> <trajectory.nc> --resnums ... -o out.csv -d hull_models.pdb`

`watpocket` writes a multi-model PDB where each successful trajectory frame contributes one convex-hull model using PDB `MODEL ... ENDMDL` blocks.

### In Scope
- Enable `-d/--draw` in two-input trajectory mode when draw path ends with `.pdb`.
- Keep existing trajectory CSV behavior on `-o` (mandatory in trajectory mode) unchanged.
- Write hull-only PDB content per model (`ANA` atoms + `CONECT`), not the full molecular system.
- Use 1-based model numbering in trajectory order.
- On per-frame hull failures, skip the frame and print a warning to `stderr` instead of aborting.

### Out of Scope
- Trajectory draw output as `.py`.
- New trajectory formats beyond `.nc`.
- Including protein/water coordinates in draw PDB.
- Changing hull geometry algorithm or water-identification heuristics.

### Assumptions
- Existing requirement remains: `-o/--output` is mandatory in trajectory mode.
- `-d` and `-o` are independent outputs (either/both workflow semantics preserved; `-o` still required by trajectory mode contract).
- Skipped frames are omitted from draw PDB and omitted from CSV/statistics calculations.
- Model serial equals trajectory frame number (1-based), so skipped frames can create numbering gaps.
- Current fixed-column `ATOM` formatting and `CONECT` adjacency rules remain unchanged.

## 2. Success Criteria (Measurable)

### Correctness
- Trajectory mode accepts `-d <name>.pdb` and produces a valid PDB with:
  - header remark,
  - repeated `MODEL n` / hull `ATOM` / `CONECT` / `ENDMDL`,
  - final `END`.
- Each model’s hull coordinates match per-frame hull computation.
- `MODEL` numbering is 1-based and follows frame index.
- Frame-level hull errors do not terminate run; warning emitted once per skipped frame to `stderr`.

### Determinism
- For identical inputs/options, generated model blocks and bond lists are byte-stable except for warning order tied to frame order.

### Performance
- Additional draw overhead remains linear in hull vertex/edge count per successful frame.
- No O(total_frames) coordinate buffering; streaming write only.

### Resource
- Memory remains bounded to per-frame structures plus small accumulators.

### Portability/Maintainability
- Works across current supported build matrix with existing C++17 toolchain.
- CLI help/errors and README/codebase map updated consistently.

## 3. Architecture Sketch

### Modules/Responsibilities
- `main()` trajectory branch:
  - parse/validate options,
  - iterate frames,
  - compute hull + waters,
  - write CSV/stats,
  - optionally append trajectory draw PDB models.
- New draw writer helpers in `src/watpocket/main.cpp`:
  - `write_hull_pdb_model(std::ostream&, const HullData&, std::size_t model_serial)`
  - optional open/close helpers for trajectory draw file preamble/final `END`.

### Dataflow
1. Load topology + trajectory sanity checks.
2. Open CSV (`-o`) and optional draw PDB (`-d .pdb`) once.
3. For each frame index `i` (1-based):
   - build CA points,
   - build hull,
   - classify waters,
   - write CSV row,
   - append `MODEL i` block if draw enabled.
4. On frame computation failure:
   - log warning to `stderr` with frame index + reason,
   - skip CSV/model emission for that frame,
   - continue.
5. After loop:
   - finalize draw PDB with `END`,
   - print CSV path + stats summary + processed/skipped counts.

### Concurrency/Memory/Error Handling
- Concurrency: unchanged single-thread sequential frame processing.
- Memory: streaming; no persistent per-frame geometry retained.
- Errors:
  - fatal: CLI misuse, topology/trajectory file open failure, initial atom-count mismatch, output-file open failure.
  - non-fatal: per-frame geometry/classification failure in trajectory loop (warn + continue).

## 4. Work Breakdown Structure (WBS)

### Phase 0: Baseline and Behavior Lock
- Changes
  - Capture current trajectory and draw behavior with existing tests.
- Validation
  - Run `ctest --test-dir build --output-on-failure`.
- Performance
  - Record baseline runtime for one WCN trajectory invocation.
- Exit
  - Baseline green; known failure path (`--draw` in trajectory mode) documented.

### Phase 1: CLI Contract Update for Trajectory Draw
- Changes
  - Update argument validation in `main()`:
    - in trajectory mode, allow `-d` only when extension is `.pdb`.
    - reject `.py` in trajectory mode with explicit error.
    - preserve `-o` mandatory rule.
- Tests
  - Update/add CLI tests in `test/CMakeLists.txt`:
    - trajectory accepts `-d model.pdb` with `-o`.
    - trajectory rejects `-d model.py`.
- Performance
  - None (validation-only).
- Exit
  - CLI tests pass and error text is stable.

### Phase 2: Multi-Model Trajectory PDB Writer
- Changes
  - Implement model-level writer for hull data using existing fixed-column `ATOM` and `CONECT` rules.
  - Add `MODEL`/`ENDMDL` wrapper and final `END` handling.
  - Integrate into trajectory loop, writing one model per successful frame.
- Tests
  - Functional test: run trajectory with `-d out.pdb`; assert file contains `MODEL`, `ENDMDL`, `END`.
  - Assert first model number is `1`.
- Performance
  - Compare runtime with and without `-d` on same trajectory.
- Exit
  - Generated draw PDB opens in PyMOL/VMD and models align with expected frame hulls.

### Phase 3: Skip-on-Failure Frame Policy
- Changes
  - Refactor per-frame `try/catch` in trajectory loop:
    - catch frame compute exceptions,
    - emit `stderr` warning with frame id,
    - continue loop.
  - Track counters: total frames read, successful frames written, skipped frames.
  - Update stdout summary to include skipped frame count.
- Tests
  - Add regression test with a controlled failing frame case (fixture or synthetic trajectory) verifying:
    - non-zero skipped count,
    - warning emitted,
    - output files still finalized.
- Performance
  - Confirm no significant overhead from warning/counter path.
- Exit
  - Runs complete even with frame-level failures; outputs are well-formed.

### Phase 4: Docs + Hardening
- Changes
  - Update:
    - `README.md` (trajectory `-d .pdb` semantics),
    - `codebase-analysis-docs/codebasemap.md`,
    - `codebase-analysis-docs/assets/*.mmd` if dataflow/CLI flow diagrams change.
- Tests
  - Re-run full `ctest` and one manual WCN scenario.
- Performance
  - Spot-check no regression on representative trajectory.
- Exit
  - Code/docs synchronized and all tests pass.

## 5. Testing & Verification Plan

### Unit/Small Tests
- Extension validation helpers for trajectory draw mode.
- PDB model serializer formatting checks (line prefixes, required records).

### Integration Tests
- End-to-end trajectory run with `-o` and `-d out.pdb`:
  - CSV produced,
  - trajectory draw PDB produced with model blocks.

### Regression/Golden Checks
- Parse generated model PDB and verify:
  - monotonic model IDs (frame-index based),
  - each model has `ATOM` and `CONECT`,
  - final `END` present.

### Determinism Checks
- Run same command twice; compare draw PDB and CSV for identical outputs.

### Sanitizers
- If sanitizer configs are enabled in local presets, run ASan/UBSan smoke for trajectory draw path.

## 6. Performance Engineering Plan

### Baseline
- Benchmark command using WCN sample trajectory with and without `-d`.
- Measure wall time and output size.

### Profiling Hypotheses
- I/O overhead from frequent formatted writes may dominate for long trajectories.
- Hull compute remains dominant for larger selected-residue sets.

### Optimization Ladder (only if needed)
1. Keep single file handle open for draw output (required).
2. Avoid repeated allocations in model writer (reuse adjacency buffers if practical).
3. Batch line writes through buffered streams.

### Guardrails
- No optimization that changes hull vertex ordering, bond determinism, or PDB column correctness.

## 7. Risk Register + Mitigations

- Risk: PDB model blocks malformed (`MODEL/ENDMDL/END` mismatch).
  - Likelihood: Medium; Impact: High.
  - Mitigation: serializer unit test + integration parse check.
  - Detection: viewer load errors or failing line-pattern tests.

- Risk: skip-on-failure masks systemic errors.
  - Likelihood: Medium; Impact: Medium.
  - Mitigation: explicit per-frame warnings + final skipped count summary.
  - Detection: non-zero skipped count in stdout/stderr.

- Risk: confusion over model numbering when frames are skipped.
  - Likelihood: Medium; Impact: Low.
  - Mitigation: document frame-index numbering and possible gaps.
  - Detection: README + tests expecting frame-index IDs.

- Risk: performance regression from PDB emission on long runs.
  - Likelihood: Medium; Impact: Medium.
  - Mitigation: stream writes, avoid per-frame reopen, benchmark before/after.
  - Detection: runtime delta above expected threshold.

## 8. Rollout / Migration Plan

- Backward compatibility:
  - Single-structure draw behavior unchanged.
  - Trajectory mode gains optional `.pdb` draw output.
  - Existing `-o` requirement unchanged.
- Incremental rollout:
  1. land CLI validation changes,
  2. land serializer + integration,
  3. land skip policy,
  4. update docs/tests.
- Validation gates:
  - all tests green,
  - manual WCN command validates multi-model output in viewer.

## 9. Deliverables Checklist

- Code changes (high-level)
  - trajectory-mode draw validation and model-PDB writing path.
  - per-frame skip-and-warn logic.
  - summary counters for successful/skipped frames.
- Benchmarks added/updated
  - before/after timing notes for trajectory with optional draw.
- Tests added/updated
  - CLI validation and trajectory draw output checks.
  - skipped-frame regression.
- Docs added/updated
  - README + codebase map (+ diagrams if changed).
- Release/changelog note
  - trajectory draw `.pdb` now supports multi-model hull output with frame skipping warnings.
