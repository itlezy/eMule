# Resume

## Last Chunk

- Normalized dependency-header resolution back into `srchybrid\emule.vcxproj` by adding `ResizableLib`, `zlib`, and `Crypto++` roots to `AdditionalIncludeDirectories` for both `Debug` and `Release`.
- Rewrote the remaining dependency `#include` directives in `srchybrid` away from hard-coded sibling traversals such as `../../eMule-zlib/...`, `../../eMule-cryptopp/...`, and `../../eMule-ResizableLib/...`.
- Fixed the follow-up header-name collisions caused by generic names in subdirectories by qualifying the project-local `srchybrid/Resource.h`, `srchybrid/MD4.h`, and `srchybrid/Opcodes.h` includes where needed.
- Rebuilt the target workspace with `..\23-build-emule-debug-incremental.cmd` until the project linked cleanly again after the include-root ordering and local-header fixes.
- Verified the successful build log is down to `0` occurrences of `warning C4464`.
- Removed the last resizable-dialog dependency from Network Information by switching it from `CResizableDialog` to plain `CDialog`, matching the already-fixed-size resource and avoiding any further resize behavior from the class itself.
- Rebuilt the target workspace with `..\23-build-emule-debug-incremental.cmd`.
- Fixed the follow-up layout regression in the Network Information dialog by keeping the 3 summary panels but converting the screen back to a deliberate fixed-size layout instead of a pseudo-resizable one.
- Removed the dialog's anchor/save-restore behavior so stale saved rectangles from the broken layout can no longer reopen the screen at an invalid size.
- Tightened the resource geometry, moved the bind target onto the compact client row, and added ellipsis-safe static value controls so long bind/server/hash text no longer tramples adjacent fields.
- Rebuilt the target workspace with `..\23-build-emule-debug-incremental.cmd`.
- Re-ran the targeted shared doctest filter `--test-case=*Status bar IP pane*` successfully (`3` cases, `7` assertions).
- Reworked the Network Information dialog into a larger summary-plus-details view with grouped client, eD2K, and Kademlia sections, while keeping the legacy rich-text report as the detailed pane.
- Added `Reload` and `Copy` actions to the Network Information dialog and made the copied plain-text report include both the new summary rows and the legacy detailed dump.
- Surfaced the resolved bind target in the Network Information client summary and aligned the dialog's eD2K public-IP formatting with the stored-byte-order formatter already used by the status bar IP pane.
- Added a new `Tools > Network Information...` command in the dynamic Tools popup and wired it to reuse the existing modal dialog entry point.
- Updated the shared `status_bar_info` regression coverage so the stored-byte-order IPv4 formatter used by both the status bar and the dialog remains pinned.
- Built the target workspace with `..\23-build-emule-debug-incremental.cmd`.
- Built the shared test binary and ran the targeted doctest filter `--test-case=*Status bar IP pane*` successfully (`3` cases, `7` assertions).
- The full shared test suite still fails in the pre-existing sampled temp-file scans from `mapped_file_reader.tests.cpp` because `recursive_directory_iterator` hits a missing path during the filesystem-backed parity/benchmark cases.
- Completed the remaining modern-limits follow-through for `FEAT_013`, `FEAT_015`, `FEAT_016`, and `FEAT_017` with the branch decision to keep `MaxConnections` at `500`.
- Raised the default file buffer size to `64 MiB`, extended the Tweaks file-buffer slider to `512 MiB` via a mixed KiB/MiB mapping helper, and kept the existing `120s` file-buffer time limit.
- Raised the queue default to `10000`, raised the per-file soft and UDP source caps to `1000` and `100`, and removed the now-dead source-cap macros from `Opcodes.h`.
- Centralized the remaining modern fixed defaults in `ModernLimits.h`, including the conservative `MaxConnections=500` branch default and the completed queue/source/file-buffer targets.
- Added shared regression coverage for the new modern-limits defaults and the file-buffer slider mapping in `eMule-build-tests`.
- Updated `docs\FEATURE-MODERN-LIMITS.md` and `docs\INDEX.md` so they reflect the actual completed modern-limits state instead of the stale `2 MiB`, `5000`, and `1000 planned` notes.
- Added a dedicated status-bar IP pane that shows the runtime bind target and public IP separately from the existing eD2K/Kad connection-state pane.
- Wired the new pane to refresh on startup, normal connection-state changes, and delayed public-IP discovery, and reused the existing Network Info dialog on double-click.
- Added a shared `StatusBarInfo` formatting helper so the compact pane text and single-line tooltip stay deterministic and regression-testable.
- Moved the status-bar IP pane to the left of the `Users` pane and aligned its public-IP source with the eD2K-reported public address instead of the broader Kad-aware fallback.
- Restored the original pre-IP pane widths for `Users`, `UpDown`, `Connected`, and `Chat`, and kept the new prepended `IP` pane by taking its width only from the elastic log area.
- Restored the original full `Up:` / `Down:` status-bar transfer text and removed the compact fallback text for the IP pane.
- Fixed the status-bar public-IP formatter to use the app's stored IPv4 byte order, so the pane and tooltip no longer display the eD2K-reported address with reversed octets.
- Captured the decision to keep VPN kill-switch behavior out of eMule itself and move it into a separate external watchdog tool design in `docs\EXTRAS_VPNKILLSWITCHDESIGN.md`.
- Added a startup bind guard that keeps the whole session offline when an explicit bind interface or bind IP is unavailable at launch, including address-only selections that are no longer present on any live interface.
- Blocked startup socket bring-up, startup UPnP, autoconnect, manual eD2K/Kad connect actions, and server UDP socket creation while the startup bind guard is active, and added shared regression coverage for the bind policy.
- Added a `Settings > Connection` checkbox to opt into that startup bind guard, persisted in `preferences.ini`, with restart-required semantics like the other bind-selection changes.
- Updated the connection dialog so selecting a concrete bind interface or bind IP automatically checks that startup bind-block option in the current session.
- Added `helpers\e2e-vpn-launch.ps1` to run a clean `%LOCALAPPDATA%\eMule` end-to-end session with emule-security `nodes.dat`/`server.met`, recursive shared-directory seeding, VPN-IP binding, and disk-backed verbose logging.
- Verified the helper against `C:\tmp\videodupez\` with bind address `10.54.218.144`: the app wrote `eMule.log` and `eMule_Verbose.log`, loaded 153 Kad contacts from `nodes.dat`, connected to `eMule Sunrise` and `eMule Security`, and started hashing the recursive share tree.
- Fixed `CKnownFileList::ShouldPurgeAICHHashset` so orphaned known2.met AICH entries are treated as purgeable instead of tripping a debug-only assertion, and added a shared regression seam for the purge decision.
- Fixed the debug-only `_tmakepathlimit` overflow assertion so overlong runtime paths now fail cleanly instead of stopping the app in `OtherFunctions.cpp`, and guarded log/perf-log path rotation against empty fallback paths.
- Updated the docs/status tracking to reject `BUG_002`, `GAP_004`, `FEAT_010`, `FEAT_024`, `WWMOD_026`, `WWMOD_027`, `WWMOD_029`, and `WWMOD_041` in the audit and index markdown files.
- Completed `FEAT_018` by moving connection and download timeouts into persisted preferences, reducing their defaults to `30s` and `75s`, and tightening the remaining fixed UDP/source-latency constants to `20s` and `15000`.
- Completed the active `FEAT_019` exposure work by adding `Connection timeout`, `Download timeout`, and `Per-client upload cap` to `Preferences > Tweaks` while reusing the existing `TCP/IP` and `Broadband` groups.
- Raised the `MaxSourcesPerFile` default to `600`, removed the now-dead timeout/upload-cap compile-time macros from `Opcodes.h`, and kept `KADEMLIAASKTIME`, `ServerKeepAliveTimeout`, and `UPNP_TIMEOUT` unchanged.
- Added the shared `ModernLimits.h` defaults/normalization helper and new shared regression coverage in `eMule-build-tests` for the FEAT_018/019 default values, timeout normalization, and upload-cap math.
- Built the target workspace with `..\23-build-emule-debug-incremental.cmd`.
- Built and ran the shared test suite with `..\36-run-emule-tests-debug.cmd`.

## Current State

- `srchybrid` now resolves dependency headers through project include roots again instead of embedding sibling-workspace `..\..` paths for `zlib`, `Crypto++`, and `ResizableLib`.
- The successful `..\23-build-emule-debug-incremental.cmd` build log is now clean for `warning C4464`.
- One internal subdirectory include path was intentionally qualified through `srchybrid/...` to avoid generic-name collisions with dependency headers while keeping the dependency include directories ordered correctly.
- The Network Information dialog is now plain `CDialog` plus fixed-size resource layout, so there is no remaining resizable helper behavior attached to that screen.
- The Network Information dialog is now intentionally fixed-size, with the 3-panel summary preserved but packed into a tighter geometry that does not depend on resize anchors.
- The screen no longer restores the previous broken saved rectangle, so old bad geometry should stop resurfacing for users who opened the earlier layout.
- `Tools > Network Information...` now opens the same modal dialog that the status-bar panes already used on double-click.
- The Network Information dialog now has an at-a-glance summary for client identity, bind target, eD2K state, and Kad state above the preserved legacy details report.
- The dialog's eD2K public-IP display now uses the same stored-byte-order IPv4 formatter as the status-bar IP pane, avoiding divergent text for the same runtime address.
- Shared regression coverage now explicitly checks that formatter, but the full shared suite remains noisy because the filesystem-backed `mapped_file_reader` sampling cases are currently environment-sensitive.
- The main status bar now has a dedicated IP pane with compact `Bind/Public` runtime address visibility and tooltip expansion for the same data.
- The prepended IP pane now uses the original status-bar sizing for all legacy panes and shows full `B:...|P:...` text without shortening.
- The public-IP side of the status-bar pane now renders with the same stored-byte-order convention as the rest of the app instead of reversing octets.
- The VPN safety direction is now documented as an external process watchdog rather than an in-process bind kill switch.
- Startup now fails closed into an offline session when the configured bind target is missing and the new connection-page startup bind-block option is enabled.
- `FEAT_018` is implemented with persisted connection/download timeout defaults and shorter fixed UDP/source-latency constants.
- `FEAT_013` through `FEAT_019` are now aligned with the current branch choices, including `MaxConnections=500`, `FileBufferSize=64 MiB`, `QueueSize=10000`, and the completed queue/source caps.
- `FEAT_019` remains the active preferences-exposure shape; the modern-limits work is now mostly documentation and validation rather than missing runtime defaults.
- `WWMOD_008`, `WWMOD_019`, and `WWMOD_050` remain available as the next low-risk modernization candidates.

## Next Chunk

- If the include cleanup continues, the next safe pass is `.rc` include normalization only; the C/C++ dependency traversal cleanup is complete.
- If more include-path cleanup is wanted, audit other generic local header names from subdirectories before reordering include roots again, since `resource.h` / `MD4.h` collisions were the only blockers in this pass.
- If more Network Information polish is needed, the next safe step is visual refinement only: adjust spacing/text priority inside the fixed-size panels or add tooltips for elided summary values without reintroducing resize behavior.
- If the dialog polish continues, the next contained step is to let users copy individual summary values or add a one-click export/save action without touching the existing report generator.
- If automated coverage for this area needs to grow, extract the new dialog summary formatting into a small helper that can be tested directly from `eMule-build-tests` instead of relying on the shared `StatusBarInfo` helper alone.
- If the shared suite needs to go green, investigate the `mapped_file_reader.tests.cpp` sampled temp-file enumeration failures before treating unrelated UI changes as test regressions.
- If status-bar polish continues, consider surfacing the resolved bind interface name in the Network Info dialog or connected-pane tooltip without changing the restored legacy pane widths.
- If VPN safety work starts, implement the external watchdog described in `docs\EXTRAS_VPNKILLSWITCHDESIGN.md` instead of reviving the in-app bind kill switch.
- If bind safety continues inside eMule, the next contained step would be runtime bind-target monitoring; the current guard is startup-only.
- Continue with a similarly scoped modernization block:
  - take `WWMOD_050` next for typed time-conversion helper cleanup, or
  - return to the deferred CRT-hardening pair `WWMOD_019` plus `WWMOD_008`
