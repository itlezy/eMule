# Resume

## Last Chunk

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

- If status-bar polish continues, consider surfacing the resolved bind interface name in the Network Info dialog or connected-pane tooltip without changing the restored legacy pane widths.
- If VPN safety work starts, implement the external watchdog described in `docs\EXTRAS_VPNKILLSWITCHDESIGN.md` instead of reviving the in-app bind kill switch.
- If bind safety continues inside eMule, the next contained step would be runtime bind-target monitoring; the current guard is startup-only.
- Continue with a similarly scoped modernization block:
  - take `WWMOD_050` next for typed time-conversion helper cleanup, or
  - return to the deferred CRT-hardening pair `WWMOD_019` plus `WWMOD_008`
