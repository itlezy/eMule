# Resume

## Last Chunk

- Removed the legacy in-app firewall opener and moved firewall setup to `helpers\\firewall-opener.ps1`.
- Raised the branch baseline to Windows 10/11 by setting `WINVER` / `_WIN32_WINNT` to `0x0A00`.
- Removed the dead `Run as unprivileged user` feature, its preferences, UI, and source files.
- Simplified the default directory-selection logic to modern `KnownFolder`-based user/public/executable modes only.
- Removed legacy UI and UPnP branches that only existed for pre-Windows-10 systems.
- Removed the remaining live Windows-version plumbing:
  - deleted `_WINVER_*` compatibility constants
  - deleted `DetectWinVersion()` and `CPreferences::GetWindowsVersion()`
  - removed cached `m_wWinVer`
  - flattened the remaining Win7/Vista checks in `EmuleDlg`, `PPgGeneral`, `Preferences`, and `FileInfoDialog`
  - simplified file-association registry writes to the per-user `HKCU\\Software\\Classes` hive
- Verified each cleanup block with `..\\23-build-emule-debug-incremental.cmd`.

## Current State

- The branch now assumes Windows 10/11 only in live code paths.
- There are no remaining live uses of `_WINVER_*`, `DetectWinVersion()`, or `CPreferences::GetWindowsVersion()`.
- The old firewall-opener path is gone from the app; firewall setup lives only in `helpers\\firewall-opener.ps1`.
- The RunAsUser subsystem is gone.
- The pre-Vista default-directory fallback maze is gone.
- The fixed-limit modernization changes remain in place and built cleanly.

## Next Chunk

- Sweep comment-only and resource-only old-OS wording that still talks about XP/Vista/Win7 where the live behavior is already modern-only.
- Continue the fixed-limit modernization work from `docs\\MODERN_LIMITS.md`.
