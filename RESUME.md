# Resume

## Last Chunk

- Added a dedicated status-bar IP pane that shows the runtime bind target and public IP separately from the existing eD2K/Kad connection-state pane.
- Wired the new pane to refresh on startup, normal connection-state changes, and delayed public-IP discovery, and reused the existing Network Info dialog on double-click.
- Added a shared `StatusBarInfo` formatting helper so the compact pane text and single-line tooltip stay deterministic and regression-testable.
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
- `FEAT_018` is implemented with persisted connection/download timeout defaults and shorter fixed UDP/source-latency constants.
- `FEAT_019` is effectively complete for the active modern-limits knobs; the remaining advanced limit controls stay in their existing Tweaks groups or other existing UI pages.
- `FEAT_017` is still partial because `QueueSize` remains `5000` even though `MaxSourcesPerFile` is now `600`.
- `WWMOD_008`, `WWMOD_019`, and `WWMOD_050` remain available as the next low-risk modernization candidates.

## Next Chunk

- If status-bar polish continues, consider surfacing the resolved bind interface name in the Network Info dialog or connected-pane tooltip without widening the new IP pane.
- Continue with a similarly scoped modernization block:
  - take `WWMOD_050` next for typed time-conversion helper cleanup, or
  - return to the deferred CRT-hardening pair `WWMOD_019` plus `WWMOD_008`
- Separately, `FEAT_017` still needs the queue-size default increase to `10000` if the modern-limits track continues before the next WWMOD item.
