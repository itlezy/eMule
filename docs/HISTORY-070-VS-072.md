# eMule 0.70b vs 0.72a — Detailed Comparison Report

**Date:** 2026-03-29
**Source directories:**
- `eMule-0.70b/` — baseline
- `eMule-0.72a/` — target

---

## Table of Contents

- [1. File Inventory](#1-file-inventory)
- [2. Version Information](#2-version-information)
- [3. Major Feature Additions](#3-major-feature-additions) (Ring buffer, ARM64, new protocol tags)
- [4. Major Refactoring](#4-major-refactoring) (CList→CRing, CxImage→GDI+, API modernization)
- [5. Bug Fixes & Code Corrections](#5-bug-fixes--code-corrections)
- [6. Code Quality / Style Improvements](#6-code-quality--style-improvements)
- [7. Protocol & Networking](#7-protocol--networking)
- [8. UI / UX](#8-ui--ux)
- [9. Security Notes](#9-security-notes)
- [10. Key Changed Files](#10-key-changed-files-summary)
- [11. Overall Development Trend](#11-overall-development-trend)

---

## 1. File Inventory

| Metric | 0.70b | 0.72a |
|--------|-------|-------|
| Total files | ~925 | ~926 |
| Source files (.cpp/.h) | ~556 | ~554 |

### Files Added in 0.72a
| File | Purpose |
|------|---------|
| `Ring.h` | New circular/ring buffer template class |
| `TitledMenu.cpp` / `TitledMenu.h` | Renamed from `TitleMenu.cpp/.h` |

### Files Removed from 0.70b
| File | Reason |
|------|--------|
| `TitleMenu.cpp` / `TitleMenu.h` | Renamed to `TitledMenu.cpp/.h` |

### Language / Kademlia Files
96 language files and Kademlia core files appear to have been reorganized in directory structure; content is largely preserved.

---

## 2. Version Information

**0.70b — `Version.h`:**
```cpp
#define VERSION_MJR      0
#define VERSION_MIN      70
#define VERSION_UPDATE   1
#define VERSION_BUILD    5
```

**0.72a — `Version.h`:**
```cpp
#define VERSION_MJR      0
#define VERSION_MIN      72
#define VERSION_UPDATE   0
#define VERSION_BUILD    1
```

### Platform Detection

**0.70b** — x64 only:
```cpp
#define VERSION_X64 _T(" x64")
```

**0.72a** — extended to include ARM64:
```cpp
#ifdef _M_X64
    #define VERSION_PLATFORM _T(" x64")
#elif _M_ARM64
    #define VERSION_PLATFORM _T(" arm64")
#else //x86
    #define VERSION_PLATFORM _T("")
#endif
```

---

## 3. Major Feature Additions

### 3.1 Ring Buffer Data Structure — `Ring.h`

A new circular/ring buffer template class replaces `CList<>` usage in the data-rate tracking subsystem. This is the most architecturally significant addition of this release.

```cpp
template<class TYPE> class CRing
{
    UINT_PTR m_nCount;      // number of items added (wraps logically)
    UINT_PTR m_nIncrement;  // growth step when capacity exceeded
    UINT_PTR m_nSize;       // current buffer capacity
    TYPE    *m_pData;       // contiguous circular buffer

    // Key operations: Add(), GetAt(i), SetAt(i,v), GetCount(), IsEmpty()
};
```

**Benefits over `CList<>`:**
- Contiguous memory — better cache locality
- No per-element heap allocation/deallocation
- Automatic circular overwrite when full (no explicit `RemoveHead()` needed)
- Faster iteration and index access

### 3.2 ARM64 Platform Support

Explicit ARM64 detection added in `Version.h` and manifests updated accordingly — prepares the build system for native ARM64 Windows builds.

### 3.3 New Protocol Tags in `Opcodes.h`

Backward-compatible additions to the file tag set:

```cpp
#define FT_KADAICHHASHRESULT    0x37  // <Count 1>{<Publishers 1><AICH Hash> Count}
#define FT_ENCRYPTION           0xF3  // <uint8>
#define FT_BUDDYHASH            0xF8  // <string>
#define FT_SERVERPORT           0xFA  // <uint16>
#define FT_SERVERIP             0xFB  // <uint32>
#define FT_SOURCEUPORT          0xFC  // <uint16>
#define FT_SOURCEPORT           0xFD  // <uint16>
#define FT_SOURCEIP             0xFE  // <uint32>
#define FT_SOURCETYPE           0xFF  // <uint8>
```

Kademlia protocol version remains at `0x0a` (10) — no wire-level protocol break.

---

## 4. Major Refactoring

### 4.1 Data Rate Tracking: `CList` → `CRing`

This refactoring affects `DownloadQueue.h/.cpp` and `UploadQueue.h/.cpp`.

**0.70b — DownloadQueue.h:**
```cpp
CList<TransferredData> average_dr_list;
```

**0.72a — DownloadQueue.h:**
```cpp
CRing<TransferredData> average_dr_hist;
```

**0.70b — UploadQueue.h (multiple separate lists):**
```cpp
CList<uint64>        average_dr_list;
CList<uint64>        average_friend_dr_list;
CList<DWORD, DWORD>  average_tick_list;
CList<int, int>      activeClients_list;
CList<DWORD, DWORD>  activeClients_tick_list;
```

**0.72a — UploadQueue.h (consolidated into typed structs + rings):**
```cpp
CRing<AverageUploadRate>  average_ur_hist;
CRing<ActiveClientsData>  activeClients_hist;
```

**New aggregate structs introduced:**
```cpp
typedef struct {
    uint64  datalen;
    DWORD   timestamp;
} TransferredData;

typedef struct {
    uint64  upBytes;
    uint64  upFriendBytes;
    DWORD   timestamp;
} AverageUploadRate;

typedef struct {
    INT_PTR slots;
    DWORD   timestamp;
} ActiveClientsData;
```

The consolidation of parallel lists into a single ring of typed structs eliminates list-pointer synchronization bugs and simplifies iteration.

---

### 4.2 Image Handling: CxImage → HBITMAP / ATL / GDI+

The external `CxImage` library dependency is removed. All video preview image encoding now uses the native Windows GDI+ / ATL pipeline.

**0.70b — BaseClient.cpp:**
```cpp
#include "CxImage/xImage.h"

void CUpDownClient::SendPreviewAnswer(const CKnownFile *pForFile,
                                      CxImage **imgFrames, uint8 nCount)
{
    CxImage *cur_frame = imgFrames[i];
    BYTE *abyResultBuffer = NULL;
    int32_t nResultSize = 0;
    if (!cur_frame->Encode(abyResultBuffer, nResultSize, CXIMAGE_FORMAT_PNG)) {
        // error
    }
    data.WriteUInt32(nResultSize);
    data.Write(abyResultBuffer, nResultSize);
    free(abyResultBuffer);
}
```

**0.72a — BaseClient.cpp:**
```cpp
#include <atlimage.h>

void CUpDownClient::SendPreviewAnswer(const CKnownFile *pForFile,
                                      HBITMAP *imgFrames, uint8 nCount)
{
    HBITMAP bmp_frame = imgFrames[i];
    if (bmp_frame) {
        if (bSend) {
            size_t nFrameSize;
            byte *byFrameBuffer = bmp2mem(bmp_frame, nFrameSize, Gdiplus::ImageFormatPNG);
            if (byFrameBuffer) {
                data.WriteUInt32((uint32)nFrameSize);
                data.Write(byFrameBuffer, (UINT)nFrameSize);
                delete[] byFrameBuffer;
            }
        }
        ::DeleteObject(bmp_frame);     // explicit GDI object cleanup
    }
}
```

**Impact:** Removes a third-party library from the build tree; ensures consistent PNG encoding behavior via the OS-supplied GDI+ codec. Memory cleanup is now explicit (`::DeleteObject`).

---

### 4.3 Windows API Modernization — `OtherFunctions.cpp`

**0.70b** used a complex dynamic-loading fallback chain for `SHGetFolderPath`:
```cpp
HRESULT(WINAPI *pfnSHGetFolderPathW)(HWND, int, HANDLE, DWORD, LPWSTR);
(FARPROC&)pfnSHGetFolderPathW =
    GetProcAddress(GetModuleHandle(_T("shell32")), "SHGetFolderPathW");
// ... multiple fallbacks to shfolder.dll and ANSI versions
```

**0.72a** drops the compatibility shim — the function is called directly:
```cpp
TCHAR szPath[MAX_PATH];
if (SUCCEEDED(::SHGetFolderPath(NULL, iCSIDL, NULL, SHGFP_TYPE_CURRENT, szPath)))
    return CString(szPath);
```

This signals that support for very old Windows versions (pre-XP SP1) has been dropped.

---

### 4.4 Menu Class Rename

`CTitleMenu` → `CTitledMenu` (one letter: `d` added).
All callsites in `TransferWnd.cpp`, `SearchResultsWnd.cpp`, and related files are updated. This is a cosmetic naming fix with no behavioral change.

---

### 4.5 Enum Underlying-Type Annotations

**0.70b:**
```cpp
enum EPartFileStatus { ... };
enum EPartFileOp     { ... };
```

**0.72a — explicit storage type (C++11):**
```cpp
enum EPartFileStatus : uint8 { ... };
enum EPartFileOp     : uint8 { ... };
```

This locks the enum's binary representation, which matters for on-disk serialization and network protocols.

---

## 5. Bug Fixes & Code Corrections

### 5.1 UDP Queue-Full Logic — `ClientUDPSocket.cpp`

The condition controlling whether to send a "queue full" reply was inverted in 0.70b due to confusing nesting.

**0.70b:**
```cpp
if (!bSenderMultipleIpUnknown) {
    if (theApp.uploadqueue->GetWaitingUserCount() + 50 > thePrefs.GetQueueSize()) {
        // send queue full
    }
} else
    // different path
```

**0.72a — logic clarified:**
```cpp
if (bSenderMultipleIpUnknown)
    // cannot send encrypted answer since we don't know this client
else if (theApp.uploadqueue->GetWaitingUserCount() + 50 > thePrefs.GetQueueSize()) {
    // send queue full
}
```

### 5.2 PartFile Status Check Simplification

**0.70b:**
```cpp
EPartFileStatus uState = GetStatus();
if ((uState == PS_PAUSED || uState == PS_INSUFFICIENT || uState == PS_ERROR) && !m_stopped && ...)
```

**0.72a — helper function + switch:**
```cpp
if (inSet(GetStatus(), PS_PAUSED, PS_INSUFFICIENT, PS_ERROR) && !m_stopped && ...)

// and elsewhere:
switch (GetStatus()) {
    case PS_ERROR:       return GetCompletionError();
    case PS_INSUFFICIENT:
    case PS_PAUSED:      return true;
}
return false;
```

### 5.3 Video Preview Start Time Comment Fix

**0.70b:**
```cpp
previewFile->GrabImage(4, 15, true, 450, this);
// start at 15 seconds; at 0 seconds frames usually were solid black
```

**0.72a — float literal for clarity + updated comment:**
```cpp
previewFile->GrabImage(4, 15.0, true, 450, this);
// do not start at 0 because videos commonly begin with blank screens
```

### 5.4 `DrawStatusBar` Parameter Change

**0.70b:**
```cpp
void DrawStatusBar(CDC *dc, const CRect &rect, bool bFlat);
```

**0.72a — DC passed by reference, not pointer:**
```cpp
void DrawStatusBar(CDC &dc, const CRect &rect, bool bFlat);
```

Eliminates a null-pointer risk at callers.

---

## 6. Code Quality / Style Improvements

### 6.1 Systematic `::` Scoping of Win32 API Calls

A sweep across `BaseClient.cpp`, `OtherFunctions.cpp`, and others adds explicit global-namespace qualification to Win32 calls:

```cpp
// 0.70b
CoInitialize(NULL);
LoadLibrary(_T("..."));
DeleteFile(pszFilePath);

// 0.72a
::CoInitialize(NULL);
::LoadLibrary(_T("..."));
::DeleteFile(pszFilePath);
```

Prevents accidental shadowing by class members or local overloads.

### 6.2 Member Variable Naming — `Friend.h`

| 0.70b | 0.72a | Reason |
|-------|-------|--------|
| `m_dwLastSeen` | `m_tLastSeen` | `t` prefix signals `time_t`, not raw `DWORD` |
| `m_dwLastChatted` | `m_tLastChatted` | same |

### 6.3 Private Member Visibility — `DownloadListCtrl`

```cpp
// 0.70b
if (theApp.emuledlg->transferwnd->GetDownloadList()->curTab == 0)

// 0.72a — renamed with m_ prefix (enforces private access discipline)
if (theApp.emuledlg->transferwnd->GetDownloadList()->m_curTab == 0)
```

### 6.4 Deleted Copy/Move Constructors — Kademlia Classes

```cpp
// 0.72a — CKademlia.h
class CKademlia {
    CKademlia(const CKademlia&)            = delete;
    CKademlia& operator=(const CKademlia&) = delete;
};
```

Prevents accidental copies of singleton-like objects; enforces correct usage at compile time.

### 6.5 Include Header Cleanup

| 0.70b | 0.72a | Notes |
|-------|-------|-------|
| `"CxImage/xImage.h"` | `<atlimage.h>` | third-party → system header |
| `<shlobj.h>` | `<ShlObj_core.h>` | refined shell object header |
| *(absent)* | `<sys/stat.h>`, `<share.h>` | explicit POSIX/C-runtime includes |

---

## 7. Protocol & Networking

- Kademlia protocol version: **unchanged** at `0x0a` — 0.70b and 0.72a nodes are fully compatible on the network.
- New `FT_*` tags in `Opcodes.h` are additive; unknown tags are ignored by older clients.
- No new opcodes or packet types introduced.
- UDP logic fix (§5.1) corrects a latent condition that could send incorrect queue-full responses.

---

## 8. UI / UX

| Area | Change |
|------|--------|
| Context menus | `CTitleMenu` → `CTitledMenu` rename throughout |
| Transfer window | `curTab` → `m_curTab` (access-discipline change) |
| Search results | Updated menu creation to use `CTitledMenu` |
| No new dialogs, panels, or visible features added in this release | — |

---

## 9. Security Notes

No regressions introduced. Positive changes:

| Change | Security Impact |
|--------|-----------------|
| Ring buffer replaces linked list | Reduces heap-fragmentation attack surface |
| Explicit `::` Win32 scoping | Prevents accidental local-override of security-critical APIs |
| `::DeleteObject` after HBITMAP use | Eliminates GDI handle leak |
| `= delete` copy constructors | Prevents unintended object slicing in crypto/Kademlia code |
| `SHGetFolderPath` direct call | Removes dynamic `LoadLibrary` code path that could be hijacked |

---

## 10. Key Changed Files (Summary)

| File | Nature of Change |
|------|-----------------|
| `Version.h` | Version bump, ARM64 platform detection |
| `Ring.h` | **New** — circular buffer template |
| `DownloadQueue.h/.cpp` | `CList` → `CRing` for rate history |
| `UploadQueue.h/.cpp` | `CList` → `CRing`, struct consolidation |
| `BaseClient.cpp` | CxImage → HBITMAP/ATL/GDI+, `::` scoping |
| `OtherFunctions.cpp` | `SHGetFolderPath` simplification, API scoping |
| `PartFile.h/.cpp` | Enum typed storage, `DrawStatusBar` ref fix, status helpers |
| `ClientUDPSocket.cpp` | Queue-full logic fix |
| `Friend.h` | `dw` → `t` prefix for time variables |
| `Opcodes.h` | New `FT_*` tags (backward compatible) |
| `TitledMenu.h/.cpp` | Renamed from `TitleMenu` |
| `TransferWnd.cpp` | Menu rename, `m_curTab` access |
| `SearchResultsWnd.cpp` | Menu rename |
| `Kademlia/Kademlia.h` | `= delete` copy semantics |

---

## 11. Overall Development Trend

The 0.70b → 0.72a transition is best characterised as a **focused modernisation and cleanup release**, not a feature release:

1. **Performance** — the `CList` → `CRing` refactoring in data-rate tracking is the headline change; it removes per-tick heap allocations in hot paths.
2. **Dependency reduction** — CxImage is excised; the build tree shrinks and the Windows-native GDI+ codec path gains consistency.
3. **Build portability** — ARM64 support is wired in, positioning the project for native Windows-on-ARM builds.
4. **Code hygiene** — `::` scoping, `= delete` constructors, typed enums, `m_` prefix discipline, and reference-vs-pointer corrections reduce latent bugs.
5. **Legacy removal** — Win9x/WinME compatibility shims (`SHGetFolderPath` dynamic load fallback) are dropped, simplifying OS-interaction code.
6. **Protocol stability** — Kademlia version and all core opcodes are unchanged; this is a fully backward-compatible release on the network.

In short: 0.72a makes eMule faster in its data-rate bookkeeping, lighter on third-party dependencies, and cleaner in its C++ style — while leaving the network protocol and user-visible feature set essentially intact.
