# Resume

## Last Chunk

- Removed the legacy RIFF / RM / WM shared-metadata extraction path and unified non-MP3 audio/video metadata extraction behind the runtime `MediaInfo.dll` helper in [`srchybrid/MediaInfo_DLL.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/MediaInfo_DLL.cpp).
- Kept `id3lib` only for the existing MPEG audio path in [`srchybrid/KnownFile.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/KnownFile.cpp).
- Updated [`srchybrid/FileInfoDialog.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/FileInfoDialog.cpp) and [`srchybrid/KnownFile.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/KnownFile.cpp) to rely on the same MediaInfo DLL-backed metadata path for non-MP3 files.
- Removed the old backend implementation files from the project:
  - `srchybrid/MediaInfo_RIFF.cpp`
  - `srchybrid/MediaInfo_RealMedia.cpp`
  - `srchybrid/MediaInfo_WindowsMedia.cpp`
- Reproduced the real debug-build startup assertions reported during MediaInfo testing:
  - `AsyncSocketEx.cpp:620`
  - MFC `sockcore.cpp:644`
- Traced those assertions to a first-start startup bug, not to MediaInfo:
  - a fresh config sets `IsFirstStart()`
  - the first-time wizard can call `Rebind()` and pre-open the TCP / UDP sockets
  - the normal startup phase then tried to create those same sockets again
- Fixed that by making same-port socket creation idempotent in:
  - [`srchybrid/ListenSocket.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/ListenSocket.cpp)
  - [`srchybrid/ClientUDPSocket.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/ClientUDPSocket.cpp)
- Rebuilt successfully with `..\23-build-emule-debug-incremental.cmd`.

## Runtime Verification

- Real debug app, first-start-style repro, shared dir `C:\tmp\videodupez`, `/assertfile` enabled:
  - artifact bundle: `..\logs\20260329-150852-debug-launch`
  - no `AsyncSocketEx` or `sockcore` assertions
  - no `Unable to create socket on port ...` errors
  - MediaInfo DLL loaded successfully
  - WMV metadata extraction logged successfully
- Real debug app, focused MP4 repro, shared dir `C:\tmp\videodupez-repro-mp4`, `/assertfile` enabled:
  - artifact bundle: `..\logs\20260329-150945-debug-launch`
  - `MediaInfo.dll` loaded from `C:\Program Files\MediaInfo\MediaInfo.dll`
  - shared metadata extraction logged:
    - `length=1753`
    - `codec="AVC"`
- This confirms the actual running debug build now extracts MP4 shared metadata through MediaInfo and no longer hits the earlier first-start socket assertions during the repro.

## Current State

- `MediaInfo.dll` version `23.00+` is a required runtime dependency for non-MP3 audio/video metadata extraction.
- The non-MP3 shared metadata path is now MediaInfo-only.
- The MP3 path still uses `id3lib`.
- The debug-launch helper remains the preferred way to reproduce runtime issues and collect logs / dumps / config snapshots.
- Actual runtime logs now show the selected MediaInfo DLL path and version.

## Next Chunk

- Decide whether to replace the remaining MP3 / `id3lib` path with MediaInfo as well, or intentionally keep `id3lib` for MPEG audio for now.
- Runtime-verify the file-details dialog path on representative MP4 / MKV samples, not just the shared-tag extraction path.
- If needed, tighten user-facing diagnostics for a missing or too-old `MediaInfo.dll` so the failure mode is obvious outside verbose logs.
