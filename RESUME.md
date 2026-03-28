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

## Current State

- `Options > Connection` and `Options > Web Interface` have more vertical room for the new bind selectors.
- The overall Options sheet now uses a larger common page size to reduce crowding in the extended tree and connection pages.
- The `Connection` page now has cleaner spacing between the client-port, bind, source/connection, and network sections, with wider bind controls and lower groups.
- The UPnP option is now grouped with the port fields, followed by port randomization and then the bind selectors.
- The dialog opens centered on the active screen instead of using generic window centering.

## Next Chunk

- Runtime-test the taller Options dialog on the main pages, especially `Connection`, `Web Interface`, and `Tweaks`.
- Check whether the `Connection` page still needs another pass after the screenshot-driven width redistribution.
- Verify active-monitor centering on single-monitor and multi-monitor setups.
- Runtime-test the new bind selectors with multiple network adapters and confirm the expected P2P vs Web UI separation.
