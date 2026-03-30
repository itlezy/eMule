# Resume

## Last Chunk

- Created the shared sibling `eMulebb-tests` git repo and wired it into both the current workspace and the new oracle workspace as a `tests` submodule.
- Tagged the parent pre-refactor baseline, branched both parent and pinned `eMule` checkouts as `v0.72a-oracle`, cloned `C:\prj\p2p\eMulebb-oracle`, and added parent-level wrappers to build and run the shared test project.
- Verified that both workspaces build and run the same shared doctest binary, and that the live diff script reports the first observed behavior difference between the modern branch and the oracle branch.

## Current State

- `C:\prj\p2p\eMulebb\tests` and `C:\prj\p2p\eMulebb-oracle\tests` now point at the same shared test repository content.
- `tests\scripts\run-live-diff.ps1` already exercises both workspaces end to end and records advisory output diffs.
- The current shared suite is still small and ring-focused; it proves the cross-worktree harness but does not yet cover the socket / packet / protocol audit targets.

## Next Chunk

- Expand the shared tests repo from the ring smoke test into real protocol coverage, starting with packet/header framing and malformed-length boundary cases that can run against both workspaces.
- Add a normalized machine-readable result format to the live diff path so branch differences are easier to review than raw doctest console text.
- Decide which observed oracle differences are intentional and which should become actionable regressions before tightening any failure policy.
