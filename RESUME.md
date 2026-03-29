# Resume

## Last Chunk

- Added a debug launch helper workflow under `helpers`:
  - `debug-launch.ps1` orchestrates build, process cleanup, optional LocalAppData config reset/patching, timed launch, and artifact bundling under `..\logs\<timestamp>-debug-launch`
  - `debug_launch_helper.py` writes the debug-focused `preferences.ini` preset used by the launcher
- Implemented three preference modes for the helper:
  - `clean` resets `%LOCALAPPDATA%\eMule\config` and writes a debug preset
  - `patch` preserves the config directory but patches `preferences.ini` with the same debug preset
  - `none` performs a non-destructive launch path for smoke-testing
- The helper archives config/log snapshots before and after launch, records a JSON session manifest, and collects the CRT debug log when present.
- Verified the helper end to end with a non-destructive `PrefsMode=none` timed launch that produced a real artifact bundle in `..\logs`.
- Verified the Python INI helper on a temporary `preferences.ini` to ensure the debug preset is written correctly.
- Changed the MediaInfo DLL contract from "pick the newest compatible installed DLL" to "require MediaInfo.dll version 23.00 or newer".
- Kept explicit absolute-path overrides and deterministic candidate probing, but now stop at the first acceptable candidate instead of preferring the newest installed version.
- Removed the old pre-18.06 MediaInfo field-name compatibility branch from `FileInfoDialog.cpp` and now use only the modern `Format*`/`Language_More` keys.
- Updated the MediaInfo missing-library hint so it clearly tells users that `MediaInfo.dll` 23.00 or newer is required and points them to install it.
- Added a regular log entry for the selected MediaInfo DLL so the loaded path and version are visible without enabling verbose candidate logging.
- Split the old `srchybrid\MediaInfo.cpp` monolith into shared helpers plus `MediaInfo_RIFF.cpp`, `MediaInfo_RealMedia.cpp`, and `MediaInfo_WindowsMedia.cpp`.
- Removed the dead `HAVE_QEDIT_H` compatibility path and kept `qedit.h` as the bundled required header.
- Kept `HAVE_WMSDK_H` and `HAVE_SAPI_H` as real optional-dependency checks driven by `emule_site_config.h`.
- Made Windows Media parsing callable unconditionally from callers and added a fail-closed stub when `wmsdk.h` is unavailable.
- Removed caller-side `HAVE_WMSDK_H` branches from `FileInfoDialog.cpp` and `KnownFile.cpp`.
- Reworked MediaInfo DLL probing to scan deterministic absolute candidates and select the highest compatible installed version instead of taking the first `LoadLibrary("MEDIAINFO.DLL")` hit.
- Added modern video codec labels for `AVC1`, `WVC1`, `HEVC`/`H265`/`HEV1`/`HVC1`, `VP8`, `VP9`, and `AV1`.
- Tightened aspect-ratio labeling so named ratios are only emitted when the numeric ratio is close to the canonical value.
- Fixed the Windows Media fallback codec selection path so it no longer always reuses the first codec entry for every later stream of the same type.
- Cleaned stale MediaInfo comments that still described XP/Vista/WMP-era assumptions as live behavior.
- Verified the whole chunk with `..\\23-build-emule-debug-incremental.cmd`.

## Current State

- A reproducible debug-launch path now exists under `helpers` and can collect launch-session artifacts without requiring a live debugger.
- Media parsing is now split by backend instead of living in one 2600-line translation unit.
- The MediaInfo DLL loader now requires `MediaInfo.dll` 23.00 or newer and selects the first acceptable deterministic candidate, with explicit absolute configuration still taking precedence when valid.
- `GetWMHeaders` is now a stable helper interface; callers do not need compile-time `HAVE_WMSDK_H` branches.
- `TextToSpeech.cpp` no longer emits the old compile-time missing-SAPI warning, but the internal `HAVE_SAPI_H` implementation guards still exist.
- The tree builds cleanly on the current Windows 10/11 toolchain baseline.

## Next Chunk

- Use the new debug-launch helper on a failing startup/repro sequence and inspect the generated manifest/log bundle to determine the next concrete bugfix target.
- If needed after first real use, add a second-stage debugger-oriented mode to the helper without changing the default script-first workflow.
- Runtime-verify the MediaInfo DLL selection logic with real candidate layouts:
  - configured absolute DLL path
  - invalid configured DLL path with fallback to an installed compatible copy
  - multiple installed MediaInfo versions where the first deterministic acceptable candidate should win
  - `<noload>` opt-out
  - installed MediaInfo below 23.00 should be rejected with the new guidance text
- Runtime-verify media metadata on representative samples:
  - AVI/RIFF
  - ASF/WMV with multiple streams
  - files whose codec names should now show `AVC1`, `WVC1`, `HEVC`, `VP9`, or `AV1`
- Finish the remaining SAPI cleanup if desired by collapsing the internal `TextToSpeech.cpp` `#ifdef HAVE_SAPI_H` branches into a smaller implementation block.
- Continue the remaining comment-only/resource-only wording sweep for stale old-OS references once MediaInfo runtime behavior is confirmed.
