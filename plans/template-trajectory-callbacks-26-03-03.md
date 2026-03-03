# Template Trajectory Callbacks - 26-03-03

## 1. Problem Framing
- Goal: replace function-pointer-based trajectory callbacks (`TrajectoryCallbacks`) with a templated, variadic callback API for `analyze_trajectory_files`, supporting separate frame/warning callback channels and allowing exceptions from user callbacks to propagate.
- In scope:
  - Public API break in `include/watpocket/watpocket.hpp`.
  - Library implementation refactor in `src/watpocket_lib/watpocket.cpp` to consume a generic callback dispatcher.
  - CLI migration in `src/watpocket/main.cpp`.
  - Codebase documentation updates (`codebase-analysis-docs/codebasemap.md`, `assets/data-flow.mmd`).
- Out of scope:
  - Python binding implementation.
  - Backward compatibility adapter for existing `TrajectoryCallbacks` callers.
  - Changes to geometry (CGAL convex hull) and chemfiles parsing behavior.
- Assumptions:
  - C++23 toolchain remains available (already configured).
  - Current topology/trajectory constraints remain unchanged (`.nc` trajectory, existing topology backends).
  - Public API consumers accept source-level migration.

## 2. Success Criteria
- Correctness:
  - Successful frame path invokes all registered frame callbacks in call-site order.
  - Frame-local failures invoke all registered warning callbacks in call-site order and preserve skip accounting.
  - Exceptions thrown from callbacks propagate out of `analyze_trajectory_files`.
  - Existing CSV/draw/statistics behavior remains unchanged for non-callback outputs.
- Performance:
  - No additional per-frame heap allocation introduced by callback dispatch.
  - Callback dispatch overhead is bounded to iteration over registered callback wrappers.
- Maintainability:
  - Public callback API is header-only and type-safe.
  - Internal trajectory loop remains non-templated and localized in `watpocket.cpp`.
  - Documentation reflects callback API change.

## 3. Architecture Sketch
- Public API layer:
  - Add tagged callback wrappers:
    - `on_trajectory_frame(F&&)`
    - `on_trajectory_warning(F&&)`
  - Add variadic template overload:
    - `template <typename... CallbackSpecs> analyze_trajectory_files(..., CallbackSpecs&&... callback_specs)`
  - Remove `TrajectoryCallbacks` struct from public API.
- Internal library layer:
  - Add non-templated callback sink struct (type-erased, non-owning):
    - `emit_frame(const TrajectoryFrameResult&)`
    - `emit_warning(std::string_view)`
  - Keep trajectory analysis core non-templated and call sink emitters.
- Dispatch model:
  - Template wrapper stores callbacks in tagged tuple wrappers.
  - Builds sink lambdas that iterate callback specs and dispatch by tag.
  - Future Python bindings can target a dedicated non-template entry point (to be added later) or invoke templated API through binding-side C++ adapters.

## 4. Work Breakdown Structure

### Phase 0: Baseline & Interface Design
- Task 0.1: inventory callback call sites and API declarations.
  - Files: `include/watpocket/watpocket.hpp`, `src/watpocket/main.cpp`, `src/watpocket_lib/watpocket.cpp`.
  - Exit criteria: complete map of declarations, call paths, and CLI usage.
- Task 0.2: define tagged callback API and compile-time constraints.
  - Deliverable: concrete tag/wrapper names and invocability requirements.
  - Exit criteria: no ambiguity between frame and warning callbacks.

### Phase 1: Public API Refactor
- Task 1.1: remove function-pointer callback struct and add tagged variadic callback API.
  - Files: `include/watpocket/watpocket.hpp`.
  - Validation: header compiles in library and CLI.
  - Exit criteria: `analyze_trajectory_files` called with 0..N callback specs.
- Task 1.2: add explicit static assertions for callback signature mismatch.
  - Validation: invalid callback signature causes clear compile-time diagnostic.
  - Exit criteria: documented expected callback signatures.

### Phase 2: Library Core Refactor
- Task 2.1: introduce internal callback sink for frame/warning events.
  - Files: `src/watpocket_lib/watpocket.cpp`.
  - Validation: trajectory loop uses sink emitters where old struct callbacks were invoked.
  - Exit criteria: core logic independent of public template machinery.
- Task 2.2: preserve skip/error semantics with callback exceptions allowed to escape.
  - Validation: callback exceptions are not swallowed by frame-local catch block.
  - Exit criteria: core catches analysis errors only, not callback failures.

### Phase 3: CLI Migration
- Task 3.1: replace old `TrajectoryCallbacks` usage in `main.cpp` with tagged callback helper.
  - Files: `src/watpocket/main.cpp`.
  - Validation: warning behavior still prints identical warning lines to `stderr`.
  - Exit criteria: CLI compiles without `TrajectoryCallbacks`.

### Phase 4: Docs, Validation, and Hardening
- Task 4.1: update codebase map and data-flow docs for tagged variadic callbacks.
  - Files: `codebase-analysis-docs/codebasemap.md`, `codebase-analysis-docs/assets/data-flow.mmd`.
  - Exit criteria: docs reference new callback API and callback channels.
- Task 4.2: build and run targeted tests.
  - Commands: configure/build and relevant tests (`watpocket`, API tests).
  - Exit criteria: successful build and passing relevant tests.

## 5. Testing & Verification Plan
- Compile-time checks:
  - Verify valid signatures:
    - frame: callable with `const TrajectoryFrameResult&`
    - warning: callable with `std::string_view`
  - Verify invalid signatures fail at compile time with `static_assert`.
- Runtime checks:
  - CLI trajectory mode with warning callback prints frame-skip warnings.
  - No-callback invocation works as before.
  - Multiple callbacks per event all fire.
  - Callback exception propagation exits analysis immediately.
- Regression checks:
  - CSV output row count equals successful frames.
  - Summary counts unchanged for non-throwing callbacks.

## 6. Performance Engineering Plan
- Baseline:
  - Use existing trajectory-mode run (WCN data) to compare wall-time before/after.
- Measurement:
  - Track per-frame overhead qualitatively by profiling callback-heavy vs callback-free runs.
- Expected impact:
  - negligible for callback-free path.
  - linear in number of registered callbacks for callback-enabled path.
- Guardrails:
  - avoid `std::function` allocations in hot loop.
  - keep callback dispatch state stack-allocated.

## 7. Risk Register + Mitigations
- Risk: callback exceptions get misclassified as frame-local analysis errors and converted to warnings.
  - Likelihood: medium; Impact: high.
  - Mitigation: isolate callback dispatch from frame-local catch and rethrow user callback exceptions.
- Risk: template API creates ambiguous callback matching.
  - Likelihood: medium; Impact: medium.
  - Mitigation: explicit tag wrappers (`on_trajectory_frame`, `on_trajectory_warning`) instead of signature inference.
- Risk: future Python bindings need stable non-templated boundary.
  - Likelihood: high; Impact: medium.
  - Mitigation: keep internal sink/core seam that can be wrapped by a future non-template bridge function.
- Risk: docs drift from implementation.
  - Likelihood: medium; Impact: medium.
  - Mitigation: update map + data-flow in same change set.

## 8. Rollout / Migration Plan
- This is an intentional API break (no compatibility mode).
- Migration path for C++ callers:
  - Replace struct-based callback assignment with variadic tagged callbacks.
  - Example pattern:
    - `on_trajectory_warning([](std::string_view msg){ ... })`
    - `on_trajectory_frame([](const TrajectoryFrameResult& r){ ... })`
- Future Python strategy:
  - Add dedicated non-template bridge API (function-pointer or binding-owned callable adapter) that forwards into the same core callback sink.

## 9. Deliverables Checklist
- [x] Public callback API changed to tagged variadic templates.
- [x] Function-pointer callback struct removed.
- [x] Library core callback sink introduced.
- [x] CLI migrated to new API.
- [x] Codebase docs updated (`codebasemap.md`, `assets/data-flow.mmd`).
- [x] Build/tests run and verified.
