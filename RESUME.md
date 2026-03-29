# Resume

## Last Chunk

- Removed the last `id3lib`-based metadata extraction code from:
  - [`srchybrid/KnownFile.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/KnownFile.cpp)
  - [`srchybrid/FileInfoDialog.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/FileInfoDialog.cpp)
- Non-image audio/video metadata now goes through the required runtime `MediaInfo.dll` path only.
- Kept the existing metadata toggle UI contract, but the enabled mode now means `MediaInfo.dll` only.
- Removed the `id3lib` build dependency from the app project files:
  - [`srchybrid/emule.vcxproj`](C:/prj/p2p/eMulebb/eMule/srchybrid/emule.vcxproj)
  - [`srchybrid/emule.sln`](C:/prj/p2p/eMulebb/eMule/srchybrid/emule.sln)
  - [`srchybrid/emule.slnx`](C:/prj/p2p/eMulebb/eMule/srchybrid/emule.slnx)
- Updated the MediaInfo probe classifier in [`helpers/mediainfo_probe.py`](C:/prj/p2p/eMulebb/eMule/helpers/mediainfo_probe.py) so MPEG audio is treated as MediaInfo-backed.
- Rebuilt successfully with `..\23-build-emule-debug-incremental.cmd`.
- Runtime-verified the real debug app against one MP3 repro directory:
  - artifact bundle: `..\logs\20260329-154950-debug-launch`
  - shared file: `C:\tmp\videodupez-repro-mp3\Tina Moore - Never Gonna Let You Go.mp3`
  - actual verbose log shows:
    - `MediaInfoDLL loaded: C:\Program Files\MediaInfo\MediaInfo.dll version=26.1.0.0`
    - `Shared meta extraction: "C:\tmp\videodupez-repro-mp3\Tina Moore - Never Gonna Let You Go.mp3" length=258 codec="MPEG Audio" title="Never Gonna Let You Go" artist="Tina Moore" album="'90s R&B Essentials"`

## Current State

- `id3lib` is no longer used by the app code or linked by the app project.
- `MediaInfo.dll` version `23.00+` is now the required runtime dependency for audio/video metadata extraction, including MP3.
- Shared metadata extraction for MP3, MP4, MKV, and other supported audio/video formats is unified on the MediaInfo DLL path.
- The parent workspace no longer needs the `eMule-id3lib` dependency for builds.
- The debug-launch helper remains the preferred way to reproduce metadata and startup issues with captured logs and dumps.

## Next Chunk

- Commit and verify the parent workspace cleanup which removes the `eMule-id3lib` submodule and its build-script references.
- Decide whether to simplify the metadata settings UI now that there is no second backend left behind the old option naming.
- Tighten the missing-`MediaInfo.dll` user-facing diagnostics if the current log-only plus dialog-path hint is not sufficient.
