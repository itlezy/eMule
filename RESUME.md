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
- Runtime-verified the real debug app against one MP3 repro directory:
  - artifact bundle: `..\logs\20260329-154950-debug-launch`
  - shared file: `C:\tmp\videodupez-repro-mp3\Tina Moore - Never Gonna Let You Go.mp3`
  - actual verbose log shows MediaInfo-backed MP3 extraction with title, artist, album, length, and codec
- Expanded the extension-driven media classifier in [`srchybrid/OtherFunctions.cpp`](C:/prj/p2p/eMulebb/eMule/srchybrid/OtherFunctions.cpp) so more files actually reach the MediaInfo metadata path.
- Added new audio classifications for:
  - `.caf`, `.dff`, `.dsf`, `.m4b`, `.m4r`, `.oga`, `.opus`, `.spx`, `.tta`, `.weba`, `.wv`
- Added new video classifications for:
  - `.mts`, `.mxf`, `.ogv`, `.webm`
- Added missing classification-only extensions for UI/search grouping:
  - images: `.avif`, `.arw`, `.cr2`, `.dng`, `.heic`, `.heif`, `.jxl`, `.nef`, `.raw`, `.svg`, `.webp`
  - archives: `.001`, `.apk`, `.jar`, `.lz4`, `.lzma`, `.r00`, `.war`, `.xz`, `.zst`
  - documents: `.csv`, `.docx`, `.epub`, `.json`, `.odp`, `.ods`, `.odt`, `.pptx`, `.xlsx`
- Fixed the obvious misclassification of `.m4b` from video to audio.
- Rebuilt successfully with `..\23-build-emule-debug-incremental.cmd`.
- Ran a static mapping check after the build confirming the new entries resolve to the intended ED2K file types.

## Current State

- `id3lib` is no longer used by the app code or linked by the app project.
- `MediaInfo.dll` version `23.00+` is the required runtime dependency for audio/video metadata extraction, including MP3.
- Shared metadata extraction for MP3, MP4, MKV, and other supported audio/video formats is unified on the MediaInfo DLL path.
- Media metadata eligibility is still determined by the extension-based ED2K file type table, but that table now covers a broader set of common modern audio/video formats.
- Archives like `.rar` remain classified as archives and do not go through the audio/video metadata path.
- Image/document/archive classification was broadened for grouping and search labeling only; no new metadata extraction path was added for those categories.

## Next Chunk

- Runtime-verify one or more newly added extensions such as `.webm`, `.opus`, `.oga`, `.m4b`, `.mts`, or `.mxf` when suitable local sample files are available.
- Decide whether to simplify the metadata settings UI now that there is no second backend behind the old option naming.
- Tighten the missing-`MediaInfo.dll` user-facing diagnostics if the current log-only plus dialog-path hint is not sufficient.
