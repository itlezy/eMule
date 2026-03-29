# Resume

## Last Chunk

- Removed Upload SpeedSense end-to-end as a breaking cleanup pass.
- Deleted the dedicated USS controller and project wiring:
  - removed `srchybrid\LastCommonRouteFinder.*`
  - removed app startup/shutdown ownership in `Emule.*`
  - removed traceroute host feeders from `ClientList` and `ServerList`
- Removed USS-driven runtime behavior:
  - removed status-bar USS pane and ping display from `EmuleDlg` and `MuleStatusBarCtrl`
  - removed queue/throttler integration and slot-veto logic from `UploadQueue` and `UploadBandwidthThrottler`
  - removed the connection-page fast-reaction hook in `PPgConnection`
- Removed USS preference state and UI:
  - deleted USS/`MinUpload` fields, accessors, and INI persistence from `Preferences.h/.cpp`
  - deleted the USS tree group and settings plumbing from `PPgTweaks.h/.cpp`
- Removed USS-only resources:
  - deleted `IDS_DYNUP*` and `IDS_USS_*` from `srchybrid\Resource.h`
  - deleted the matching entries from `srchybrid\emule.rc` and all `srchybrid\lang\*.rc` files
- Updated docs so they no longer describe USS or `MinUpload`:
  - `docs\BROADBAND.md`
  - `docs\PREFERENCES.md`
  - `docs\MOVETOCMAKE.md`
  - `docs\REPONETWO.md`
  - `docs\THREADING.md`
- Rebuilt successfully with `..\23-build-emule-debug-incremental.cmd`.
- WIP commits created:
  - `6b6c84c` `WIP remove USS runtime and preference plumbing`
  - `2603066` `WIP purge USS resources and documentation`

## Current State

- The app no longer builds or ships the USS traceroute/ping worker.
- The Tweaks page no longer exposes USS settings.
- The main window status bar no longer reserves or updates a USS pane.
- Upload budgeting now depends on configured upload capacity and active upload limit, without live USS allowance.
- A repo-wide search for `LastCommonRouteFinder`, `IDS_DYNUP*`, `IDS_USS_*`, `MinUpload`, and `USS*` config keys in this submodule is clean.

## Next Chunk

- Runtime-smoke the debug build and confirm:
  - no USS section in Tweaks
  - no extra USS status-bar pane
  - upload-limit changes still behave sanely with finite and unlimited caps
- If upload pacing regressed with `MaxUpload = UNLIMITED`, tune the unlimited-path estimator in `UploadBandwidthThrottler` now that USS is gone.
