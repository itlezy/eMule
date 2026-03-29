# Resume

## Last Chunk

- Removed the legacy first-start wizard from startup, the hot menu, and the project build.
- Deleted `srchybrid\PShtWiz1.cpp` so fresh installs now go straight to the main UI.
- Modernized the remaining connection helper into a `Presets` dialog with generic speed tiers instead of obsolete ISP-era entries.
- Kept the bandwidth tuning behavior focused on capacities, limits, max connections, and max sources per file.
- Verified with `..\23-build-emule-debug-incremental.cmd`.

## Current State

- Fresh installs no longer open a first-run wizard.
- Preferences > Connection now exposes `Presets...` instead of `Wizard...`.
- The connection presets dialog starts from the current bandwidth settings, offers generic modern tiers, and still applies the existing tuning heuristics.
- The build is green, and the only remaining uncommitted change in the worktree is the unrelated existing `AGENTS.md` modification.

## Next Chunk

- Manually exercise the new `Presets...` dialog in the UI and confirm preset selection, custom entry, and apply behavior feel right.
- Decide whether the button/control IDs and internal class naming should also be renamed from `Wizard` to `Presets`, or left as-is to minimize churn.
- Add localized `IDS_PRESETS` translations if broader language coverage is needed.
