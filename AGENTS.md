# Rules

- this repo is the canonical app source for the rebuilt v0.72a line
- the authoritative branching policy lives in `EMULE_WORKSPACE_ROOT\..\eMulebb-setup\BRANCHING.md`; follow it over local habit or stale branch names
- `community/v0.72a` is the imported baseline and `main` is the maintained integration branch
- active release lines are promoted from `main`; use `eMulebb-setup\BRANCHING.md` as the branching source of truth
- the first post-community commit must remain the global source-encoding normalization commit
- place changes at the earliest layer where they are true, then let later milestones inherit them
- keep commits isolated by behavior and avoid mixing baseline, seam, and bugfix work
- do not reintroduce workspace orchestration or dependency policy into this repo root
- build, dependency materialization, and local status tracking are owned by the canonical workspace rooted at `EMULE_WORKSPACE_ROOT`
- shared tests live in `EMULE_WORKSPACE_ROOT\repos\eMule-build-tests`
- do not start new work on `stale/*` branches
- do not use the canonical repo checkout as the normal editing checkout for app work; use the active worktrees instead
- normal app work belongs in these worktrees:
  - `eMule-main` for `main`
  - `eMule-v0.72a-build` for `release/v0.72a-build`
  - `eMule-v0.72a-bugfix` for `release/v0.72a-bugfix`
- treat the canonical repo checkout as the branch store and maintenance checkout unless a task explicitly requires operating there
