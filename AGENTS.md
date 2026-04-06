# Rules

- this repo is the canonical app source for the rebuilt v0.72a line
- `community/v0.72a` is the imported baseline and `bb/v0.72a/main` is the maintained integration branch
- accepted `build`, `test`, and `bugfix` states are cut from the integration line only after review
- the first post-community commit must remain the global source-encoding normalization commit
- place changes at the earliest layer where they are true, then let later milestones inherit them
- keep commits isolated by behavior and avoid mixing baseline, seam, and bugfix work
- do not reintroduce workspace orchestration or dependency policy into this repo root
- build, dependency materialization, and local status tracking are owned by the release workspace under `C:\prj\p2p\eMule\workspaces\v0.72a`
- shared tests live in `C:\prj\p2p\eMule\repos\eMule-build-tests`
