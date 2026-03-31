# eMule Community Build — Code Review: v0.60d → v0.70b → v0.72a

**Repository:** `eMule\analysis\bbclean7\eMule`
**Branch analyzed:** `origin/v0.72a` (current: `v0.72a-community`)
**Base branch:** `origin/v0.60d`
**Review date:** 2026-03-24
**Reviewer:** automated diff analysis

---

## Table of Contents

- [Overview](#overview)
- [Part 1: v0.60d → v0.70b](#part-1-v060d--v070b)
  - [Commit 1 — ef58358 (IOCP, UNC paths, GUI)](#commit-1--ef58358-2023-08-18)
  - [Commit 2 — 1398352 (TLS 1.3, PeerCache removal, UNC redesign)](#commit-2--1398352-2024-08-16)
- [Part 2: v0.70b → v0.72a](#part-2-v070b--v072a)
  - [Commit 3 — 24d1de7 (ARM64, CxImage removal, MbedTLS 4.0)](#commit-3--24d1de7-2026-01-05)
  - [Commit 4 — 18911c5 (VS2022 x64 porting)](#commit-4--18911c5-2026-03-23)
- [Summary and Risk Matrix](#summary-and-risk-matrix)

---

## Overview

There are exactly **4 commits** bridging v0.60d to v0.72a, spanning from August 2023 to March 2026. Two commits correspond to the v0.70b release cycle, two to the v0.72a release cycle.

| Commit | Hash | Date | Author | Manifest version | Release milestone |
|--------|------|------|--------|-----------------|-------------------|
| 1 | `ef58358` | 2023-08-18 | irwir | 0.70.0.9 | v0.70 baseline |
| 2 | `1398352` | 2024-08-16 | irwir | 0.70.1.5 | **v0.70b** |
| 3 | `24d1de7` | 2026-01-05 | irwir | 0.72.0.1 | v0.72a baseline |
| 4 | `18911c5` | 2026-03-23 | Threepwood-7 | — | v0.72a community fix |

**Total files changed across all commits:** ~500+ unique source files
**Net line delta:** ~+16,000 insertions / −20,000 deletions (significant refactoring, not just additions)

---

## Part 1: v0.60d → v0.70b

### Commit 1 — `ef58358` (2023-08-18)
**"Using I/O completion ports for disk operations / Improve handling of UNC paths / Minor GUI enhancements"**

**Scope:** 562 files changed, +16,870 / −20,679 lines

#### 1.1 New I/O subsystem: `PartFileWriteThread`

Two new files were introduced:
- `srchybrid/PartFileWriteThread.cpp` (231 lines)
- `srchybrid/PartFileWriteThread.h`

This is the most significant architectural change in this commit. Downloads no longer write directly to disk from the network receive thread. Instead, a dedicated worker thread (`CPartFileWriteThread`) accepts write requests via a **Windows I/O Completion Port (IOCP)** and serializes disk writes from a lower-priority thread.

Key implementation details:
```cpp
m_hPort = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);
// ... worker loop:
GetQueuedCompletionStatus(m_hPort, &dwBytes, &completionKey, &pOverlapped, INFINITE);
```

The thread is started at `THREAD_PRIORITY_BELOW_NORMAL` and uses `PostQueuedCompletionStatus` for graceful shutdown. `PartFile.cpp` integrates this by calling `CPartFileWriteThread::RemFile(this)` during file removal and routing write operations through `theApp.m_pPartFileWriteThread`.

**Assessment:** Sound design. IOCP is the correct Windows primitive for high-throughput async I/O. The thread priority choice is appropriate to avoid starving network I/O. The completion port is created with concurrency limit 1, which means writes are fully serialized — correct for file integrity but worth noting it may become a bottleneck at very high download speeds.

**Regression risk:** Medium. This changes fundamental download file write behavior. Race conditions between the write thread and file deletion/check need careful review. `RemFile()` is called in the destructor path, suggesting correct teardown, but the ASSERT in the destructor (`ASSERT(!m_hPort && !m_bRun)`) indicates the lifecycle is strictly enforced.

#### 1.2 UNC Path Improvements

Multiple files were updated to handle UNC paths (`\\server\share\...`) correctly. The `DirectoryTreeCtrl` and preferences subsystems gained awareness of UNC shares as a separate category from local drives. The groundwork laid here is expanded further in commit 2.

The `GetLogicalDriveStrings()` enumeration loop was refactored to avoid redundant null-termination (the buffer is already guaranteed null-terminated by the API contract). Minor but correct cleanup.

#### 1.3 GUI Enhancements

- **Icon:** `Mule_Vista.ico` reduced from 51,918 to 27,086 bytes — likely optimized/recompressed, same visual result.
- **Manifest:** `emuleWin32.manifest` and `emulex64.manifest` bumped to version `0.70.0.9`. Windows 8.1 and 8 compatibility GUIDs retained alongside Windows 10.
- **Copyright:** `emule.rc2` updated from "2002-2021" to "2002-2023".
- **Comment fix:** "shortest-to-complete" strategy renamed to "nearest-to-complete" in a download priority comment — minor clarity improvement.

#### 1.4 General Code Modernization

This commit also performs a large-scale pass over nearly every `.cpp`/`.h` file. Common patterns observed:

- Pointer arithmetic `memchr(m_pBuf + dwPos, ...)` → `memchr(&m_pBuf[dwPos], ...)` (subscript style, less ambiguous with non-char types)
- Removal of redundant null checks before delete
- `INT_PTR` used consistently for loop counters on collection sizes
- `_countof()` preferred over `sizeof(arr)/sizeof(arr[0])`
- Trailing whitespace removal throughout

---

### Commit 2 — `1398352` (2024-08-16)
**"Added TLS 1.3 support / PeerCache removed / Redesigned Options->Directories and UNC shares handling / A few minor changes and fixed regressions"**

**Scope:** 476 files changed, +3,383 / −6,630 lines (net −3,247 — a cleaning commit)

#### 2.1 TLS 1.3 Support (`WebSocket.cpp`)

This is the highest-impact security change in the entire v0.60d→v0.72a range.

The web interface SSL stack was upgraded from TLS 1.2 to TLS 1.3 via MbedTLS. Key additions:

```cpp
// PSA crypto must be initialized before any TLS 1.3 key exchange
int ret = (int)psa_crypto_init();  // returns PSA_SUCCESS = 0

// Session ticket support (enables TLS 1.3 session resumption)
mbedtls_ssl_ticket_init(&ticket_ctx);
mbedtls_ssl_cookie_init(&cookie_ctx);
mbedtls_ssl_ticket_setup(&ticket_ctx, mbedtls_ctr_drbg_random, &ctr_drbg,
                          MBEDTLS_CIPHER_AES_256_GCM, 86400 /*seconds*/);

// Restrict to PSK-based key exchange (no PKI required for local web UI)
mbedtls_ssl_conf_tls13_key_exchange_modes(&conf,
    MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK |
    MBEDTLS_SSL_TLS1_3_KEY_EXCHANGE_MODE_PSK_EPHEMERAL);

mbedtls_ssl_conf_new_session_tickets(&conf, 1);
```

Cleanup order was adjusted: all MbedTLS context objects are freed in the correct reverse-initialization order, which is mandatory to avoid use-after-free in the MbedTLS internals.

**Assessment:** The PSA crypto initialization (`psa_crypto_init()`) is correctly placed before any TLS operation. Session tickets with a 24-hour lifetime are appropriate for a local web UI. The use of PSK-only key exchange modes for TLS 1.3 is a reasonable conservative choice for a localhost server. The cleanup sequence follows MbedTLS best practices.

**Concern:** The session ticket key is derived from the DRBG at startup. If the process is restarted, existing sessions become invalid (this is expected behavior, not a bug). The 86,400-second ticket lifetime is generous — consider whether shorter lifetimes are appropriate if the web UI is exposed beyond localhost.

#### 2.2 PeerCache Removal

PeerCache was a bandwidth-optimization overlay network feature that cached chunk availability. It has been entirely removed.

**Files deleted:**
- `srchybrid/PeerCacheClient.cpp`
- `srchybrid/PeerCacheFinder.cpp`
- `srchybrid/PeerCacheFinder.h`
- `srchybrid/PeerCacheSocket.h`
- `srchybrid/ClientVersionInfo.h` (153-line header, consolidated elsewhere)

**Data removed from `Preferences.cpp`:**
- `cumUpDataPort_PeerCache`, `sesUpDataPort_PeerCache`
- `cumDownDataPort_PeerCache`, `sesDownDataPort_PeerCache`
- `m_uPeerCacheLastSearch`, `m_bPeerCacheWasFound`
- `m_bPeerCacheEnabled`, `m_nPeerCachePort`, `m_bPeerCacheShow`

**Assessment:** PeerCache was already effectively dead (the overlay network shut down years ago). Removing it reduces attack surface, binary size, and maintenance burden. No functional regression for current users. The statistics fields should be cleared from any existing `preferences.ini` on first run — this should be verified in the preferences loading code to avoid spurious log warnings.

#### 2.3 Redesigned Options → Directories / UNC Shares

The `DirectoryTreeCtrl` gained a new `AddDirectory(const CString &strDir)` method that handles UNC paths distinctly from local paths. UNC shares are now tracked in `m_aUNCshares` (a sorted array), replacing the previous `m_lstUNC` linked list.

The sorted insertion logic in `AddDirectory`:
```cpp
INT_PTR i = m_aUNCshares.GetCount();
if (!i)
    m_aUNCshares.Add(sShare);
else
    while (--i >= 0) {
        int cmp = sShare.CompareNoCase(m_aUNCshares[i]);
        if (cmp >= 0) {
            if (cmp)
                m_aUNCshares.InsertAt(i + 1, sShare);
            break;
        }
    }
```

**Assessment:** The sorted insertion traverses backwards which is efficient for the typical small number of UNC shares. The case-insensitive comparison is correct for Windows paths. The duplicate detection (`if (cmp)`) before insertion is correct. One edge case: if `i` reaches `-1` without breaking, the loop exits without inserting — this means shares lexicographically before all existing entries are silently dropped. This is a **bug**: the insert when `i == -1` (prepend case) is missing. A `if (i < 0) m_aUNCshares.InsertAt(0, sShare);` after the loop would fix it.

`Preferences.cpp` gained a `MovePreferences()` helper to consolidate file-relocation logic during config directory migration. This is a straightforward refactor with no behavioral change.

#### 2.4 `ChatSelector.cpp` (+159 lines)

Significant additions to the chat subsystem. Exact nature of changes not reconstructed from stat alone, but the large line count suggests new chat message handling or filtering functionality.

#### 2.5 Minor Changes

- `emule.rc2`: copyright updated to "2002-2024"
- `emuleWin32.manifest` / `emulex64.manifest`: version bumped to `0.70.1.5`
- Language project files (`zh_TW.vcxproj`): minor property updates
- Widespread pointer-style consistency cleanup continued from commit 1

---

## Part 2: v0.70b → v0.72a

### Commit 3 — `24d1de7` (2026-01-05)
**"Added ARM64 platform. Updated some older code, fixed a number of issues and regressions. Libraries: updated MbedTLS to version 4.0, removed CxImage and libpng."**

**Scope:** 508 files changed, +6,951 / −8,821 lines

#### 3.1 CxImage / libpng Removal — `CaptchaGenerator.cpp` (Major Refactor)

The CAPTCHA generation subsystem was completely rewritten to eliminate the CxImage and libpng external libraries. The new implementation uses only native Windows APIs (GDI/GDI+).

**Before (CxImage):**
```cpp
CxImage *pimgResult = new CxImage(
    nLetterCount > 1 ? ((LETTERSIZE) + nLetterCount * (CROWDEDSIZE)) : (LETTERSIZE),
    48, 1, CXIMAGE_FORMAT_BMP);
pimgResult->SetPaletteColor(0, 255, 255, 255);
pimgResult->SetPaletteColor(1, 0, 0, 0, 0);
pimgResult->Clear();
```

**After (GDI/GDI+):**
```cpp
HBITMAP m_hbmpCaptcha = ::CreateDIBSection(NULL, (BITMAPINFO*)&bmiMono,
    DIB_RGB_COLORS, &pv, NULL, 0);
HDC hdc = ::CreateCompatibleDC(NULL);
HFONT hFont = CreateFontIndirect(&m_LF);
```

Letter rotation is now implemented using `PlgBlt()` (parallelogram blit from GDI), which achieves the same rotation effect as CxImage's `Rotate2()` but without the library dependency. `<atlimage.h>` (ATL's GDI+ wrapper) is used for JPEG/PNG encoding of the final CAPTCHA image.

**Assessment:** Correct approach — CxImage is unmaintained and adds significant build complexity. The GDI `PlgBlt()` approach is a valid and efficient replacement for image rotation. ATL's `CImage` class handles encoding correctly for MFC applications. The net effect is +140/-13 in `CaptchaGenerator.cpp`, meaning the new code is actually larger (the GDI approach is more verbose), but the dependency elimination justifies this.

**Regression risk:** Low. CAPTCHAs are only used in the web UI's anti-spam flow, not in the core P2P protocol. Visual output should be equivalent. Resource handles (HDC, HBITMAP, HFONT) must be correctly cleaned up — this should be code-reviewed manually to confirm no GDI handle leaks.

#### 3.2 `VisualStylesXP.cpp/h` Removed

The `VisualStylesXP` module was deleted. This was a runtime-loading wrapper for `uxtheme.dll` to support Windows XP visual styles dynamically. Its removal signals the project no longer targets Windows XP.

**Implication:** The minimum OS is now implicitly Windows Vista or later (consistent with the manifests, which already listed Windows 10/8.1/8 as supported). This is a reasonable modernization.

#### 3.3 `ImportParts.cpp` Removed

The "Import Parts" feature (which allowed importing partial downloads from other eMule installations or clients) was removed. This was a rarely-used maintenance feature.

#### 3.4 MbedTLS 4.0 Update

MbedTLS updated from v3.x to v4.0 (major version bump). The primary visible change:

- The `threading_alt.h` file moved from `mbedtls/include/mbedtls/` to `mbedtls/tf-psa-crypto/include/mbedtls/` — reflecting the PSA crypto subsystem becoming a separately structured sub-project within MbedTLS 4.0.
- `MBEDTLS_ALLOW_PRIVATE_ACCESS` preprocessor define added in the project (commit 4), needed because MbedTLS 4.0 restricts access to struct internals by default.

**Assessment:** MbedTLS 4.0 is a significant API-breaking release. The migration appears to have been done carefully. The `MBEDTLS_ALLOW_PRIVATE_ACCESS` define is a migration aid that allows existing code using internal MbedTLS structs to compile without full API migration — this is acceptable as a transitional measure but should eventually be resolved by migrating to the public PSA API.

#### 3.5 ARM64 Platform Support

New files added:
- `srchybrid/res/emuleARM64.manifest`
- `srchybrid/emule.slnx` (new VS 2022 solution XML format)
- `srchybrid/lang/lang.slnx`
- `srchybrid/Ring.h` (new ring buffer utility header)

ARM64 configurations added to `emule.vcxproj` and `emule.sln` with toolset `v145` and the ARM64-specific compiler flag `/Zc:enumTypes`.

**Note:** ARM64 support added in this commit is entirely removed in commit 4. See section 4.1.

#### 3.6 `ColorButton.cpp/h` Refactoring

Style reformatting to comma-first initialization lists:
```cpp
// Before: trailing-comma style
CColorButton::CColorButton() :
    _Inherited(),
    m_Color(CLR_DEFAULT),

// After: leading-comma style
CColorButton::CColorButton()
    : _Inherited()
    , m_Color(CLR_DEFAULT)
```

Several getter/setter methods removed from the public interface and either inlined in the header or eliminated entirely: `GetColor()`, `GetDefaultColor()`, `SetDefaultColor()`, `SetCustomText()`, `SetDefaultText()`, `SetTrackSelection()`.

**Assessment:** Pure cosmetic/style refactor. No behavioral change. The comma-first style is a personal preference — it has the advantage of making diff output cleaner for multi-item additions, but is non-standard in the MFC ecosystem.

#### 3.7 Version and Manifest

- `emule.rc2`: copyright updated to "2002-2026"; added `#pragma code_page(65001)` (UTF-8 source encoding declaration); added `VS_FF_PRERELEASE` flag for `_BETA` and `_DEVBUILD` builds.
- `emuleWin32.manifest`: version bumped to `0.72.0.1`.
- New `emuleARM64.manifest` added (later removed).

---

### Commit 4 — `18911c5` (2026-03-23)
**"VS2022 x64 porting for v0.72a build workspace"**

**Scope:** 3 files changed, +54 / −86 lines
**Author:** Threepwood-7 (different contributor from the other 3 commits)

This commit is a targeted build-system fix to make the v0.72a source tree compile correctly in the community workspace layout.

#### 4.1 ARM64 Removal

All ARM64 build configurations added in commit 3 are removed from `emule.sln` and `emule.vcxproj`. This dramatically simplifies the solution file (57 lines removed from `emule.sln`, 50 from `emule.vcxproj`).

**Rationale (from commit message):** The community workspace targets x64 only. ARM64 support requires a separate toolchain setup that the community build environment does not provide.

**Assessment:** Pragmatic decision. ARM64 Windows is not a primary eMule deployment target. The `emuleARM64.manifest` file from commit 3 remains in the tree as minor cruft. The `emule.slnx` wrapper was later removed once `emule.vcxproj` became the sole app build entrypoint.

#### 4.2 Toolset Upgrade: `v141_xp` → `v143`

The Visual C++ platform toolset was updated from `v141_xp` (VS 2017 with Windows XP support) to `v143` (VS 2022). This is consistent with the removal of `VisualStylesXP.cpp` in commit 3.

**Impact:** The project no longer produces XP-compatible binaries. Minimum OS is now Windows Vista (though practically Windows 10 given the manifests).

#### 4.3 Submodule Path Layout

All dependency include/library paths updated from upstream names to `eMule-`-prefixed names:

| Before | After |
|--------|-------|
| `../cryptopp/` | `../eMule-cryptopp/` |
| `../mbedtls/` | `../eMule-mbedtls/` |
| `../miniupnpc/` | `../eMule-miniupnp/` |
| `../ResizableLib/` | `../eMule-ResizableLib/` |
| `../zlib/` | `../eMule-zlib/` |

Also added `miniupnp` root directory to the include search path (previously only the `miniupnpc/` subdirectory was included).

**Assessment:** This reflects the community fork's dependency naming convention (prefixing all submodules with `eMule-` for clarity). Combined with the recent commits in the current branch (`v0.72a-community`) that further retarget these paths, this forms a coherent build workspace layout.

#### 4.4 WebSocket.cpp — Unicode-Safe Certificate Loading (Critical Fix)

The most important source code change in this commit: certificate and private key files can now be loaded from paths containing non-ASCII characters (international characters, spaces, etc.).

**Root cause:** `mbedtls_x509_crt_parse_file()` and `mbedtls_pk_parse_keyfile()` internally use `fopen()` with a narrow (ANSI) string, which cannot represent Unicode paths on Windows.

**Before:**
```cpp
ret = mbedtls_x509_crt_parse_file(&srvcert, thePrefs.GetWebCertPath());
ret = mbedtls_pk_parse_keyfile(&pkey, thePrefs.GetWebKeyPath(), NULL, ...);
```

**After:**
```cpp
CFile certFile;
if (!certFile.Open(thePrefs.GetWebCertPath(), CFile::modeRead | CFile::shareDenyWrite)) {
    ret = MBEDTLS_ERR_X509_FILE_IO_ERROR;
} else {
    const ULONGLONG fileLen = certFile.GetLength();
    std::vector<unsigned char> buf(static_cast<size_t>(fileLen) + 1, 0);
    certFile.Read(buf.data(), static_cast<UINT>(fileLen));
    certFile.Close();
    ret = mbedtls_x509_crt_parse(&srvcert, buf.data(), buf.size());
}
```

The buffer is zero-initialized (`+ 1, 0`), ensuring null-termination for MbedTLS's PEM parser.

**Assessment:** Correct fix. `MFC CFile::Open()` uses `CreateFileW()` internally and handles Unicode paths correctly. The `shareDenyWrite` flag prevents certificate file modification while it is being read. The `+1` null-terminator ensures compatibility with both PEM (text, null-terminated expected) and DER (binary, length-delimited — the null byte is benign). The same approach is applied identically for the private key file.

**Caveat:** `certFile.GetLength()` returns `ULONGLONG`. The cast to `size_t` could truncate on 32-bit builds if the certificate file is theoretically larger than 4 GB — not a practical concern for TLS certificates but worth noting. The subsequent cast to `UINT` for `Read()` is consistent with this.

#### 4.5 `bcrypt.lib` Added

`bcrypt.lib` added to AdditionalDependencies for both Debug and Release x64 configurations. This is the Windows CNG (Cryptography Next Generation) library, required by MbedTLS 4.0's PSA crypto backend on Windows (used for hardware-backed RNG and certain cipher operations).

**Assessment:** Required and correct. Without this, the TLS 1.3 PSA initialization (`psa_crypto_init()`) would fail at runtime with a missing symbol error.

---

## Summary and Risk Matrix

| Area | Change | Risk | Notes |
|------|--------|------|-------|
| File I/O | IOCP-based async write thread | Medium | Core download path changed; lifecycle correctness critical |
| TLS | TLS 1.3 via MbedTLS + PSA | High | Security-critical; implementation looks correct |
| PeerCache | Complete removal | Low | Feature was already dead; no runtime regression |
| UNC paths | DirectoryTreeCtrl + Prefs redesign | Low-Medium | Bug: prepend case missing in sorted UNC insert |
| CxImage | Replaced with GDI/GDI+ | Low | CAPTCHA only; verify GDI handle cleanup |
| libpng | Removed (ATL `CImage` for encoding) | Low | Standard Windows API; no third-party risk |
| MbedTLS | v3 → v4.0 (major update) | High | `MBEDTLS_ALLOW_PRIVATE_ACCESS` is a migration bridge |
| ARM64 | Added then removed | Info | Experimental; removed cleanly from build configs |
| XP support | Dropped (`v141_xp` → `v143`) | Low | Expected; aligns with manifest OS targeting |
| Unicode paths | CFile-based cert loading | Low | Correct fix; minor cast concern on 32-bit |
| Build paths | `eMule-` prefix convention | Low | Pure build system; no runtime impact |
| VisualStylesXP | Removed | Low | Dead code for modern Windows |
| ImportParts | Removed | Low | Rarely-used feature |

### Identified Defects

1. **`DirectoryTreeCtrl::AddDirectory()` — missing prepend case**
   When a UNC share sorts lexicographically before all existing entries in `m_aUNCshares`, the reverse-search loop reaches index `-1` and exits without inserting the share. **Severity: Low** (UNC shares are rarely added in large numbers; the first share is always inserted correctly since the early-return handles the empty case).

2. **`CaptchaGenerator.cpp` — GDI handle leak potential**
   The refactored code acquires multiple GDI handles (HDC, HBITMAP, HFONT). Verify that all exception/early-return paths call the corresponding `DeleteObject()`/`DeleteDC()`. **Severity: Low** (CAPTCHA generation is infrequent and isolated).

3. **`MBEDTLS_ALLOW_PRIVATE_ACCESS` — technical debt**
   Using internal MbedTLS struct fields via this define will break on future MbedTLS updates. **Severity: Low** (currently functional; should be tracked for next MbedTLS update).

### Recommendations

1. **Security audit of TLS 1.3 session ticket implementation** — session ticket key rotation policy and entropy source should be verified.
2. **Fix the UNC share prepend bug** in `DirectoryTreeCtrl::AddDirectory()`.
3. **GDI handle audit** of the new `CaptchaGenerator.cpp` implementation.
4. **Plan migration away from `MBEDTLS_ALLOW_PRIVATE_ACCESS`** to the public PSA API before the next MbedTLS major update.
5. **Consider shorter TLS session ticket lifetime** for the web UI (current: 86,400 seconds / 24 hours).

---

*End of report. Generated from git history analysis of the eMule community fork.*
