# Retired Thumbnail Preview Capability

This document is retained as feature history only. The internal thumbnail-preview capability was removed from this branch instead of being migrated to Media Foundation or FFmpeg.

## Table of Contents

- [Feature Tracking](#feature-tracking)
- [Background](#background)
- [Current architecture](#current-architecture-for-reference)
- [Option 2 — Windows Media Foundation](#option-2----windows-media-foundation-imfsourcereader-feat_022)
- [Option 3 — FFmpeg](#option-3----ffmpeg-libavcodec--libavformat--libswscale-feat_023)
- [Comparison](#comparison)
- [MediaInfo Integration](#mediainfo-integration-from-emuleai-analysis-feat_024)

## Feature Tracking

| ID | Feature | Status |
|----|---------|--------|
| FEAT_022 | Windows Media Foundation migration (recommended) | **[REJECTED]** thumbnail preview capability retired |
| FEAT_023 | FFmpeg alternative (long-term) | **[REJECTED]** thumbnail preview capability retired |
| FEAT_024 | MediaInfo static embedding | **[REJECTED]** |

## Background

The old capability used `FrameGrabThread.cpp` plus DirectShow `IMediaDet` through the bundled `qedit.h` header to generate video thumbnails for remote preview exchange. That feature is no longer part of the product.

`qedit.h` remains in the tree only because `MediaInfo.h` still includes it for separate media-info probing. Removing that dependency is a different task from the retired thumbnail-preview feature.

---

## Current architecture (for reference)

```
CKnownFile::GrabImage()
  +-- AfxBeginThread(CFrameGrabThread)
       +-- CFrameGrabThread::Run()
            +-- GrabFrames()          <- IMediaDet lives here
                 |-- CoCreateInstance(MediaDet)
                 |-- seek N times (50 s apart)
                 |-- optional resize + CQuantizer colour reduction
                 +-- PostMessage(TM_FRAMEGRABFINISHED)
                      +-- CEmuleDlg::OnFrameGrabFinished()
                           +-- CKnownFile::GrabbingFinished()
                                +-- SendPreviewAnswer() to peer
```

Files touched by the current design:

| File | Role |
|------|------|
| `FrameGrabThread.h/.cpp` | Thread + `GrabFrames()` implementation |
| `KnownFile.cpp` | Launches thread, owns `GrabbingFinished()` |
| `PartFile.cpp` | Subclass override, adds `m_bPreviewing` mutex |
| `EmuleDlg.h/.cpp` | `TM_FRAMEGRABFINISHED` message + handler |
| `SearchFile.h` | Stores grabbed `HBITMAP` array |
| `PreviewDlg.cpp` | Displays frames in UI |
| `qedit.h` (bundled) | Legacy header kept alive manually |

The public interface that callers see is narrow:
```cpp
// Only entry point
bool CKnownFile::GrabImage(uint8 nFramesToGrab, double dStartTime,
                           bool bReduceColor, uint16 nMaxWidth, void *pSender);

// Only callback
void CKnownFile::GrabbingFinished(HBITMAP *imgResults, uint8 nFramesGrabbed,
                                  void *pSender);
```
Both options replace only what is inside `FrameGrabThread.cpp`; the rest of the
architecture stays unchanged.

---

## Option 2 -- Windows Media Foundation (`IMFSourceReader`) (FEAT_022)

### What changes

Replace `GrabFrames()` with an MF-based implementation.  Everything outside
`FrameGrabThread.cpp` (and the `#include <qedit.h>` / `#include <dshow.h>` block) stays
the same.

`IMediaDet` is a thin wrapper around DirectShow that calls `GetBitmapBits()`.
`IMFSourceReader` is the MF equivalent: open a source, select the video stream, call
`ReadSample()` at a seeked position, get an `IMFSample` containing raw decoded pixels,
blit to a DIB section.

### Key MF types used

| MF type | Purpose |
|---------|---------|
| `IMFSourceReader` | Open file, seek, decode one frame |
| `MFCreateSourceReaderFromURL` | Factory, no COM `CoCreateInstance` boilerplate |
| `IMFMediaType` | Configure output to RGB32 |
| `IMFSample` / `IMFMediaBuffer` | Hold decoded pixel data |
| `MFVideoInterlaceMode` | Handle interlaced content |

MF is available from Windows 7 onwards via `mfreadwrite.lib` + `mfplat.lib` + `mf.lib`.
No new external dependency.

### Implementation sketch

```cpp
// Replace the IMediaDet block in GrabFrames():

MFStartup(MF_VERSION);

IMFAttributes *pAttr = nullptr;
MFCreateAttributes(&pAttr, 1);
pAttr->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);

IMFSourceReader *pReader = nullptr;
MFCreateSourceReaderFromURL(strFileName, pAttr, &pReader);

// Force RGB32 output
IMFMediaType *pType = nullptr;
MFCreateMediaType(&pType);
pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
pType->SetGUID(MF_MT_SUBTYPE,    MFVideoFormat_RGB32);
pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, pType);

// Read native dimensions from the configured type
IMFMediaType *pNative = nullptr;
pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pNative);
UINT32 width, height;
MFGetAttributeSize(pNative, MF_MT_FRAME_SIZE, &width, &height);

for (uint32 i = 0; i < nFramesToGrab; ++i) {
    PROPVARIANT pos;
    InitPropVariantFromInt64((LONGLONG)(dStartTime * 1e7), &pos); // 100-ns units
    pReader->SetCurrentPosition(GUID_NULL, pos);
    PropVariantClear(&pos);

    IMFSample *pSample = nullptr;
    DWORD flags = 0;
    pReader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM,
                        0, nullptr, &flags, nullptr, &pSample);
    if (!pSample) break;

    IMFMediaBuffer *pBuf = nullptr;
    pSample->ConvertToContiguousBuffer(&pBuf);
    BYTE *pPixels = nullptr; DWORD cbLen = 0;
    pBuf->Lock(&pPixels, nullptr, &cbLen);

    // --- existing resize / quantise logic goes here (unchanged) ---
    // pPixels is RGB32, top-down; adapt stride calculation accordingly

    pBuf->Unlock();
    // ... release COM pointers, store HBITMAP ...

    dStartTime += TIMEBETWEENFRAMES;
}

MFShutdown();
```

Resize and `CQuantizer` colour reduction logic can be kept as-is; only the pixel-source
changes from `IMediaDet::GetBitmapBits` to an `IMFMediaBuffer`.

### Impact assessment

| Area | Impact | Notes |
|------|--------|-------|
| `FrameGrabThread.cpp` | Full rewrite of `GrabFrames()` | ~150 lines in, ~150 lines out |
| `FrameGrabThread.h` | No change | |
| `KnownFile.cpp/.h` | No change | |
| `PartFile.cpp` | No change | |
| `EmuleDlg.cpp/.h` | No change | |
| `qedit.h` (bundled) | Can be removed if not used elsewhere | `MediaInfo.h` also includes it -- check separately |
| `emule.vcxproj` | Add `mfreadwrite.lib mfplat.lib mf.lib mfuuid.lib` | Remove dshow/quartz entries if any |
| Test surface | `GrabFrames()` only | Same `TM_FRAMEGRABFINISHED` path exercised |
| New dependency | None (Windows 7+) | |
| Risk | Low | MF is stable, well-documented, actively supported |

### Risks

- **Pixel layout**: MF with `MFVideoFormat_RGB32` outputs top-down rows; `IMediaDet`
  outputs bottom-up (standard DIB). The resize loop uses `i * height / bmih.biHeight`
  for row indexing -- verify sign convention and flip if needed.
- **Partial files**: `CPartFile` acquires `m_FileCompleteMutex` before calling
  `GrabImage`. MF opens the file exclusively during decoding. Ensure the mutex window
  covers the full MF session so a download thread does not truncate the file mid-read.
- **Format support**: MF relies on installed Windows codec packs. Obscure containers
  (e.g. old Real Media, some MKV variants) may not decode on a vanilla Windows install.
  `IMediaDet` had the same limitation -- no regression, just worth documenting.

### Effort estimate (rough)

| Task | Size |
|------|------|
| Rewrite `GrabFrames()` | Small (~150 lines) |
| Pixel layout / stride verification | Small |
| `vcxproj` linker changes | Trivial |
| Smoke-test with mp4, avi, mkv | Small |
| Total | ~1-2 days |

---

## Option 3 -- FFmpeg (`libavcodec` / `libavformat` / `libswscale`) (FEAT_023)

### What changes

Replace `GrabFrames()` with an FFmpeg-based implementation and add FFmpeg as a new
submodule dependency (matching the pattern of `eMule-id3lib`, `eMule-mbedtls`, etc.).

### Key FFmpeg types used

| FFmpeg type | Purpose |
|-------------|---------|
| `AVFormatContext` | Open container, find streams |
| `AVCodecContext` | Decode video frames |
| `AVPacket` | Compressed data read from container |
| `AVFrame` | Decoded pixel data |
| `SwsContext` | Pixel format + resize conversion |

### Implementation sketch

```cpp
// Replace GrabFrames() body:

avformat_network_init(); // once at startup, or lazy-init

AVFormatContext *pFmt = nullptr;
if (avformat_open_input(&pFmt, strFileNameUtf8, nullptr, nullptr) != 0) return 0;
avformat_find_stream_info(pFmt, nullptr);

int vidIdx = av_find_best_stream(pFmt, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
if (vidIdx < 0) { avformat_close_input(&pFmt); return 0; }

AVStream *pStream = pFmt->streams[vidIdx];
const AVCodec *pCodec = avcodec_find_decoder(pStream->codecpar->codec_id);
AVCodecContext *pCtx = avcodec_alloc_context3(pCodec);
avcodec_parameters_to_context(pCtx, pStream->codecpar);
avcodec_open2(pCtx, pCodec, nullptr);

int destW = (nMaxWidth > 0 && (uint32)pCtx->width > nMaxWidth) ? nMaxWidth : pCtx->width;
int destH = (nMaxWidth > 0 && (uint32)pCtx->width > nMaxWidth)
            ? pCtx->height * destW / pCtx->width : pCtx->height;
AVPixelFormat destFmt = bReduceColor ? AV_PIX_FMT_PAL8 : AV_PIX_FMT_BGR24;

SwsContext *pSws = sws_getContext(pCtx->width, pCtx->height, pCtx->pix_fmt,
                                  destW, destH, destFmt,
                                  SWS_BILINEAR, nullptr, nullptr, nullptr);

AVFrame *pFrame = av_frame_alloc();
AVPacket *pPkt  = av_packet_alloc();

for (uint32 i = 0; i < nFramesToGrab; ++i) {
    int64_t ts = (int64_t)(dStartTime * AV_TIME_BASE);
    av_seek_frame(pFmt, -1, ts, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(pCtx);

    // Decode until we get one video frame at or after ts
    while (av_read_frame(pFmt, pPkt) >= 0) {
        if (pPkt->stream_index != vidIdx) { av_packet_unref(pPkt); continue; }
        avcodec_send_packet(pCtx, pPkt);
        av_packet_unref(pPkt);
        if (avcodec_receive_frame(pCtx, pFrame) == 0) break;
    }

    // sws_scale into a DIB section, store HBITMAP
    // CQuantizer not needed -- sws can produce PAL8 directly

    av_frame_unref(pFrame);
    dStartTime += TIMEBETWEENFRAMES;
}

sws_freeContext(pSws);
av_frame_free(&pFrame);
av_packet_free(&pPkt);
avcodec_free_context(&pCtx);
avformat_close_input(&pFmt);
```

`CQuantizer` becomes redundant; `sws_scale` handles both resize and pixel-format
conversion to `AV_PIX_FMT_PAL8` in one pass (with better dithering than the current
octree approach).

### Dependency strategy

Follow the existing submodule pattern:

```
eMule-ffmpeg/          <- new submodule tracking a stable FFmpeg release branch
  include/
  lib/
    libavcodec.lib
    libavformat.lib
    libavutil.lib
    libswscale.lib
  bin/
    avcodec-*.dll
    avformat-*.dll
    avutil-*.dll
    swscale-*.dll
```

FFmpeg can be built as static libs (LGPL-compatible, no DLL distribution needed) or
as shared libs. **LGPL compliance requires that eMule is dynamically linked to FFmpeg
or that build instructions allow relinking** -- check the project's licence obligations.

### Impact assessment

| Area | Impact | Notes |
|------|--------|-------|
| `FrameGrabThread.cpp` | Full rewrite of `GrabFrames()` | ~150 lines in, ~150 lines out |
| `FrameGrabThread.h` | No change | |
| `KnownFile.cpp/.h` | No change | |
| `PartFile.cpp` | No change | |
| `EmuleDlg.cpp/.h` | No change | |
| `Quantize.h/.cpp` | Can be removed (if not used elsewhere) | Check `CaptchaGenerator.cpp` |
| `qedit.h` (bundled) | Can be removed (if `MediaInfo.h` is also migrated) | |
| `emule.vcxproj` | Add 4 FFmpeg libs, include path, DLL copy step | Moderate change |
| CMake / build scripts | Add `eMule-ffmpeg` submodule, update `check-dep-updates` | |
| Deployment | FFmpeg DLLs (or static link) must ship with eMule | New obligation |
| Test surface | `GrabFrames()` only | Same downstream path |
| New dependency | **Yes -- FFmpeg (~30 MB DLLs or ~15 MB static)** | |
| Risk | Medium | Format coverage excellent; dependency management is the main cost |

### Risks

- **Licence compliance**: FFmpeg is LGPL 2.1+. Dynamic linking is the safe path. Static
  linking is permissible only if eMule (GPL v2) users can relink against a modified
  FFmpeg -- document and verify before choosing static.
- **Binary size**: FFmpeg adds ~15-30 MB depending on codecs compiled in. A minimal
  build (mp4/mkv/avi only, no encoders) can reduce this significantly.
- **`CString` vs UTF-8 paths**: FFmpeg's `avformat_open_input` takes a `char*` path.
  Windows paths containing non-ASCII characters must be converted from `CString` (UTF-16)
  to UTF-8 before passing. Use `WideCharToMultiByte(CP_UTF8, ...)`.
- **Partial files**: Same concern as Option 2 -- FFmpeg will hold the file open for the
  duration of `GrabFrames()`; ensure `m_FileCompleteMutex` is held throughout.
- **Build complexity**: A new submodule means maintaining a pre-built FFmpeg drop or
  adding an FFmpeg build step to the CI pipeline.

### Effort estimate (rough)

| Task | Size |
|------|------|
| Source/build FFmpeg minimal static/shared | Medium |
| Add submodule + `vcxproj` wiring | Small-Medium |
| Rewrite `GrabFrames()` | Small (~150 lines) |
| UTF-8 path conversion | Trivial |
| Remove `CQuantizer` (optional cleanup) | Trivial |
| LGPL compliance review | Small |
| Smoke-test with mp4, avi, mkv, wmv | Small |
| Total | ~3-5 days |

---

## Comparison

| Criterion | Option 2 (MF) | Option 3 (FFmpeg) |
|-----------|---------------|-------------------|
| New external dependency | None | Yes (~15-30 MB) |
| Format coverage | Windows codecs only | Near-universal |
| Effort | ~1-2 days | ~3-5 days |
| Maintenance burden | Low (OS-provided) | Medium (track releases) |
| Licence consideration | None | LGPL compliance needed |
| Removes bundled `qedit.h` | Yes (pending `MediaInfo.h` audit) | Yes |
| Removes `CQuantizer` | No | Yes (optional) |
| Risk | Low | Medium |
| Minimum Windows version | Windows 7 | XP+ (if built for it) |

**Recommendation**: Option 2 (MF) is the lower-risk, lower-effort path that solves the
immediate problem (deprecated API + bundled header). Option 3 (FFmpeg) is the better
long-term choice if broad codec support matters -- users on a minimal Windows install
without codec packs will fail silently with Option 2. If format coverage complaints are
already known, go straight to Option 3.

---

## MediaInfo Integration (from eMuleAI analysis) (FEAT_024)

eMuleAI embeds MediaInfo and ZenLib as static libraries with no runtime DLL dependency. This approach is relevant to eMulebb for both metadata extraction and as a potential replacement for the aging id3lib dependency.

### eMuleAI approach

- MediaInfo + ZenLib are compiled as static libraries and linked directly into the eMuleAI binary
- No runtime DLL distribution needed
- eMuleAI provides a thin wrapper (`CMediaInfoLIB`) around MediaInfoLib

### Recommended approach for eMulebb

- Vendor pinned versions of MediaInfo and ZenLib source
- Build as static libraries following the existing submodule pattern (like `eMule-id3lib`, `eMule-mbedtls`)
- Link directly into the eMule binary
- Do NOT reuse eMuleAI's wrapper layer (`CMediaInfoLIB`) -- call `MediaInfoLib` API directly to avoid carrying unnecessary abstraction
- Plan to eventually replace `id3lib` with MediaInfo for MP3 metadata extraction, consolidating to a single metadata backend

### Shutdown cleanup

eMuleAI patches MediaInfo for explicit teardown at application shutdown. This may be needed in eMulebb as well to avoid leak reports in debug builds. Investigate whether upstream MediaInfo has addressed this or whether a local patch is still required.

### Integration path

1. Vendor MediaInfo + ZenLib as a new submodule
2. Build static libraries and integrate into `emule.vcxproj`
3. Replace `FileInfoDialog.cpp` metadata probing (which currently uses a separate `IMediaDet` path)
4. Evaluate replacing id3lib MP3 tag reading with MediaInfo (single backend for all media metadata)

---

## Shared implementation notes (both options)

1. `FrameGrabResult_Struct`, `TM_FRAMEGRABFINISHED`, and `CKnownFile::GrabbingFinished()`
   are unchanged -- no callers outside `FrameGrabThread.cpp` need modification.
2. The `TEST_FRAMEGRABBER` debug path in `SharedFilesCtrl.cpp` and the `CImage::Save`
   block in `FrameGrabThread.cpp` carry over unchanged.
3. `FileInfoDialog.cpp` has a separate `IMediaDet` usage for metadata probing -- that is
   **not** covered by this work and should be tracked separately.
4. `MediaInfo.h` includes `qedit.h`; `qedit.h` cannot be deleted until that dependency
   is also resolved.
