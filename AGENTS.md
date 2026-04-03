# AGENTS.md

## Purpose
This repository contains `watpocket`, a C++ project that integrates:
- `chemfiles` for molecular structure/trajectory IO
- `CGAL` for convex hull and geometric computation

## Environment Setup
Before running any build commands, activate the virtual environment:
```bash
source .venv/bin/activate
```

## Required Workflow
1. Read the current codebase map before making changes:
   - `codebase-analysis-docs/codebasemap.md`
2. Read the relevant source files you will modify.
3. Make the requested code changes.
4. After every change, immediately update the codebase documentation so it stays in sync:
   - `codebase-analysis-docs/codebasemap.md`
   - `codebase-analysis-docs/assets/*.mmd` when architecture/data flow/boundaries change
5. Keep documentation updates in the same change set as the code change.

## Planning Requests
- If the user asks to create a plan (or to plan implementation work), you must use the planner agent instructions in:
  - `.codex/agents/planner.md`

## Skill Usage Requirements
For all tasks in this repository, use these skills proactively:
- `cgal` skill: for convex hull logic, geometric kernels, robustness, and CGAL API usage
- `chemfiles` skill: for molecular file IO, frame/topology handling, residue/atom access, and trajectory-related work

When a task touches geometry or molecular IO, load and follow both skill instructions before implementation.

## Scope and Consistency
- Preserve existing project constraints and build conventions.
- Keep `watpocket` as the primary binary target.
- Do not leave code and docs out of sync; documentation maintenance is mandatory after each modification.
