# Resume

## Last Chunk

- Added a reusable MediaInfo probe helper under `helpers\mediainfo_probe.py`.
- The probe mirrors the app's runtime contract closely enough to diagnose the current DLL path:
  - resolves the first app-compatible `MediaInfo.dll` candidate
  - enforces the same `23.00+` minimum-version rule
  - calls the same `MediaInfo_New` / `MediaInfo_Open` / `MediaInfo_Get` API family
  - queries the same `Format*`, duration, stream-count, video, audio, text, and menu fields used by `FileInfoDialog.cpp`
- Added optional `ffprobe` reference capture so MediaInfo output can be compared to an independent parser during diagnosis.
- Probed the sample set in `C:\tmp\videodupez` and wrote the JSON report to `..\logs\mediainfo-probe-videodupez.json`.
- Verified the installed runtime DLL used for probing is `C:\Program Files\MediaInfo\MediaInfo.dll` version `26.01.0.0`.
- Runtime result summary:
  - all files opened successfully through the MediaInfo DLL
  - all `.mkv` and `.mp4` files, which are the sample files that would actually hit the MediaInfo DLL path in eMule, returned non-empty format, duration, resolution, frame-rate, and audio core fields
  - the MediaInfo output for those `.mkv` / `.mp4` files matched `ffprobe` closely on container, codec family, dimensions, frame rate, duration, and audio layout
  - the `.avi` and `.wmv` samples also parsed successfully through the DLL, but current eMule code would route those through the native RIFF / Windows Media parsers before MediaInfo
- Observed detail worth keeping in mind:
  - the direct MediaInfo probe returned blank `FrameRate` values for the two WMV samples even though `ffprobe` reported `30000/1001`
  - that is not a blocker for the current app behavior because `.wmv` files are handled by the native Windows Media parser first
- Verified the helper itself with `python -m py_compile helpers\mediainfo_probe.py`.

## Current State

- A reproducible debug-launch path now exists under `helpers` and can collect launch-session artifacts without requiring a live debugger.
- A reusable MediaInfo DLL probe now exists and can validate the installed runtime DLL against the same field set that `FileInfoDialog.cpp` uses.
- Media parsing is now split by backend instead of living in one 2600-line translation unit.
- The MediaInfo DLL loader now requires `MediaInfo.dll` 23.00 or newer and selects the first acceptable deterministic candidate, with explicit absolute configuration still taking precedence when valid.
- `GetWMHeaders` is now a stable helper interface; callers do not need compile-time `HAVE_WMSDK_H` branches.
- `TextToSpeech.cpp` no longer emits the old compile-time missing-SAPI warning, but the internal `HAVE_SAPI_H` implementation guards still exist.
- The tree builds cleanly on the current Windows 10/11 toolchain baseline.

## Next Chunk

- Decide whether the runtime probe helper should remain a permanent diagnostic tool or whether its useful parts should be folded into the existing debug-launch workflow.
- Runtime-verify the actual in-app MediaInfo dialog path, not just the DLL contract, by driving a real file-details session on a known `.mkv` / `.mp4` sample and checking the log bundle.
- Runtime-verify the MediaInfo DLL selection logic with real candidate layouts:
  - configured absolute DLL path
  - invalid configured DLL path with fallback to an installed compatible copy
  - `<noload>` opt-out
  - installed MediaInfo below 23.00 should be rejected with the new guidance text
- Decide whether the WMV frame-rate gap seen in the direct MediaInfo probe matters enough to add a fallback when the file is forced through the DLL path.
- Finish the remaining SAPI cleanup if desired by collapsing the internal `TextToSpeech.cpp` `#ifdef HAVE_SAPI_H` branches into a smaller implementation block.
