# Refactor Plan: `GetMimeType` (`srchybrid/MediaInfo.cpp:504`)

## Current State

`GetMimeType` sniffs file content to determine MIME type. Two-stage approach:

1. **`FindMimeFromData`** (Windows `urlmon.dll`) -- content-only sniffing, ~26 hardcoded
   types. Rejects `application/octet-stream` as useless. Does not pass the filename so
   the result is purely header-based.
2. **Manual magic-byte checks** for RAR, BZ2, ACE, LHA/LZH (4 formats).

### Callers

| Location | Purpose |
|---|---|
| `FileInfoDialog.cpp:540` | Populates `SMediaInfo::strMimeType` for the file-info dialog |
| `PPgSecurity.cpp:232` | Identifies downloaded IP-filter files (zip/gz/rar) before extraction |

### Problems

- `FindMimeFromData` coverage is tiny and undocumented in behavior across Windows versions.
- Only 4 manual signatures -- misses extremely common P2P formats.
- No detection for 7z, xz, ISO, MKV/WebM, FLAC, OGG, MP4, FLV, WMV/WMA/ASF, TORRENT, etc.
- RAR5 signature (`Rar!\x1a\x07\x01\x00`) not checked -- only RAR4 and ancient RAR.
- Buffer is 8 KB but most signatures need < 16 bytes; no benefit to reading more.
- `PPgSecurity.cpp:40` re-declares the function instead of including `MediaInfo.h`.

---

## Plan

### Phase 1 -- Expand magic-byte table (low risk, high value)

Replace the chain of `if`/`memcmp` blocks with a static lookup table:

```cpp
struct MagicEntry {
    int         offset;     // byte offset into buffer
    const BYTE *signature;  // magic bytes
    int         sigLen;     // length of signature
    LPCTSTR     mimeType;   // result
};
```

Populate it with signatures for at minimum:

| Format | Offset | Magic | MIME |
|--------|--------|-------|------|
| RAR 4 | 0 | `52 61 72 21 1A 07 00` | `application/x-rar-compressed` |
| RAR 5 | 0 | `52 61 72 21 1A 07 01 00` | `application/x-rar-compressed` |
| RAR (ancient) | 0 | `52 45 7E 5E` | `application/x-rar-compressed` |
| 7z | 0 | `37 7A BC AF 27 1C` | `application/x-7z-compressed` |
| BZ2 | 0 | `42 5A 68` | `application/x-bzip2` |
| XZ | 0 | `FD 37 7A 58 5A 00` | `application/x-xz` |
| GZ | 0 | `1F 8B` | `application/gzip` |
| ZIP | 0 | `50 4B 03 04` | `application/zip` |
| ACE | 7 | `2A 2A 41 43 45 2A 2A` | `application/x-ace-compressed` |
| LHA/LZH | 2 | `2D 6C 68 35 2D` | `application/x-lha-compressed` |
| ISO 9660 | 0x8001 | `43 44 30 30 31` | `application/x-iso9660-image` |
| MKV/WebM | 0 | `1A 45 DF A3` | `video/x-matroska` |
| OGG | 0 | `4F 67 67 53` | `audio/ogg` |
| FLAC | 0 | `66 4C 61 43` | `audio/flac` |
| MP4/M4A | 4 | `66 74 79 70` | `video/mp4` |
| FLV | 0 | `46 4C 56` | `video/x-flv` |
| ASF (WMV/WMA) | 0 | `30 26 B2 75 8E 66 CF 11` | `video/x-ms-asf` |
| TORRENT | 0 | `64 38 3A 61 6E 6E 6F 75 6E 63 65` | `application/x-bittorrent` |

The table-driven approach:
- Walk the table after `FindMimeFromData` falls through.
- For each entry: `if (iRead >= entry.offset + entry.sigLen && memcmp(buffer + entry.offset, entry.signature, entry.sigLen) == 0)`.
- Return first match.

**ISO note:** ISO 9660 signature is at offset 0x8001 (32 769). Either increase buffer
to 33 KB (still trivial) or do a second `_lseek`+`_read` only when no earlier signature
matched. Probably worth it since ISOs are common in P2P.

### Phase 2 -- Fix BZ2 signature bug

Current code checks for `"BZh19"` (5 bytes). Real BZ2 header is `"BZh"` followed by
a block-size digit `1`-`9`. The current check only matches block size 1 files (which
are rare). Fix: match `"BZh"` (3 bytes) then verify `buffer[3] >= '1' && buffer[3] <= '9'`.

### Phase 3 -- Reduce buffer, skip `FindMimeFromData` for known types

- Shrink the buffer from 8 KB to 512 bytes for the initial read (all non-ISO signatures
  fit in the first 16 bytes). If ISO check is needed, do a targeted seek.
- Run the magic-byte table **before** calling `FindMimeFromData`. Our table is faster
  (no COM call) and more reliable. Fall back to `FindMimeFromData` only for types we
  don't cover (HTML, RTF, PDF, etc.).

### Phase 4 -- Clean up the forward declaration

`PPgSecurity.cpp:40` has a bare `bool GetMimeType(...)` declaration instead of
including `MediaInfo.h`. Replace with `#include "MediaInfo.h"` (check for include
guard / `#pragma once` first).

### Phase 5 -- Optional: WebM vs MKV disambiguation

MKV and WebM share the EBML header `1A 45 DF A3`. To distinguish them, parse the EBML
DocType element deeper in the header (`webm` vs `matroska`). Low priority -- returning
`video/x-matroska` for both is fine for the file-info dialog use case.

---

## Risks & Notes

- **No behavioral change to callers.** Both callers just store or compare the string;
  adding more types won't break anything.
- **No new dependencies.** Pure byte-level checks, no libraries needed.
- **Test approach:** Drop sample files (or craft minimal headers) into a temp dir and
  call `GetMimeType` in a unit-test loop comparing expected vs actual MIME strings.
- Keep `FindMimeFromData` as a fallback -- it handles some obscure Microsoft-specific
  types and image sub-types (`image/pjpeg`, etc.) that we don't need to reimplement.
