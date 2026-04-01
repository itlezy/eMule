# WWMOD - What Would a Modern OS Do?

**Windows 10+ Compatibility Audit**
**Date:** 2026-03-31
**Branch:** v0.72a-broadband-dev
**Scope:** ~528 C/C++ source files in `srchybrid/`

---

## Table of Contents

- [Purpose](#purpose)
- [Category A — Dead Code](#category-a---dead-code) (WWMOD_001–009)
- [Category B — Upgrade Targets](#category-b---upgrade-targets) (WWMOD_010–047)
- [Category C — Archaic Limits & Approaches](#category-c---archaic-limits--approaches) (WWMOD_048–050)

---

## Purpose

This document catalogs every pattern in the eMule codebase that is archaic, dead, or
suboptimal when the deployment target is Windows 10 version 1607+ only. Each item has a
unique `WWMOD_nnn` identifier for tracking.

Items are grouped into categories:
- **A** - Dead Code (code that is literally unreachable or functionally dead on Win10+)
- **B** - Upgrade Targets (controls, APIs, patterns that have modern replacements)
- **C** - Archaic Limits & Approaches (hard-coded values and design patterns from the 2000s)

Severity levels:
- **P0** - Actively harmful (security risk, data loss, wrong behavior on modern systems)
- **P1** - Significant improvement opportunity
- **P2** - Nice-to-have cleanup
- **P3** - Cosmetic / long-term aspiration

Cross-references to existing docs: `AUDIT-DEADCODE.md`, `FEATURE-MODERN-LIMITS.md`,
`PLAN-MODERNIZATION-2026.md`, `REFACTOR-TASKS.md`.

---

## Category A - Dead Code

### WWMOD_001 - Dead Win9x/NT4 `#ifndef` Guards in `stdafx.h`

**Severity:** P2
**Files:** `srchybrid/stdafx.h:150-196`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

Constants `EWX_FORCEIFHUNG`, `WS_EX_LAYOUTRTL`, `LAYOUT_RTL`, `COLOR_HOTLIGHT`,
`WS_EX_LAYERED`, `LWA_COLORKEY`, `LWA_ALPHA`, `HDF_SORTUP`, `HDF_SORTDOWN`,
`COLOR_GRADIENTACTIVECAPTION`, `LVBKIF_TYPE_WATERMARK`, `LVBKIF_FLAG_ALPHABLEND` are
all wrapped in `#ifndef` guards. Every one of these has been defined in the Windows SDK
since Windows XP (SDK 5.1) at latest. With `WINVER=0x0A00` and the v143 toolset, the
guards never fire.

**Action:** Remove all `#ifndef`/`#define`/`#endif` blocks for these constants. They are
unconditionally defined by the SDK headers included earlier.

---

### WWMOD_002 - `MAXCON5WIN9X` Constant

**Severity:** P2
**Files:** `srchybrid/Opcodes.h:108`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

```cpp
#define MAXCON5WIN9X   10
```

Connection throttle for Windows 9x. Meaningless on Win10. Referenced nowhere in
active code paths.

**Action:** Remove the define.

---

### WWMOD_003 - Win98 Unicode Workaround Comments and Calls

**Severity:** P2
**Files:** 8+ files including `MuleListCtrl.cpp:115`, `MuleToolBarCtrl.cpp:129`,
`DirectoryTreeCtrl.cpp:189`, `SharedDirsTreeCtrl.cpp:111`, `SmileySelector.cpp:128`,
`StatisticsDlg.cpp:153`, `TreePropSheet.cpp:727`, `ArchivePreviewDlg.cpp:230`,
`IPFilterDlg.cpp:176`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

Pattern: `// Win98: Explicitly set to Unicode to receive Unicode notifications.`
followed by explicit `SendMessage(CCM_SETUNICODEFORMAT, TRUE)` calls.

On Windows 10 with `CharacterSet=Unicode`, all common controls are natively Unicode.
These calls are no-ops.

**Action:** Remove the `SendMessage(CCM_SETUNICODEFORMAT, ...)` calls and their
associated comments. Verify by grepping for `CCM_SETUNICODEFORMAT`.

---

### WWMOD_004 - Win98/ME Icon Resource Limit Workaround in `SelfTest.cpp`

**Severity:** P2
**Files:** `srchybrid/SelfTest.cpp:70-97`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

Large block of comments documenting the 1 MB icon resource limit on Win98/ME and
strategies to order icons to avoid it. The actual icon ordering in the `.rc` file may
still reflect this obsolete constraint.

**Action:** Remove the Win98/ME-specific comments. Consider reordering icons logically
rather than by ancient resource-limit constraints.

---

### WWMOD_005 - Dead MSVC Version Guards (`_MSC_VER < 1400`)

**Severity:** P2
**Files:** `srchybrid/stdafx.h:47-49,62-64`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

```cpp
#if _MSC_VER<1400    // VS 2005
#pragma warning(disable:4217)
#endif
#if _MSC_VER<1400
#pragma warning(disable:4529)
#endif
```

The project requires `v143` (VS 2022, `_MSC_VER >= 1930`). All `_MSC_VER < 1400`
and `_MSC_VER >= 1400` guards are dead or always-true.

**Action:** Remove all `#if _MSC_VER<1400` blocks (dead code). Convert all
`#if _MSC_VER>=1400` blocks to unconditional (always true). Similarly evaluate
`_MSC_VER>=1900` guards (VS 2015, always true with v143).

---

### WWMOD_006 - Dynamic Loading of Always-Present Win10 APIs

**Severity:** P1
**Files:**
- `EmuleDlg.cpp:334` - `GetProcAddress` for `ChangeWindowMessageFilter` (available since Vista)
- `EmuleDlg.cpp:3419-3422` - `LoadLibrary("dwmapi.dll")` + `DwmGetColorizationColor` (DWM is always on in Win10)
- `Preferences.cpp:2818` - `GetProcAddress` for `SHGetKnownFolderPath` (available since Vista)
- `Preferences.cpp:2986-2989` - `LoadLibrary("dwmapi.dll")` + `DwmIsCompositionEnabled` (always TRUE on Win10; DWM cannot be disabled)
- `Mdump.cpp:66-72` - `LoadLibrary("DBGHELP.DLL")` + `MiniDumpWriteDump` (dbghelp.dll ships with Windows since XP SP2 and is always present on Win10)
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

**Action:** Replace dynamic loading with direct static linking:
- `ChangeWindowMessageFilter` -> link directly, or better, use `ChangeWindowMessageFilterEx` (Win7+)
- `DwmGetColorizationColor` / `DwmIsCompositionEnabled` -> link `dwmapi.lib` directly; remove the composition-enabled check entirely (always TRUE)
- `SHGetKnownFolderPath` -> link directly
- `DBGHELP.DLL` -> link `dbghelp.lib` directly

---

### WWMOD_007 - Dead `_WINSOCK_DEPRECATED_NO_WARNINGS` Suppression

**Severity:** P1
**Files:** `srchybrid/stdafx.h:120-122`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

```cpp
#ifndef _WINSOCK_DEPRECATED_NO_WARNINGS
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif
```

This silences deprecation warnings for `inet_addr`, `inet_ntoa`, `gethostbyname`, etc.
Rather than suppressing warnings, the deprecated APIs should be replaced (see WWMOD_030).

**Action:** Remove the suppression after migrating deprecated Winsock calls.

---

### WWMOD_008 - `_CRT_SECURE_NO_DEPRECATE` Blanket Suppression

**Severity:** P1
**Files:** `srchybrid/stdafx.h:93-95`

Disables all CRT security deprecation warnings globally. This hides real issues
with `sprintf`, `strcpy`, etc.

**Action:** Remove the blanket suppression. Fix individual call sites to use safe
variants (`_s` functions, `StringCch*`, or `std::format`).

---

### WWMOD_009 - `WinNT` / Win98 Comments in Active Code

**Severity:** P3
**Files:**
- `TaskbarNotifier.cpp:604` - "WinNT: ExtCreateRegion returns NULL"
- `OtherFunctions.cpp:1776` - "Seen under WinNT 4.0/Win98"
- `Pinger.cpp:30` - "NT4 and Win98 don't respond"
- `ToolTipCtrlX.cpp:444` - "Win98: To draw an empty line..."
- `MuleListCtrl.cpp:1528` - "does not work with Win98 (COMCTL32 v5.8)"
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

**Action:** Remove outdated platform-specific comments. If there are associated
code workarounds, evaluate whether they are still needed.

---

## Category B - Upgrade Targets

### WWMOD_010 - No DPI Awareness

**Severity:** P0
**Files:** `srchybrid/emule.vcxproj:85,112,136`, `srchybrid/res/emulex64.manifest`

The project explicitly sets `<EnableDpiAwareness>false</EnableDpiAwareness>` in all
configurations. The manifest declares `longPathAware` but has **no DPI awareness
declaration**. On modern high-DPI displays (125%, 150%, 200%), eMule is bitmap-
stretched by Windows, resulting in blurry UI.

**Action (multi-step):**
1. Add `<dpiAware>true/pm</dpiAware>` and `<dpiAwareness>PerMonitorV2</dpiAwareness>`
   to the manifest
2. Audit all hardcoded pixel values across dialogs and controls
3. Use `GetDpiForWindow()` / `GetSystemMetricsForDpi()` for layout calculations
4. Ensure all icon/bitmap loading uses DPI-appropriate sizes

This is the single highest-impact modernization item for user-visible quality.

---

### WWMOD_011 - Legacy MFC List Controls (CListCtrl)

**Severity:** P1
**Files:** 66+ files, 271 occurrences of `CListCtrl`/`CTreeCtrl`/`CStatusBarCtrl`/
`CTabCtrl`/`CToolBarCtrl`

Key classes using `CListCtrl`:
- `CMuleListCtrl` (base class for all list views)
- `CDownloadListCtrl`, `CUploadListCtrl`, `CQueueListCtrl`
- `CClientListCtrl`, `CSearchListCtrl`, `CSharedFilesCtrl`
- `CCollectionListCtrl`, `CDownloadClientsCtrl`

`CMuleListCtrl` extends `CListCtrl` with custom drawing, sorting, and column
management. It does **not** use `LVS_OWNERDATA` (virtual list mode), meaning every
item is stored in the control's internal data structure, causing O(n) performance for
large lists.

**Action:**
- Convert `CMuleListCtrl` to use `LVS_OWNERDATA` (virtual list mode) for all list
  views that can hold >1000 items (download, upload, queue, search results, shared
  files). This eliminates per-item memory allocation inside the control and enables
  instant population of lists with 100K+ items.
- Consider `CMFCListCtrl` for built-in sort arrows and modern visual integration
- Migrate `CTabCtrl`-based classes (`CClosableTabCtrl`, `ButtonsTabCtrl`) to
  `CMFCTabCtrl` for modern tab appearance and tear-off support

---

### WWMOD_012 - Legacy CPropertySheet / CPropertyPage Preferences Dialog

**Severity:** P2
**Files:** 37 files, 139 occurrences of `CPropertySheet`/`CPropertyPage`

The preferences dialog uses `CTreePropSheet` (a third-party tree-view wrapper around
`CPropertySheet`). This is a custom implementation of what MFC now provides natively
via `CMFCPropertySheet` with `PropSheetLook_Tree`.

All PPg* files (PPgGeneral, PPgConnection, PPgDisplay, etc.) derive from
`CPropertyPage`.

**Action:** Evaluate replacing `CTreePropSheet` with `CMFCPropertySheet` configured
with `SetLook(CMFCPropertySheet::PropSheetLook_Tree)`. This would eliminate a
third-party dependency and gain native visual style support.

---

### WWMOD_013 - WSAAsyncSelect Networking Model

**Severity:** P1 historically, P3 for remaining follow-up
**Files:** historical `srchybrid/AsyncSocketEx.cpp`, removed `srchybrid/AsyncSocketExLayer.cpp`

This finding described the pre-migration networking stack, where `WSAAsyncSelect` pumped socket
events through a hidden helper window on the UI thread. That architecture has been removed in the
current branch: live TCP and UDP transport now sit on the shared `WSAPoll` backend, and the old
proxy/layer chain is gone.

**Modern alternatives:**
- `WSAPoll` (Win Vista+) for a poll-based bridge model; this is the current branch state
- I/O Completion Ports (`CreateIoCompletionPort`) for high-performance async I/O
- Registered I/O (RIO) for ultra-low-latency scenarios

**Action:** Treat the helper-window migration as done. The remaining networking action is
long-term IOCP consideration and operational soak/stress validation of the current `WSAPoll`
backend, not further `WSAAsyncSelect` removal.

---

### WWMOD_014 - MFC Synchronization Primitives (CCriticalSection)

**Severity:** P2
**Files:** 27 files, 58 occurrences of `CCriticalSection`/`CSingleLock`/`CEvent`/`CMutex`

MFC's `CCriticalSection` wraps Windows `CRITICAL_SECTION`, which is heavier than
SRW Locks (introduced in Vista). `CSingleLock`/`CMutex` add MFC overhead.

Key contended locks:
- `UploadBandwidthThrottler.h` - 6 occurrences (hot path)
- `SharedFileList.cpp` - 5 occurrences
- `UploadQueue.cpp/h` - 9 occurrences
- `UPnPImplMiniLib.cpp` - 5 occurrences

**Action:**
- Replace `CCriticalSection` + `CSingleLock` with `SRWLOCK` + `SRWLockExclusive`/
  `SRWLockShared` for read-heavy scenarios (shared file list, upload queue)
- Replace `CEvent` with `CONDITION_VARIABLE` where appropriate
- Use `std::mutex` / `std::shared_mutex` / `std::condition_variable` for new code

---

### WWMOD_015 - AfxBeginThread Instead of Modern Thread Pools

**Severity:** P2
**Files:** 21+ call sites across the codebase

All background work uses `AfxBeginThread` to create dedicated OS threads. Each
hashing job, file completion, archive scan, UPnP discovery, etc. spawns a new thread.

**Action:**
- For short-lived tasks (hashing, media info extraction, frame grabbing), use the
  Windows Thread Pool API (`CreateThreadpoolWork` / `SubmitThreadpoolWork`)
- For long-running background loops (bandwidth throttler, disk I/O, directory
  watcher), dedicated threads remain appropriate
- Consider `std::async` / `std::jthread` (C++20) for simpler cases

---

### WWMOD_016 - Custom Type Aliases Instead of `<cstdint>`

**Severity:** P2
**Files:** `srchybrid/types.h` (entire file)

```cpp
typedef unsigned char      uint8;
typedef signed char        sint8;
typedef unsigned short     uint16;
...
typedef unsigned __int64   uint64;
typedef signed __int64     sint64;
```

These replicate `<cstdint>` types (`uint8_t`, `int8_t`, ..., `uint64_t`, `int64_t`)
which have been standard since C++11.

**Action:** Replace with `<cstdint>` standard types across the codebase. The `uchar`
alias can remain if widely used.

---

### WWMOD_017 - MFC Container Classes Instead of STL

**Severity:** P1
**Files:** 164 files, 682 occurrences of `CArray`/`CList`/`CMap`/`CTypedPtrList`/
`CStringArray`/`CWordArray`/`CDWordArray`/`CByteArray`/`CObArray`/`CPtrArray`/
`CUIntArray`

MFC containers predate the STL and have significant disadvantages:
- No iterator support (range-for loops)
- No move semantics
- No emplace construction
- `CArray` uses `memcpy` for reallocation (unsafe for non-POD types)
- `CList` is a doubly-linked list with per-element allocation
- `CMap` uses a fixed-bucket hash table with poor collision handling
- No exception safety guarantees

**Action:** Migrate to STL equivalents:
- `CArray<T>` -> `std::vector<T>`
- `CList<T>` -> `std::vector<T>` or `std::list<T>` where list semantics are needed
- `CMap<K,V>` -> `std::unordered_map<K,V>`
- `CStringArray` -> `std::vector<CString>` or `std::vector<std::wstring>`
- `CByteArray` -> `std::vector<uint8_t>`
- `CTypedPtrList` -> `std::vector<std::unique_ptr<T>>`

This is one of the largest mechanical changes but dramatically improves code quality,
debuggability, and performance.

---

### WWMOD_018 - GDI-Only Drawing (No GDI+ / Direct2D)

**Severity:** P2
**Files:** Throughout UI code, especially `OScopeCtrl`, `BarShader`, `ProgressCtrlX`,
`DialogMinTrayBtn`, `TaskbarNotifier`, `StatisticsDlg`, `MuleListCtrl` (custom draw)

All graphical rendering uses raw GDI calls (`CDC`, `CPen`, `CBrush`, `BitBlt`,
`CreateCompatibleBitmap`, etc.). The delay-loaded `gdiplus.dll` in the linker settings
suggests GDI+ was intended but never adopted.

**Action:**
- For anti-aliased drawing (graphs, progress bars): use GDI+ (`Gdiplus::Graphics`)
- For high-performance, DPI-aware rendering: use Direct2D (`ID2D1RenderTarget`)
- At minimum: use GDI+ for the oscilloscope graphs and bar shaders where aliasing
  artifacts are most visible

---

### WWMOD_019 - Unsafe String Formatting Functions

**Severity:** P1
**Files:** 11 files, 40 occurrences of `sprintf`/`_stprintf`/`wsprintf`/`swprintf`/
`_snprintf`/`_sntprintf`

These functions have buffer overflow risks. `_snprintf` does not null-terminate on
truncation.

**Action:** Replace with:
- `StringCchPrintf` / `StringCbPrintf` (from `<strsafe.h>`)
- `CString::Format` (already widely used elsewhere in the codebase)
- `std::format` (C++20, if the toolset supports it)

---

### WWMOD_020 - MD4 as Primary File Hash

**Severity:** P0 (protocol constraint limits action)
**Files:** `srchybrid/MD4.cpp`, `srchybrid/MD4.h`, plus 38 files using MD4/MD5/SHA-1

MD4 (128-bit) is the ed2k file identification hash. It has been cryptographically
broken since 2004. However, it is a **protocol-level identifier** shared across the
entire ed2k/Kad network — changing it would break interoperability.

Current state:
- AICH (Advanced Intelligent Corruption Handling) uses SHA-1 as a secondary hash
  for corruption detection (better than MD4 but also deprecated for security)
- MD5 is used for some credential hashing

**Action:**
- **Short term:** Ensure MD4 is never used for security decisions (authentication,
  integrity verification of untrusted data). Use AICH/SHA-1 or better for all
  integrity checks.
- **Medium term:** Add SHA-256 as a parallel file identifier in Kad metadata;
  publish both MD4 and SHA-256 hashes
- **Long term:** Protocol extension proposal for SHA-256-based file identification

---

### WWMOD_021 - No C++ Language Standard Specified in Project

**Severity:** P1
**Files:** `srchybrid/emule.vcxproj`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

The `.vcxproj` file does not set `<LanguageStandard>`. With v143, this defaults to
C++14. The codebase could benefit from C++17 or C++20 features:
- `std::string_view`, `std::optional`, `std::variant` (C++17)
- `if constexpr`, structured bindings, `[[nodiscard]]` (C++17)
- `std::format`, `std::jthread`, `std::span` (C++20)
- `constexpr` improvements in both standards

**Action:** Add `<LanguageStandard>stdcpp20</LanguageStandard>` to the project.
Incrementally adopt modern language features.

Adjacent cleanup completed with this chunk:
- removed the legacy `_SpecialBootstrapNodes` build configuration
- removed `_BOOTSTRAPNODESDAT` codepaths and the special bootstrap-only `nodes.dat` format
- removed the startup splash screen feature and its dedicated source files while fixing the C++20 fallout

Adjacent cleanup landed with this change:
- removed the legacy `_SpecialBootstrapNodes` build configuration
- removed `_BOOTSTRAPNODESDAT` codepaths and the special bootstrap-only `nodes.dat` format
- kept the normal `nodes.dat` path and `nodes.fastkad.dat` sidecar flow

---

### WWMOD_022 - `#define` Constants Instead of `constexpr`

**Severity:** P2
**Files:** `srchybrid/Opcodes.h` (entire file, 100+ defines), plus scattered across
other headers

```cpp
#define PARTSIZE          9728000ui64
#define EMBLOCKSIZE       184320u
#define MAX_EMULE_FILE_SIZE  0x4000000000ui64
#define MAXCONPER5SEC     50
```

Macros have no type safety, no scope, and no debugger visibility.

**Action:** Convert to `constexpr` variables in a namespace:

```cpp
namespace emule {
    constexpr uint64_t PARTSIZE = 9'728'000;
    constexpr uint32_t EMBLOCKSIZE = 184'320;
    constexpr uint64_t MAX_FILE_SIZE = 0x40'0000'0000; // 256 GB
}
```

---

### WWMOD_023 - No `/permissive-` Conformance Mode

**Severity:** P2
**Files:** `srchybrid/emule.vcxproj`

The project does not enable `/permissive-` (strict C++ conformance). This allows
MSVC-specific non-standard code that may break with other compilers or future MSVC
versions.

**Action:** Add `/permissive-` to `<AdditionalOptions>`. Fix resulting compilation
errors (typically: two-phase lookup issues, implicit narrowing conversions).

---

## Category C - Archaic Limits & Approaches

### WWMOD_024 - MAX_PATH (260) Path Length Limit

**Severity:** P1
**Files:** 39 files, 156 occurrences of `MAX_PATH`

The codebase pervasively uses `TCHAR buf[MAX_PATH]` and `MAX_PATH`-sized buffers.
While the manifest declares `longPathAware=true`, the actual code still truncates
at 260 characters. Key hot spots:

- `Emule.cpp` - 14 occurrences (startup paths, config paths)
- `Ini2.cpp` - 20 occurrences (preference file I/O)
- `OtherFunctions.cpp` - 18 occurrences (utility functions)
- `PPgDirectories.cpp` - 6 occurrences (directory selection)
- `Preferences.cpp` - 5 occurrences

Note: `GUIDE-LONGPATHS.md` exists with 28 references, indicating this is a known
tracked issue.

**Action:** Replace `MAX_PATH`-sized static buffers with dynamically-sized
`CString` or `std::wstring`. Use `\\?\`-prefixed paths or `GetFinalPathNameByHandle`
for paths that may exceed 260 characters. See `GUIDE-LONGPATHS.md` for the detailed
migration plan.

---

### WWMOD_025 - IPv4-Only Networking

**Severity:** P1
**Files:** 14 files, 21+ occurrences of `sockaddr_in` (not `sockaddr_in6`/
`sockaddr_storage`)

The entire network stack assumes IPv4:
- IP addresses stored as `uint32` (4 bytes)
- `sockaddr_in` used exclusively
- `inet_addr()` / `inet_ntoa()` deprecated IPv4-only functions
- Kad routing tables keyed on 32-bit IPs
- Protocol opcodes transmit 4-byte IP fields
- Server list and peer exchange use 4-byte IPs

Key files: `AsyncSocketEx.cpp`, `Pinger.cpp`, `ServerSocket.cpp`, `UDPSocket.cpp`,
`KademliaUDPListener.cpp`, `ServerConnect.cpp`

**Action (phased):**
1. Internal: Use `sockaddr_storage` and `inet_pton`/`inet_ntop` for all socket
   operations
2. Local: Support IPv6 for server connections and HTTP downloads
3. Protocol: Design Kad extension tags for 128-bit peer addresses
4. Full: IPv6 peer-to-peer transfers (requires protocol extension)

---

### WWMOD_026 - 9.28 MB Fixed Part Size

**Severity:** P1 (protocol constraint limits action)
**Status:** Rejected.
**Files:** `srchybrid/Opcodes.h:101`

```cpp
#define PARTSIZE  9728000ui64  // ~9.28 MB
```

This was designed when files were typically 100-700 MB. For a 50 GB file, this
creates 5,381 parts, each requiring its own MD4 hash, AICH verification, source
tracking, and UI representation. Modern networks could use 64 MB or 256 MB parts.

Protocol constraint: changing PARTSIZE breaks interoperability with all existing
clients and hash databases.

**Disposition:** Rejected. Keep the current part size for ed2k protocol compatibility.

---

### WWMOD_027 - 256 GB Maximum File Size

**Severity:** P2
**Status:** Rejected.
**Files:** `srchybrid/Opcodes.h:102`

```cpp
#define MAX_EMULE_FILE_SIZE  0x4000000000ui64  // 256 GB
```

While this was a massive improvement over the original 4 GB limit, modern use cases
include Linux ISOs, game bundles, and datasets exceeding 256 GB. The limit is
artificial.

**Disposition:** Rejected. Keep the current maximum file size instead of widening the compatibility surface for this branch.

---

### WWMOD_028 - Conservative Connection and Queue Limits

**Severity:** P1
**Files:** `srchybrid/Opcodes.h:60-128`

| Constant | Current | Modern suggestion | Rationale |
|----------|---------|-------------------|-----------|
| `MAX_RESULTS` | 100 | 500-1000 | Search result limits from dial-up era |
| `MAX_SOURCES_FILE_SOFT` | 750 | 2000-5000 | Source tracking capacity |
| `MAX_UP_CLIENTS_ALLOWED` | 50 | 200 | Upload slot ceiling |
| `MAXCONPER5SEC` | 50 | 200 | Connection rate limiting |
| `CONNECTION_TIMEOUT` | 40s | 20s | Modern latency expectations |
| `DOWNLOADTIMEOUT` | 100s | 45s | Stale connection detection |
| `FILEREASKTIME` | 29min | 15min | Source reask frequency |
| `KADEMLIAMAXINDEX` | 50,000 | 500,000 | Kad keyword index capacity |
| `KADEMLIAMAXENTRIES` | 60,000 | 600,000 | Kad entry capacity |
| `KADEMLIAMAXSOURCEPERFILE` | 1,000 | 10,000 | Sources per file in Kad |
| `RSAKEYSIZE` | 384 bits | 2048 bits | RSA key size (384 is trivially breakable) |
| `MAX_EMULE_FILE_SIZE` | 256 GB | 2+ TB | Modern file sizes |
| `SESSIONMAXTRANS` | 64 GB | 256 GB+ | Per-session upload cap |
| `MAXFILECOMMENTLEN` | 128 chars | 1024 chars | User comment length |

See also: `FEATURE-MODERN-LIMITS.md` for tracked changes.

**Action:** Raise limits progressively. Some (like `RSAKEYSIZE`) require protocol
negotiation with peers. Others (like `KADEMLIAMAXINDEX`) are purely local.

---

### WWMOD_029 - 384-bit RSA Keys

**Severity:** P0
**Status:** Rejected.
**Files:** `srchybrid/Opcodes.h:95`

```cpp
#define RSAKEYSIZE  384  // 384 bits
```

A 384-bit RSA key can be factored in seconds on modern hardware. This is used for the
eMule credit system's client identity. Any peer can forge another peer's credits.

**Disposition:** Rejected. Keep the legacy RSA key size for protocol compatibility with the existing credit-system identity model.

---

### WWMOD_030 - Deprecated Winsock APIs

**Severity:** P1
**Files:** 14 files, 21+ call sites
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

Deprecated functions that were in use before this cleanup:
- `inet_addr()` - IPv4 only, no error reporting (returns `INADDR_NONE` on error,
  which is also a valid address `255.255.255.255`)
- `inet_ntoa()` - Uses thread-unsafe static buffer, IPv4 only
- `gethostbyname()` - No longer present in active code; only historical comments remained

**Action:** Replace with modern equivalents:
- `inet_addr()` -> `inet_pton(AF_INET, ...)`
- `inet_ntoa()` -> `inet_ntop(AF_INET, ...)`
- `gethostbyname()` -> `getaddrinfo()` (supports IPv6, async-safe)

---

### WWMOD_031 - `_T()` / `TCHAR` Dual-Path Macros

**Severity:** P3
**Files:** Throughout the entire codebase (thousands of occurrences)

With `CharacterSet=Unicode` set in the project, `_T("...")` expands to `L"..."` and
`TCHAR` expands to `wchar_t`. The dual-path ANSI/Unicode abstraction is unnecessary
when targeting Unicode-only Windows 10.

**Action:** This is a massive mechanical change with low practical benefit. Keep as-is
unless doing a large-scale modernization pass. If CString is retained (recommended for
MFC code), `_T()` is idiomatic and acceptable.

---

### WWMOD_032 - No ETW / Structured Logging

**Severity:** P2
**Files:** Throughout; current logging goes to `Log.cpp` -> rich edit control

Diagnostics use `TRACE` (debug only), `AddDebugLogLine` / `AddLogLine` (appends text
to a rich edit control in the UI), and `OutputDebugString`.

Problems:
- Logging to a UI control is expensive and thread-unsafe from non-UI threads
- No structured format (no timestamps, severity levels in machine-readable form)
- No way to capture logs from a running instance without the UI
- `TRACE` disappears in release builds

**Action:**
- Add a lightweight structured logging framework (even a simple file-based one)
- Consider ETW (Event Tracing for Windows) for zero-overhead diagnostic tracing
- Keep the UI log view as a consumer of the logging subsystem, not the primary sink

---

### WWMOD_033 - Binary File Formats with Limited Versioning

**Severity:** P2
**Files:** `srchybrid/Opcodes.h:33-39` (version defines), `KnownFile.cpp`,
`PartFile.cpp`, `Preferences.cpp`

```cpp
#define PREFFILE_VERSION         0x14
#define PARTFILE_VERSION         0xe0
#define PARTFILE_SPLITTEDVERSION 0xe1
#define PARTFILE_VERSION_LARGEFILE 0xe2
#define CREDITFILE_VERSION       0x12
```

`.met` files use a binary format with a single version byte. No schema, no forward
compatibility, no migration tooling. A version bump requires understanding the exact
byte layout.

**Action:**
- Add a format migration layer that can read old versions and write the current version
- Consider adding a parallel JSON/SQLite representation for new data
- Document the binary format specifications

---

### WWMOD_034 - No Memory-Mapped I/O for Large Files

**Severity:** P2
**Files:** `PartFile.cpp`, `KnownFile.cpp`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

File hashing, part verification, and AICH calculation used buffered file reads in the
main hashing paths. For multi-gigabyte files, memory-mapped I/O improves the heavy
sequential-read paths by:
- Leverage the OS page cache more efficiently
- Avoid double-buffering (app buffer + OS cache)
- Enable zero-copy hashing

**Action:** Use `CreateFileMapping` / `MapViewOfFile` for sequential-read-heavy
operations (hashing, verification). Particularly impactful for AICH tree computation
on large files. Shared regression coverage should verify exact byte-range replay for the
mapped reader.

---

### WWMOD_035 - `UpgradeFromVC71.props` Import

**Severity:** P3
**Files:** `srchybrid/emule.vcxproj:45`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

```xml
<Import Project="$(VCTargetsPath)Microsoft.CPP.UpgradeFromVC71.props" />
```

This is a migration shim from Visual Studio .NET 2003 (VC++ 7.1) from ~2003. It's
been carried forward through 20+ years of VS upgrades. On VS 2022 it is likely a
no-op.

**Action:** Remove the import. Verify the build still works (it will).

---

### WWMOD_036 - Static MFC Linking

**Severity:** P2
**Files:** `srchybrid/emule.vcxproj:37`

```xml
<UseOfMfc>Static</UseOfMfc>
```

Static MFC linking produces a larger executable (~5-10 MB increase) and means security
updates to MFC require recompiling. Dynamic MFC linking uses `mfc140u.dll` which gets
patched via Windows Update.

**Action:** Consider switching to `Dynamic` MFC linking. Requires distributing
the MFC DLLs via VC++ Redistributable (common practice). Trade-off: single-file
deployment vs. smaller binary + automatic MFC security patches.

---

### WWMOD_037 - No Address Sanitizer / Static Analysis in CI

**Severity:** P2
**Files:** `srchybrid/emule.vcxproj`

The project enables `/Wall` (EnableAllWarnings) which is good, but:
- No `/analyze` (MSVC static analyzer)
- No AddressSanitizer (`/fsanitize=address`)
- No `/sdl` (Security Development Lifecycle checks)
- `/GS` (buffer security check) status not explicitly set (defaults to on)

**Action:** Add to Debug configuration:
- `/analyze` for static analysis
- `/fsanitize=address` for runtime memory error detection
- `/sdl` for additional security checks
- Explicitly set `/GS` and `/guard:cf` (Control Flow Guard)

---

### WWMOD_038 - Delay-Loaded DLLs That Are Always Present

**Severity:** P3
**Files:** `srchybrid/emule.vcxproj:78,104,129`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

```xml
<DelayLoadDLLs>gdiplus.dll;msimg32.dll;oleacc.dll;ws2_32.dll</DelayLoadDLLs>
```

All four DLLs are always present on Windows 10:
- `gdiplus.dll` - ships with Windows since XP SP1
- `msimg32.dll` - ships with Windows since Windows 2000
- `oleacc.dll` - accessibility, ships with Windows since 2000
- `ws2_32.dll` - Winsock 2, always present

Delay loading adds complexity (import thunks, potential for delayed load failures)
with no benefit when the DLLs are guaranteed to exist.

**Action:** Remove from `DelayLoadDLLs`. Link normally. Remove `delayimp.lib` from
additional dependencies if no other delay-loaded DLLs remain.

---

### WWMOD_039 - `qedit.h` Bundled Header from DirectX 8.0 Era

**Severity:** P2
**Files:** `srchybrid/qedit.h` (very large file)

This is a bundled copy of the DirectShow `qedit.h` header, with comments referencing
DirectX 8.0 and Windows XP header merges. It defines `ISampleGrabber` and related
interfaces used for video frame grabbing.

Modern alternatives: Media Foundation (available since Win7, preferred on Win10+)
replaces DirectShow for media processing.

**Action:**
- If video frame grabbing is needed: migrate to Media Foundation's
  `IMFSourceReader` API
- If only used for thumbnail generation: consider using the Windows Shell
  `IThumbnailProvider` interface instead
- Remove the bundled `qedit.h` after migration

---

### WWMOD_040 - `ResizableLib` Third-Party Dependency

**Severity:** P3
**Files:** `srchybrid/emule.vcxproj:52` (references `eMule-ResizableLib`)

ResizableLib is a third-party MFC extension for making dialogs resizable. It was
essential in the Win2000/XP era when MFC had no built-in layout management. Modern
MFC (VS 2008+) provides `CMFCDynamicLayout` for automatic control repositioning
on resize.

**Action:** Evaluate replacing ResizableLib with `CMFCDynamicLayout`. This would
eliminate an external build dependency.

---

### WWMOD_041 - No Accessibility Support

**Severity:** P2
**Status:** Rejected.
**Files:** Throughout UI code

The application has no MSAA (Microsoft Active Accessibility) or UIA (UI Automation)
support. Custom-drawn controls, owner-draw list items, and custom tooltip
implementations are invisible to screen readers.

The delay-loaded `oleacc.dll` suggests some awareness of accessibility, but no
actual implementation.

**Disposition:** Rejected. No accessibility implementation work is planned for this branch.

---

### WWMOD_042 - `CAsyncSocketEx` Custom Socket Library

**Severity:** P1
**Files:** `srchybrid/AsyncSocketEx.cpp` (~1300 lines), `AsyncSocketEx.h`,
`AsyncSocketExLayer.cpp`, `AsyncSocketExLayer.h`

A heavily customized fork of a ~2002-era socket wrapper. Features:
- Custom hidden-window-based socket event dispatch
- Layered proxy support (SOCKS4/4a/5, HTTP CONNECT)
- Manual socket index management (fixed array of 4096 sockets)
- Window message-based async model

The 4096-socket limit (`#define WM_SOCKETEX_NOTIFY (WM_USER + 3)` with a fixed
notification window message range) is a hard ceiling.

**Action:** This is the core of the network stack and a major rewrite candidate.
Short-term: audit the 4096 socket limit. Long-term: replace with a modern async
I/O library or custom IOCP wrapper.

---

### WWMOD_043 - `Pinger.cpp` Raw ICMP Implementation

**Severity:** P2
**Files:** `srchybrid/Pinger.cpp`

Custom raw socket ICMP ping implementation. Comments reference NT4/Win98 behavior.
Windows provides `IcmpSendEcho2` / `IcmpSendEcho2Ex` APIs that handle ICMP properly,
including on systems where raw sockets require elevation.

**Action:** Replace raw socket ICMP with the `IcmpSendEcho2Ex` API. This works
without raw socket permissions and supports both IPv4 and IPv6 (via
`Icmp6SendEcho2`).

---

### WWMOD_044 - No HTTP/2 or Modern TLS for HTTP Downloads

**Severity:** P1
**Files:** `srchybrid/HttpDownloadDlg.cpp`, `srchybrid/HttpClientReqSocket.cpp`,
`srchybrid/URLClient.cpp`

HTTP downloads (server lists, IP filter updates, version checks) use a custom HTTP
client built on top of `CAsyncSocketEx`. This means:
- No HTTP/2 support
- Manual HTTP header parsing
- Custom chunked transfer decoding
- No connection pooling
- TLS handling via custom integration

Windows 10 provides `WinHTTP` (for service/background downloads) and `WinINet` (for
user-facing downloads) which handle HTTP/2, TLS 1.3, proxy auto-detection, certificate
validation, and compression natively.

**Action:** Replace custom HTTP client with `WinHTTP` (`WinHttpOpen` /
`WinHttpConnect` / `WinHttpSendRequest`). This eliminates thousands of lines of custom
HTTP parsing code and gains HTTP/2, automatic proxy, and modern TLS for free.

---

### WWMOD_045 - `CRichEditCtrl` for Log Window

**Severity:** P3
**Files:** `srchybrid/HTRichEditCtrl.cpp`, `srchybrid/HTRichEditCtrl.h`

The log window uses `CRichEditCtrl` (v5.0 per `_RICHEDIT_VER 0x0500` in stdafx.h)
with custom hyperlink detection, color formatting, and line limiting.

RichEdit 5.0 is fine on Win10, but the control becomes very slow with thousands of
log lines because it maintains full formatting state for every character.

**Action:** For a log viewer, consider:
- Virtual-mode text control (render only visible lines)
- A simple owner-draw list box (one line per entry)
- Limit the in-control line count more aggressively and keep the full log on disk

---

### WWMOD_046 - No Taskbar Progress Integration

**Severity:** P2
**Files:** `srchybrid/EmuleDlg.cpp`
**Status:** Stale in the current tree as of 2026-03-31.

Windows 7+ provides `ITaskbarList3` for showing download progress in the taskbar
button (green/yellow/red progress overlay). This is highly visible to users.

The current branch already implements taskbar progress handling in
`CemuleDlg::EnableTaskbarProgress`, `CemuleDlg::UpdateStatusBarProgress`, and
`CemuleDlg::OnTaskbarBtnCreated` via `ITaskbarList3::SetProgressValue` and
`SetProgressState`.

**Action:** Treat this audit item as stale and do not schedule implementation work for it.

---

### WWMOD_047 - No Windows Notification (Toast) Support

**Severity:** P3
**Files:** `srchybrid/TaskbarNotifier.cpp`

The application uses a custom popup window (`CTaskbarNotifier`) for tray
notifications. This is a ~600-line custom implementation that predates the Windows
toast notification system.

Windows 10+ provides the WinRT toast notification API (`ToastNotificationManager`)
which integrates with Action Center, supports buttons, images, and user
interaction.

**Action:** Add toast notification support via the WinRT API (accessible from
Win32 desktop apps via `Windows.UI.Notifications`). Keep the custom notifier as
a fallback for users who disable system notifications.

---

### WWMOD_048 - `VC_EXTRALEAN` / `WIN32_LEAN_AND_MEAN` Trade-offs

**Severity:** P3
**Files:** `srchybrid/stdafx.h:8-10`

```cpp
#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN
#endif
```

`VC_EXTRALEAN` excludes many Windows headers from MFC includes. This was important
for compilation speed in the 2000s. With modern precompiled headers and SSDs,
the compilation time savings are negligible, but the macro can hide APIs that
developers expect to be available.

**Action:** Keep as-is (harmless). Document which APIs it excludes in case of
mysterious "undeclared identifier" errors.

---

### WWMOD_049 - `OmitFramePointers` in Release

**Severity:** P2
**Files:** `srchybrid/emule.vcxproj:97`
**Status:** Fixed on 2026-03-31 in `v0.72a-broadband-dev`.

```xml
<OmitFramePointers>true</OmitFramePointers>
```

Omitting frame pointers makes crash dumps and profiling less useful. Modern optimizers
gain minimal benefit from this on x64 (the x64 ABI uses a different unwinding
mechanism). The tiny performance gain is not worth the debuggability loss.

**Action:** Set to `false` in Release configuration to improve crash dump quality.

---

### WWMOD_050 - Time Macros Using Integer Multiplication Overflow Risk

**Severity:** P2
**Files:** `srchybrid/Opcodes.h:49-56`

```cpp
#define SEC(sec)    (sec)
#define MIN2S(min)  ((min)*60)
#define HR2S(hr)    MIN2S((hr)*60)
#define DAY2S(day)  HR2S((day)*24)
#define SEC2MS(sec) ((sec)*1000)
```

These macros compose via multiplication and can overflow `int` for large values.
`DAY2MS(365)` would overflow 32-bit int. More critically, they have no type safety.

**Action:** Convert to `constexpr` functions with explicit types:

```cpp
constexpr int64_t Seconds(int64_t s) { return s; }
constexpr int64_t Minutes(int64_t m) { return m * 60; }
constexpr int64_t Hours(int64_t h) { return h * 3600; }
constexpr int64_t Days(int64_t d) { return d * 86400; }
constexpr int64_t ToMs(int64_t s) { return s * 1000; }
```

Or use `<chrono>` for type-safe time representation.

---

## Summary Statistics

| Category | Count | P0 | P1 | P2 | P3 |
|----------|------:|---:|---:|---:|---:|
| A - Dead Code | 9 | 0 | 2 | 6 | 1 |
| B - Upgrade Targets | 20 | 2 | 7 | 8 | 3 |
| C - Archaic Limits | 21 | 1 | 7 | 9 | 4 |
| **Total** | **50** | **3** | **16** | **23** | **8** |

### P0 Items (Act Now)
- **WWMOD_010** - No DPI awareness (blurry UI on modern displays)
- **WWMOD_020** - MD4 as security hash (protocol-constrained)
- **WWMOD_029** - 384-bit RSA keys (trivially breakable)

### P1 Items (High Impact)
- **WWMOD_006** - Dynamic loading of always-present APIs
- **WWMOD_007** - Winsock deprecation suppression
- **WWMOD_008** - CRT security suppression
- **WWMOD_011** - Virtual list mode for CListCtrl
- **WWMOD_013** - WSAAsyncSelect networking model
- **WWMOD_017** - MFC containers -> STL
- **WWMOD_019** - Unsafe string formatting
- **WWMOD_021** - No C++ language standard set
- **WWMOD_024** - MAX_PATH limits
- **WWMOD_025** - IPv4-only networking
- **WWMOD_026** - Fixed 9.28 MB part size
- **WWMOD_028** - Conservative connection limits
- **WWMOD_030** - Deprecated Winsock APIs
- **WWMOD_042** - CAsyncSocketEx 4096-socket limit
- **WWMOD_044** - Custom HTTP client (no HTTP/2)

---

## Related Documents

- `AUDIT-DEADCODE.md` — Dead code patterns and cleanup tracking
- `FEATURE-MODERN-LIMITS.md` — Limit modernization plan and progress
- `PLAN-MODERNIZATION-2026.md` — Full 12-month engineering roadmap
- `REFACTOR-TASKS.md` — Refactoring task tracking
- `GUIDE-LONGPATHS.md` — Long path migration guide
- `AUDIT-SECURITY.md` — Security audit findings
- `ARCH-THREADING.md` — Threading architecture documentation
- `ARCH-NETWORKING.md` — Networking architecture documentation
