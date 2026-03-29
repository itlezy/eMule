# Resume

## Last Chunk

- Added separate bind interface and bind address selectors for P2P and Web UI.
- Added a shared IPv4 adapter resolver and kept the runtime bind path on the existing effective bind-address model.
- Kept P2P bind changes restart-only and applied Web UI bind changes by restarting only the web sockets.
- Enlarged the Options dialog again; all `IDD_PPG_*` pages are now `300 x 350`.
- Rebalanced the `Options > Connection` control layout to use the extra height more evenly.
- Widened the Options pages and stretched the `Connection` page horizontally so long bind labels and lower groups are less cramped.
- Refined the lower `Connection` page groups to give captions and field labels more padding and restored a clearer `Connection Limits` heading.
- Moved the `Use UPnP to Setup Ports` checkbox up so it sits with the TCP/UDP port controls instead of the bind section.
- Centered the Options dialog on the active monitor work area when it opens.
- Verified the current UI/layout code with `..\\23-build-emule-debug-incremental.cmd`.
- Wrote a detailed fixed-value modernization plan to `docs\\MODERN_LIMITS.md`.
- Started `docs\\MODERN_LIMITS.md` Phase 1 / Phase 2 work:
  - default `MaxHalfConnections` is now `50`
  - default/fallback `MaxConnectionsPerFiveSeconds` is now `50`
  - legacy fallback per-slot upload target floor is now `32 KiB/s`
  - `UPLOAD_CLIENT_MAXDATARATE` is now `8 MiB/s`
  - default `FileBufferSize` is now `2 MiB`
  - default `FileBufferTimeLimit` is now `120s`
  - default `MinFreeDiskSpace` is now `5 GiB`
  - Tweaks now exposes configurable `UDP receive buffer size (KiB)` and `TCP big send buffer size (KiB)`
  - Tweaks now edits `Min. free disk space` in `GB` instead of `MB`
  - build verified with `..\\23-build-emule-debug-incremental.cmd`

## Current State

- `Options > Connection` and `Options > Web Interface` have more vertical room for the new bind selectors.
- The overall Options sheet now uses a larger common page size to reduce crowding in the extended tree and connection pages.
- The `Connection` page now has cleaner spacing between the client-port, bind, source/connection, and network sections, with wider bind controls and lower groups.
- The UPnP option is now grouped with the port fields, followed by port randomization and then the bind selectors.
- The dialog opens centered on the active screen instead of using generic window centering.
- `docs\\MODERN_LIMITS.md` now contains an execution-ready plan for modernizing stale hard-coded limits using larger fixed defaults instead of adaptive behavior.
- The first fixed-limit modernization chunk is partially implemented and built cleanly.
- `GetTargetClientDataRate()` now has explicit comments describing its heuristic role and fallback path.
- The legacy per-client upload target cap is now `8 MiB/s` instead of `1 MiB/s`.
- Socket-buffer preferences are now persisted and exposed in Tweaks.
- The live UDP socket reapplies the configured receive buffer on Tweaks apply.
- A manual Windows 10/11 firewall helper script is present at `helpers\\firewall-opener.ps1`.
- `IsRunningXPSP2*` and the `WinXPSP2OrHigher` migration key have been removed.

## Next Chunk

- Simplify pre-Windows-10 UI and feature gates.
- Remove or isolate the legacy XP `CFirewallOpener` product path after the helper-script fallback exists.
