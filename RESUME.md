# Resume

## Last Chunk

- Removed `HAVE_WIN7_SDK_H` wrappers and compiled the taskbar-goodies path unconditionally.
- Collapsed `emule_site_config.h` to a modern Windows 10/11 toolchain contract.
- Removed `XP_BUILD` from the project and deleted the fallback code that depended on it.
- Removed `VistaDefines.h` and switched fully to the Windows 10 SDK declarations.
- Removed dead `_WIN32_WINNT` / `WINVER` fallback declarations from UI helper code.
- Kept the real MFC/Win32 platform layer intact; only compatibility scaffolding was removed.
- Verified every cleanup block with `..\\23-build-emule-debug-incremental.cmd`.

## Current State

- The branch now assumes Windows 10/11 only in live code paths.
- There are no remaining live uses of `_WINVER_*`, `DetectWinVersion()`, or `CPreferences::GetWindowsVersion()`.
- There are no remaining live uses of `HAVE_WIN7_SDK_H`, `HAVE_VISTA_SDK`, `VistaDefines.h`, or `XP_BUILD`.
- The old firewall-opener path is gone from the app; firewall setup lives only in `helpers\\firewall-opener.ps1`.
- The RunAsUser subsystem is gone.
- The pre-Vista default-directory fallback maze is gone.
- The fixed-limit modernization changes remain in place and built cleanly.

## Next Chunk

- MediaInfo cleanup TODO:
  - review `HAVE_WMSDK_H`, `HAVE_QEDIT_H`, and `HAVE_SAPI_H` to ensure they only represent real optional dependency availability
  - simplify remaining MediaInfo-era SDK compatibility branches without changing media feature behavior
  - clean any stale comments that still describe removed Vista/XP SDK assumptions
- Sweep comment-only and resource-only old-OS wording that still talks about XP/Vista/Win7 where the live behavior is already modern-only.
- Continue the fixed-limit modernization work from `docs\\MODERN_LIMITS.md`.
