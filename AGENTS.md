# Rules

- this repo is the canonical app source for the rebuilt v0.72a line
- the authoritative workspace policy lives in `EMULE_WORKSPACE_ROOT\repos\eMule-tooling\docs\WORKSPACE_POLICY.md`; follow it over local habit or stale branch names
- `community/v0.72a` is the imported baseline and `main` is the maintained integration branch
- accepted `build`, `test`, and `bugfix` states are cut from the integration line only after review
- the first post-community commit must remain the global source-encoding normalization commit
- place changes at the earliest layer where they are true, then let later milestones inherit them
- keep commits isolated by behavior and avoid mixing baseline, seam, and bugfix work
- do not reintroduce workspace orchestration or dependency policy into this repo root
- workspace-wide rules about branches, worktrees, setup ownership, and dependency pins belong in the central workspace policy document, not here
- do not start new work on `stale/*` branches
