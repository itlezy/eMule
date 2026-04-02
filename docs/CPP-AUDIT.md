# C++ Language & Safety Audit

**Branch:** `v0.72a-broadband-dev`
**Last updated:** 2026-04-02
**Scope:** `srchybrid/` — 199 `.cpp` + 234 `.h` files, ~177K LOC
**Standard:** C++17 (MSVC)

---

## Current Branch Status

Completed bounded hardening chunks already landed on this branch:

- **[DONE]** `CPP_025` targeted atomic migration for app state, UI refresh flags, upload I/O counters, and bandwidth-slot signaling (`fb87ad5`)
- **[DONE]** `CPP_033` packet parser and buffer bounds hardening for TCP, UDP callback, and download block parsing (`fe805ff`)
- **[DONE]** `CPP_037` client/socket lifetime teardown hardening with shared detach ordering (`54ab77a`)
- **[DONE]** `CPP_032` compression and inflate exception-safety hardening with RAII work buffers (`236dc67`)
- **[DONE]** `CPP_031` socket I/O result validation and failure propagation hardening (`151cb46`)
- **[DONE]** bounded `CPP_021` AICH sync worker concurrency hardening (`c3d0a47`)
- **[DONE]** bounded `CPP_022` upload queue retirement and disk-I/O lock-order hardening (`09b3808`)
- **[DONE]** bounded `CPP_024` worker-to-UI message safety hardening (`7123271`)
- **[DONE]** bounded `CPP_022` upload throttler queue lock-order hardening (`0e3221e`)
- **[DONE]** bounded `CPP_035` transient resource ownership hardening (`3ab87c7`)
- **[DONE]** bounded `CPP_036` parser and allocation null-guard hardening (`ac0844c`)
- **[DONE]** bounded `CPP_026` UDP and upload-disk lock-scope hardening (`4ba2304`)
- **[DONE]** bounded `CPP_021` / `CPP_023` / `CPP_026` / `CPP_028` EMSocket send-path hardening (`f7fc906`)
- **[DONE]** bounded `CPP_023` BaseClient friend/buddy snapshot hardening (`430af74`)
- **[DONE]** bounded `CPP_021` / `CPP_028` shared-file hashing and auto-rescan coordination hardening (`83e15b6`)
- **[DONE]** bounded `CPP_028` socket sleep-poll cleanup for UDP resend and callback drain (`ae3d1da`)
- **[DONE]** bounded `CPP_012` fixed-buffer formatting hardening (`165e809`)
- **[DONE]** bounded `CPP_032` / `CPP_035` credits, collection, and listen-socket ownership hardening (`6a58bf6`)
- **[DONE]** bounded `CPP_034` AICH hashset and part-selection numeric hardening (`bf0c827`)
- **[DONE]** bounded `CPP_028` / `CPP_032` / `CPP_035` AICH maintenance hardening (`b9579cf`)
- **[DONE]** bounded `CPP_035` / `CPP_036` / `CPP_038` client part-status ownership hardening (`c23f169`)
- **[DONE]** bounded `CPP_035` / `CPP_038` Win32 file-handle RAII hardening for overlapped reads and utility file opens (`26c21fc`)

The broader audit categories below remain open unless explicitly marked otherwise; completed chunks reduce the remaining surface but do not imply the entire category is closed.

---

## Table of Contents

- [1. Language Modernization](#1-language-modernization)
  - [CPP_001 — C-style Casts](#cpp_001--c-style-casts)
  - [CPP_002 — Raw Arrays and C Strings](#cpp_002--raw-arrays-and-c-strings)
  - [CPP_003 — Manual Memory Management](#cpp_003--manual-memory-management)
  - [CPP_004 — Missing const / constexpr](#cpp_004--missing-const--constexpr)
  - [CPP_005 — Range-based For Loops](#cpp_005--range-based-for-loops)
  - [CPP_006 — enum class Conversion](#cpp_006--enum-class-conversion)
  - [CPP_007 — NULL → nullptr](#cpp_007--null--nullptr)
  - [CPP_008 — auto Keyword Opportunities](#cpp_008--auto-keyword-opportunities)
  - [CPP_009 — Structured Bindings](#cpp_009--structured-bindings)
  - [CPP_010 — Init Statements in if/switch](#cpp_010--init-statements-in-ifswitch)
- [2. Standard Library Usage](#2-standard-library-usage)
  - [CPP_011 — MFC → std Container Migration](#cpp_011--mfc--std-container-migration)
  - [CPP_012 — Unsafe String Functions](#cpp_012--unsafe-string-functions)
  - [CPP_013 — Algorithm Opportunities](#cpp_013--algorithm-opportunities)
  - [CPP_014 — Time Handling (std::chrono)](#cpp_014--time-handling-stdchrono)
  - [CPP_015 — File I/O (std::filesystem)](#cpp_015--file-io-stdfilesystem)
  - [CPP_016 — Numeric Conversions](#cpp_016--numeric-conversions)
  - [CPP_017 — Random Number Generation](#cpp_017--random-number-generation)
  - [CPP_018 — std::optional / std::variant](#cpp_018--stdoptional--stdvariant)
  - [CPP_019 — std::string_view Opportunities](#cpp_019--stdstring_view-opportunities)
  - [CPP_020 — Missing Standard Headers](#cpp_020--missing-standard-headers)
- [3. Threading Risks & Improvements](#3-threading-risks--improvements)
  - [CPP_021 — Shared Mutable State Without Locks](#cpp_021--shared-mutable-state-without-locks)
  - [CPP_022 — Lock Ordering / Deadlock Risks](#cpp_022--lock-ordering--deadlock-risks)
  - [CPP_023 — TOCTOU Races](#cpp_023--toctou-races)
  - [CPP_024 — Thread-Unsafe MFC Calls](#cpp_024--thread-unsafe-mfc-calls)
  - [CPP_025 — Missing volatile / std::atomic](#cpp_025--missing-volatile--stdatomic)
  - [CPP_026 — CSingleLock Anti-Patterns](#cpp_026--csinglelock-anti-patterns)
  - [CPP_027 — Thread Inventory](#cpp_027--thread-inventory)
  - [CPP_028 — Busy-Wait / Sleep Polling](#cpp_028--busy-wait--sleep-polling)
  - [CPP_029 — Thread Pool Opportunities](#cpp_029--thread-pool-opportunities)
  - [CPP_030 — std::mutex Migration Candidates](#cpp_030--stdmutex-migration-candidates)
- [4. Error Handling & Safety](#4-error-handling--safety)
  - [CPP_031 — Unchecked Return Values](#cpp_031--unchecked-return-values)
  - [CPP_032 — Exception Safety](#cpp_032--exception-safety)
  - [CPP_033 — Buffer Overflows](#cpp_033--buffer-overflows)
  - [CPP_034 — Integer Overflow Risks](#cpp_034--integer-overflow-risks)
  - [CPP_035 — Resource Leaks](#cpp_035--resource-leaks)
  - [CPP_036 — Null Pointer Dereference Risks](#cpp_036--null-pointer-dereference-risks)
  - [CPP_037 — Use-After-Free Patterns](#cpp_037--use-after-free-patterns)
  - [CPP_038 — RAII Wrapper Opportunities](#cpp_038--raii-wrapper-opportunities)
  - [CPP_039 — noexcept Opportunities](#cpp_039--noexcept-opportunities)
  - [CPP_040 — [[nodiscard]] Candidates](#cpp_040--nodiscard-candidates)
- [Consolidated Item Index](#consolidated-item-index)
- [Summary Statistics](#summary-statistics)
- [Modernization Roadmap](#modernization-roadmap)

---

## 1. Language Modernization

### CPP_001 — C-style Casts

**Count:** 100+ occurrences
**Severity:** Medium
**Priority:** HIGH

C-style casts `(TYPE)expr` bypass type checking and hide intent.  Replace with `static_cast`, `reinterpret_cast`, or `const_cast` to make intent explicit and catch misuse at compile time.

| File | Line | Current | Recommended |
|---|---|---|---|
| AbstractFile.cpp | 417 | `(LPCTSTR)EncodeUrlUtf8(...)` | `static_cast<LPCTSTR>(...)` |
| AbstractFile.cpp | 419 | `(LPCTSTR)EncodeBase16(...)` | `static_cast<LPCTSTR>(...)` |
| BaseClient.cpp | 383 | `(LPCTSTR)md4str(...)` | `static_cast<LPCTSTR>(...)` |
| BaseClient.cpp | 584 | `(UINT)(data.GetLength() - data.GetPosition())` | `static_cast<UINT>(...)` |
| BaseClient.cpp | 759 | `(UINT)m_byEmuleVersion` | `static_cast<UINT>(...)` |
| BarShader.cpp | 171 | `(int)uPixels` | `static_cast<int>(...)` |
| BarShader.cpp | 220 | `(int)(fRed + .5f)` | `static_cast<int>(...)` |
| BuddyButton.cpp | 81 | `(HANDLE)lpfnOldWndProc` | `reinterpret_cast<HANDLE>(...)` |
| CaptchaGenerator.cpp | 193 | `(LONG)x`, `(LONG)y` | `static_cast<LONG>(...)` |
| ChatSelector.cpp | 157 | `(UINT)sizeof pf` | `static_cast<UINT>(...)` |

**Status:** Open

---

### CPP_002 — Raw Arrays and C Strings

**Count:** 50+ occurrences
**Severity:** Medium
**Priority:** HIGH

Stack-allocated `char`/`TCHAR` buffers risk overflow and cannot resize.  Replace with `std::array` (fixed), `std::vector` (dynamic), or `CString`/`std::wstring` (text).

| File | Line | Declaration |
|---|---|---|
| BaseClient.cpp | 1631 | `TCHAR szSoftware[128]` |
| BaseClient.cpp | 1877 | `uchar achBuffer[250]` |
| AddSourceDlg.cpp | 143–148 | `TCHAR szScheme[INTERNET_MAX_SCHEME_LENGTH]` + 5 more `TCHAR[]` |
| ButtonsTabCtrl.cpp | 51 | `TCHAR szLabel[256]` |
| CaptchaGenerator.cpp | 175 | `WCHAR wT[2] = { 0 }` |
| CatDialog.cpp | 145 | `TCHAR buffer[MAX_PATH]` |
| ClientCredits.cpp | 542 | `uchar pachSignature[200] = {}` |
| ClientUDPSocket.cpp | 234 | `uchar reqfilehash[MDX_DIGEST_SIZE]` |
| ClientUDPSocket.cpp | 385 | `uchar uchUserHash[MDX_DIGEST_SIZE]` |
| ClosableTabCtrl.cpp | 194 | `TCHAR szLabel[256]` |

**Status:** Open

---

### CPP_003 — Manual Memory Management

**Count:** 30+ `new`/`delete` pairs; 5+ `GlobalAlloc`/`GlobalFree` pairs
**Severity:** High
**Priority:** MEDIUM

Raw `new`/`delete` invites leaks on exception paths.  Replace with `std::unique_ptr`, `std::vector`, or RAII wrappers.

| File | Line | Pattern |
|---|---|---|
| BaseClient.cpp | 298 | `free(m_pszUsername)` |
| BaseClient.cpp | 301–304 | `delete[] m_abyPartStatus; delete[] m_abyUpPartStatus` |
| DownloadClient.cpp | 89 | `new char[m_nPartCount]{}` |
| DownloadClient.cpp | 467–470 | `delete[] m_abyPartStatus; m_abyPartStatus = new uint8[m_nPartCount]` |
| BarShader.cpp | 74 | `m_Modifiers = new float[count]` |
| AsyncSocketEx.cpp | 667 | `new char[nSockAddrLen]()` |
| ClientCredits.cpp | 252 | `new byte[count * sizeof(CreditStruct)]` |
| AICHSyncThread.cpp | 225 | `new BYTE[nHashCount * GetHashSize()]` |
| Collection.cpp | 53 | `new BYTE[m_nKeySize]` |
| Emule.cpp | 281 | `free((void*)m_pszProfileName)` |
| Emule.cpp | 701–708 | `GlobalAlloc` / `GlobalFree` for clipboard |

**Status:** Open

---

### CPP_004 — Missing const / constexpr

**Count:** 20+ `#define` constants; static const candidates
**Severity:** Low
**Priority:** MEDIUM

Numeric `#define` macros pollute the preprocessor namespace, have no type, and cannot participate in constant expressions.

| File | Line | Current | Recommended |
|---|---|---|---|
| CaptchaGenerator.cpp | 30–31 | `#define LETTERSIZE 32` / `#define CROWDEDSIZE 23` | `constexpr int kLetterSize = 32;` |
| ChatWnd.cpp | 37–40 | Splitter dimension `#define`s | `constexpr int k...` |
| ClientCredits.h | 21–25 | `#define MAXPUBKEYSIZE 80` etc. | `constexpr uint8_t kMaxPubKeySize = 80;` |
| BaseClient.cpp | 72 | `#define URLINDICATOR _T(...)` | `constexpr LPCTSTR kUrlIndicator = _T(...)` |
| EncryptedStreamSocket.cpp | 97–102 | Crypto constants | `constexpr ...` |
| DownloadQueue.cpp | 703–708 | Protocol constants | `constexpr ...` |
| EncryptedDatagramSocket.cpp | 136–144 | Crypto constants | `constexpr ...` |

**Status:** Open

---

### CPP_005 — Range-based For Loops

**Count:** 80+ `POSITION`-based MFC loops; 534 total `GetHeadPosition` sites
**Severity:** Low
**Priority:** HIGH (readability & safety)

MFC `POSITION`-based iteration is verbose, error-prone, and prevents standard algorithm use.

| File | Line | Current Pattern |
|---|---|---|
| AbstractFile.cpp | 63, 100 | `for (POSITION pos = list.GetHeadPosition(); pos != NULL;)` |
| ClientList.cpp | 76, 186, 235, 245, 255, 265, 275, 285, 296, 307, 321 | Same MFC pattern |
| DownloadClient.cpp | 90 | `for (POSITION pos = m_PendingBlocks_list.GetHeadPosition(); ...)` |
| DownloadQueue.cpp | 73 | Nested `POSITION` loops over file lists |
| BindAddressResolver.cpp | 44 | `std::vector<CString>::const_iterator` (verbose) |
| IP2Country.cpp | 549 | `std::map<uint32, std::string>::const_iterator itFound = ...` |
| kademlia/utils/FastKad.cpp | 58–59 | std::map iterator patterns |
| kademlia/utils/SafeKad.cpp | 95, 108, 121 | Multiple map iterations |

**Recommendation:** Provide an MFC-to-range adapter (thin `begin()`/`end()` overload for `CList`, `CArray`) or migrate to `std::list`/`std::vector`.

**Status:** Open

---

### CPP_006 — enum class Conversion

**Count:** 5 non-scoped enums
**Severity:** Low
**Priority:** LOW

| File | Line | Current |
|---|---|---|
| MediaInfo.cpp | 692 | `enum { MIMETYPE_PROBE_SIZE = 512 };` |
| ED2KLink.h | 28 | `typedef enum { ... } EED2KLinkType;` |
| HttpClientReqSocket.h | 21 | `typedef enum { ... };` |

**Status:** Open

---

### CPP_007 — NULL → nullptr

**Count:** 2,262 occurrences across all source files
**Severity:** Low
**Priority:** HIGH (mechanical, safe refactor)

| File | Count |
|---|---|
| PartFile.cpp | 93 |
| DownloadQueue.cpp | 66 |
| DownloadClient.cpp | 47 |
| BaseClient.cpp | 42 |
| ClientList.cpp | 42 |
| UploadQueue.cpp | 27 |
| Preferences.cpp | 24 |

**Status:** Open

---

### CPP_008 — auto Keyword Opportunities

**Count:** 40+ verbose declarations
**Severity:** Low
**Priority:** MEDIUM

| File | Line | Before | After |
|---|---|---|---|
| DownloadListCtrl.cpp | 286 | `ListItems::const_iterator ownerIt = m_ListItems.find(owner);` | `const auto ownerIt = ...` |
| IP2Country.cpp | 549 | `std::map<uint32, std::string>::const_iterator itFound = ...` | `const auto itFound = ...` |
| IP2Country.cpp | 574 | `std::map<std::string, MMDBValue>::const_iterator itFound = ...` | `const auto itFound = ...` |
| kademlia/utils/FastKad.cpp | 58 | `std::map<uint32, ResponseTimeEntry>::iterator itOldest = ...` | `auto itOldest = ...` |

**Status:** Open

---

### CPP_009 — Structured Bindings

**Count:** 3–5 candidates (limited by MFC `CPair` patterns)
**Severity:** Low
**Priority:** LOW

Applicable only after migrating MFC maps to `std::unordered_map` / `std::map`.

| File | Line | Candidate |
|---|---|---|
| CollectionCreateDialog.cpp | 154 | `CPair *pair` iteration → `auto [key, value]` |
| CollectionCreateDialog.cpp | 321 | `CKnownFilesMap::CPair` iteration |
| CollectionViewDialog.cpp | 126 | Map pair iteration |

**Status:** Open — blocked by CPP_011

---

### CPP_010 — Init Statements in if/switch

**Count:** 0 current uses
**Severity:** Low
**Priority:** LOW

C++17 `if (auto x = expr; cond)` not used anywhere.  Opportunities exist at check-then-act sites (e.g., `auto pos = list.Find(x); if (pos != NULL)`).

**Status:** Open

---

## 2. Standard Library Usage

### CPP_011 — MFC → std Container Migration

**Count:** 159 MFC container declarations (70 `CTypedPtrList`, 89 `CArray`, many `CMap`)
**Severity:** Medium
**Priority:** HIGH

MFC containers cannot be used with standard algorithms, range-for, or move semantics.

| File | Line | Current | Recommended |
|---|---|---|---|
| ClientList.h | 27 | `CTypedPtrList<CPtrList, CUpDownClient*>` | `std::list<CUpDownClient*>` |
| DownloadQueue.h | 190–191 | Two `CTypedPtrList` for file lists | `std::list<CPartFile*>` |
| SearchList.h | 123–127 | Mixed `CTypedPtrList` and `CMap` | `std::vector` + `std::unordered_map` |
| PartFile.h | 364–370 | Gap lists + frequency arrays | `std::list<Gap_Struct>` + `std::vector<uint16>` |
| EMSocket.h | 118–121 | Buffer arrays + packet queues | `std::vector<WSABUF>` + `std::list<Packet*>` |
| AbstractFile.h | 93, 126 | Tag arrays | `std::vector<CTag*>` |
| SharedFileList.h | 137–138 | Hash waiting lists | `std::list<UnknownFile_Struct*>` |
| KnownFile.h | 138 | `CArray<uint16, uint16>` | `std::vector<uint16>` |
| ClientCredits.h | 129 | `CMap<CCKey, ..., CClientCredits*>` | `std::unordered_map<CCKey, CClientCredits*>` |
| Preferences.h | 1252 | `CArray<Category_Struct*>` | `std::vector<Category_Struct*>` |

**Status:** Open

---

### CPP_012 — Unsafe String Functions

**Count:** 7+ remaining raw copy / fixed-buffer string sites
**Severity:** High
**Priority:** HIGH

| File | Line | Function | Risk |
|---|---|---|---|
| CustomAutoComplete.cpp | 231 | `wcscpy(rgelt[i], ...)` | Raw copy without destination bound |
| Emule.cpp | 705 | `_tcscpy(pGlobalT, strText)` | Clipboard buffer copy relies on caller sizing |
| EmuleDlg.cpp | 3341 | `wcscpy(m_thbButtons[i].szTip, tooltip)` | Tooltip buffer copy without explicit bound |
| GradientStatic.cpp | 181 | `_tcscpy(lfFont.lfFaceName, _T("Arial"))` | Raw fixed-buffer copy into `LOGFONT` face name |
| OtherFunctions.cpp | 3661 | `_tcscpy(path, tchBuffer)` | Path copy without explicit destination bound |
| PartFile.cpp | 2831 | `_tcscpy(newfilename, ...)` | Raw filename copy into duplicated buffer |

**Recommendation:** Continue replacing raw copy/formatting sites with `CString`, `_tcscpy_s`, or equivalent bounded helpers. The fixed-buffer `_stprintf` / `sprintf` endpoint and UPnP port conversions are already addressed.

**Status:** **[PARTIAL]**

---

### CPP_013 — Algorithm Opportunities

**Count:** 534 `POSITION`-based iteration loops; many linear searches
**Severity:** Medium
**Priority:** HIGH

Hand-written loops that duplicate `<algorithm>` functionality.

| File | Line | Current | Recommended |
|---|---|---|---|
| ClientList.cpp | 162 | `POSITION pos = list.Find(toremove)` | `std::find(list.begin(), list.end(), toremove)` |
| UploadQueue.cpp | 150–161 | Loop to find best client | `std::max_element` with comparator |
| SearchList.cpp | 75, 85, 98 | List iteration with filter | `std::find_if` with lambda |
| DownloadClient.cpp | 584 | `m_OtherNoNeeded_list.Find(file)` | `std::any_of(...)` |
| OtherFunctions.h | 189 | `HeapSort(CArray<uint16>&, ...)` | `std::sort(v.begin()+first, v.begin()+last+1)` |
| ClientList.cpp | — | Min/max over client list | `std::min_element` / `std::max_element` |
| UploadQueue.cpp | 75 | `max(1ui64, ...)` | `std::max(...)` |

**Status:** Open

---

### CPP_014 — Time Handling (std::chrono)

**Count:** 191 `GetTickCount()`; 106 `time(NULL)` / `CTime` occurrences
**Severity:** Medium
**Priority:** MEDIUM

`GetTickCount()` wraps at 49.7 days and returns `DWORD` (no type safety).  `time(NULL)` returns `time_t` with no duration semantics.

| File | Occurrences | Pattern |
|---|---|---|
| ClientList.cpp | 14 | `::GetTickCount()` for timeout arithmetic |
| DownloadClient.cpp | 14 | Same |
| UploadQueue.cpp | 12 | Same |
| ListenSocket.cpp | 6 | Same |
| PartFile.cpp | 5 | `time(NULL)`, `CTime::GetCurrentTime()` |

**Recommendation:** Migrate to `std::chrono::steady_clock` for elapsed time, `std::chrono::system_clock` for wall clock.

**Status:** Open

---

### CPP_015 — File I/O (std::filesystem)

**Count:** 50+ `CFile` / `CreateFile` / `fopen` sites
**Severity:** Low
**Priority:** MEDIUM

| File | Line | Current |
|---|---|---|
| AICHSyncThread.cpp | 84 | `CFile::modeReadWrite \| CFile::modeCreate` |
| ClientCredits.cpp | 179 | `::CreateFile(strBakFileName, ...)` |
| kademlia/io/DataIO.cpp | 423, 427, 448 | `fopen("wclwrtab_gen.txt", "wb")` (hardcoded) |
| DownloadQueue.cpp | 1667 | `CFile::modeCreate \| CFile::modeWrite \| CFile::typeBinary` |
| KnownFile.cpp | 1030 | `CreateHash(CFile *pFile, ...)` |

**Recommendation:** Use `std::filesystem::path` for path manipulation; consider `std::fstream` for new code.

**Status:** Open

---

### CPP_016 — Numeric Conversions

**Count:** 47 `atoi` / `_ttoi` / `strtoul` calls
**Severity:** Medium
**Priority:** LOW

`atoi` returns 0 on both valid "0" and invalid input — no error indication.

| File | Count | Context |
|---|---|---|
| MediaInfo_DLL.cpp | 17 | Media tag parsing |
| Ini2.cpp | 8 | INI configuration parsing |
| CreditsThread.cpp | 5 | Credit data parsing |
| AddSourceDlg.cpp | 2 | URL parsing |

**Recommendation:** Replace with `std::from_chars` (C++17) for `char` data, or `_tcstoul` with error checking for `TCHAR` data.

**Status:** Open

---

### CPP_017 — Random Number Generation

**Count:** 40 `rand()` / `srand()` calls
**Severity:** High
**Priority:** MEDIUM

`rand()` is not thread-safe (global state), has poor statistical properties, and produces a maximum of `RAND_MAX` (typically 32,767).

| File | Count | Usage |
|---|---|---|
| OtherFunctions.cpp | 7 | General randomness |
| CaptchaGenerator.cpp | 5 | Visual challenge generation |
| PartFile.cpp | 4 | Source selection |
| BaseClient.cpp | 2 | Protocol randomization |
| Preferences.cpp | 1 | `srand(seed)` initialization |

**Recommendation:** Replace with `std::mt19937` + `std::uniform_int_distribution`.  Use `std::random_device` for seeding.

**Status:** Open

---

### CPP_018 — std::optional / std::variant

**Count:** 20+ sentinel-value return sites
**Severity:** Low
**Priority:** MEDIUM

Functions returning `-1` or `NULL` for "not found" lose type safety.

| File | Line | Pattern | Recommended |
|---|---|---|---|
| DownloadQueue.cpp | 233, 251 | Returns `-1` for not-found category | `std::optional<int>` |
| DownloadQueue.cpp | 439, 449 | Returns `NULL` for not-found file | `std::optional<CPartFile*>` |
| ClientList.cpp | 162 | `POSITION pos = list.Find(...)` → `NULL` | `std::optional<iterator>` |
| Packets.h/cpp | — | Multiple packet types with manual dispatch | `std::variant<Standard, Control, Split>` |

**Status:** Open

---

### CPP_019 — std::string_view Opportunities

**Count:** 100+ `LPCTSTR` and `const CString&` parameters
**Severity:** Low
**Priority:** MEDIUM

Many read-only string parameters force unnecessary `CString` construction or `TCHAR*` decay.

| File | Line | Current | Recommended |
|---|---|---|---|
| Preferences.cpp | 626 | `LPCTSTR GetConfigFile()` | `const CString& GetConfigFile()` |
| Preferences.cpp | 631 | `LPCTSTR const sFile` parameter | `std::wstring_view sFile` |
| KnownFile.cpp | 426 | `SetFileName(LPCTSTR pszFileName)` | `SetFileName(std::wstring_view)` |
| KnownFile.cpp | 449 | `CreateFromFile(LPCTSTR dir, LPCTSTR file)` | `std::wstring_view` params |
| ClientList.cpp | 160 | `RemoveClient(..., LPCTSTR pszReason)` | `std::wstring_view reason` |

**Status:** Open

---

### CPP_020 — Missing Standard Headers

**Severity:** Low
**Priority:** LOW

Core files lack `<algorithm>`, `<numeric>`, `<functional>` includes, leading to hand-rolled min/max/sort/accumulate.

| Missing Header | Hand-rolled Usage |
|---|---|
| `<algorithm>` | `HeapSort()` in OtherFunctions.h; linear searches everywhere |
| `<numeric>` | Manual sum accumulation in statistics |
| `<functional>` | No custom comparators with `std::sort` |

**Status:** Open

---

## 3. Threading Risks & Improvements

### CPP_021 — Shared Mutable State Without Locks

**Count:** 12+ unprotected globals
**Severity:** Critical
**Priority:** CRITICAL

| Global | File | Line | Issue |
|---|---|---|---|
| `theApp.downloadqueue` | AICHSyncThread.cpp | 44–48 | Iterator over filelist without lock held across full loop |
| `theApp.sharedfiles` | AICHSyncThread.cpp | 137–141 | `CSingleLock` during iteration, but released between accesses |
| `theApp.knownfiles` | AICHSyncThread.cpp | 212 | Accessed without holding appropriate lock |
| `theApp.emuledlg` | AICHSyncThread.cpp | 307–308 | Null check → method call without re-check (race window) |
| `theApp.clientcredits` | BaseClient.cpp | 627, 738, 983 | Multiple accesses without synchronization |
| `theApp.clientlist->GetBuddy()` | BaseClient.cpp | 939, 972 | Pointer dereference after null-check without re-check |
| `theApp.uploadqueue` | UploadDiskIOThread.cpp | 96, 115 | Accessed from I/O thread |
| `theApp.m_app_state` | Emule.h | 118 | Non-volatile enum read from many threads |
| `thePrefs` | AICHSyncThread.cpp | 83, 185, 212 | Reads from worker threads without lock |
| `thePrefs` | BaseClient.cpp | 612, 631, 656, 689 | Reads from network threads |
| File-scope statics | UploadQueue.cpp | 54–56 | `i1sec, i2sec, i5sec, i60sec` accessed from timer callbacks |
| `theApp.IsClosing()` | AICHSyncThread.cpp | 70, 139, 294 | Shutdown flag polled without lock |

**Status:** **[PARTIAL]**

---

### CPP_022 — Lock Ordering / Deadlock Risks

**Count:** 10+ potential deadlock sites
**Severity:** Critical
**Priority:** HIGH

No documented lock hierarchy exists.  Multiple functions acquire locks in inconsistent order.

| Lock A | Lock B | Files | Risk |
|---|---|---|---|
| `m_csBlockListsLock` | `m_csUploadListMainThrd…` | UploadQueue.cpp:456 vs 768 | Inverted order across functions |
| `theApp.hashing_mut` | shared file list internal | SharedFileList.cpp:407, 480 | Nested acquisition |
| `sendLocker` (EMSocket) | — | EMSocket.cpp:421, 437 | Re-entrant Lock/Unlock in single function |
| `queueLocker` | `tempQueueLocker` | UploadBandwidthThrottler.cpp:500, 625 | Both acquired simultaneously |
| `m_lockFlushList` | thread state | PartFileWriteThread.cpp:82–86 | Race with thread shutdown |
| `m_queueLock` | — | Emule.cpp:1422, 1440, 1461 | Multiple places, inconsistent ordering |
| `CGDIThread::m_csGDILock` | — | CreditsThread.cpp:118, 128 | Worker thread vs. main thread contention |

**Recommendation:** Document a strict lock-ordering DAG.  Introduce `std::scoped_lock` for multi-lock acquisition.

**Status:** **[PARTIAL]**

---

### CPP_023 — TOCTOU Races

**Count:** 12+ check-then-act patterns
**Severity:** High
**Priority:** HIGH

| File | Line | Pattern |
|---|---|---|
| UploadQueue.cpp | 156–210 | `GetCount()` outside lock → iterate elements |
| AICHSyncThread.cpp | 298–301 | `GetHashingCount() != 0` → `Sleep(100)` → recheck |
| AICHSyncThread.cpp | 307–308 | `m_hWnd != NULL` → `PostMessage` (window could destroy) |
| Indexed.cpp | 269 | `InterlockedExchange(&cleaning, 1)` → cleanup without lock |
| PartFile.cpp | 4404, 4415 | `InterlockedExchange` on `m_nPendingDisplayUpdate` |
| DownloadQueue.cpp | 73–77 | `POSITION` iteration without lock across loop body |
| SharedFileList.cpp | 620 | `InterlockedCompareExchange(&m_lAutoRescanDirty, ...)` → rescan |
| BaseClient.cpp | 656 | `SearchFriend()` → dereference without maintained lock |
| BaseClient.cpp | 939, 972 | `GetBuddy() != NULL` → dereference (buddy could disconnect) |
| AsyncDatagramSocket.cpp | 60–61 | `emuledlg != NULL && GetSafeHwnd() != NULL` → `PostMessage` |
| DownloadQueue.cpp | 87–88 | `GetTempDirCount()` → iterate with that count |

**Status:** **[PARTIAL]**

---

### CPP_024 — Thread-Unsafe MFC Calls

**Count:** 10+ sites
**Severity:** High
**Priority:** MEDIUM

MFC window operations (SendMessage, Invalidate, SetItemText) must only be called from the thread that created the window.

| File | Line | Thread | Call |
|---|---|---|---|
| AICHSyncThread.cpp | 306–308 | Hash sync thread | `SetAICHHashing()`, `ShowFilesCount()` on UI control |
| AICHSyncThread.cpp | 318–320 | Hash sync thread | Same |
| AICHSyncThread.cpp | 291 | Hash sync thread | `QueueLogLine()` → may PostMessage |
| AsyncDatagramSocket.cpp | 61 | UDP thread | `PostMessage` to main window |
| DownloadClient.cpp | 1432 | Socket thread | `PostMessage(UM_CLIENT_DISPLAY_UPDATE)` |
| KnownFile.cpp | 550, 578 | Hash thread | `PostMessage(TM_FILEOPPROGRESS)` |
| HttpDownloadDlg.cpp | 420, 443, 477 | Download thread | `PostMessage` without existence check |

**Recommendation:** All worker → UI communication should use `PostMessage` with pre-validated `HWND`, never `SendMessage` or direct method calls.

**Status:** **[PARTIAL]**

---

### CPP_025 — Missing volatile / std::atomic

**Count:** 6+ old-style `volatile LONG`; 6+ plain booleans used for signaling
**Severity:** High
**Priority:** HIGH

`volatile` alone does not guarantee atomicity on multi-core CPUs.  Use `std::atomic<>`.

| File | Line | Current | Recommended |
|---|---|---|---|
| Emule.h | 118 | `AppState m_app_state;` (plain enum) | `std::atomic<AppState>` |
| DownloadQueue.h | 79 | `bool m_bStopping;` | `std::atomic<bool>` |
| Search.h | 134 | `bool m_bStoping;` (typo + no atomic) | `std::atomic<bool> m_bStopping;` |
| PartFile.h | 393 | `volatile LONG m_nPendingDisplayUpdate;` | `std::atomic<int>` |
| SharedFileList.h | 149 | `volatile LONG m_lAutoRescanDirty;` | `std::atomic<long>` |
| UploadQueue.h | 42 | `volatile LONG m_nPendingIOBlocks;` | `std::atomic<int>` |
| UpdownClient.h | 646 | `volatile LONG m_nPendingDisplayUpdateMask;` | `std::atomic<int>` |
| UploadBandwidthThrottler.h | 80 | `volatile LONG m_needsMoreBandwidthSlots;` | `std::atomic<long>` |

**Positive:** `AsyncDatagramSocket.h:45–48` and `AsyncSocketEx.cpp:42–45` already use `std::atomic` — partial migration started.

**Status:** **[PARTIAL]**

---

### CPP_026 — CSingleLock Anti-Patterns

**Count:** 2+ remaining problematic patterns
**Severity:** Medium
**Priority:** MEDIUM

| Pattern | File | Line | Issue |
|---|---|---|---|
| Lock scope too narrow | DownloadQueue.cpp | 47–51 | `GetCount()` checked without lock, elements accessed without lock |
| Remaining ad hoc lock APIs outside hardened networking paths | Various secondary sites | — | Manual or weakly-scoped lock sequencing still exists outside the already-refactored UDP, upload-disk, and EMSocket send paths |

**Recommendation:** Keep converting remaining callsites to structured `CSingleLock` scopes or explicit policy helpers. The major networking and upload-disk sites are already addressed.

**Status:** **[PARTIAL]**

---

### CPP_027 — Thread Inventory

**Severity:** Info
**Priority:** Reference

| Thread | Creation | State Accessed | Sync Mechanism |
|---|---|---|---|
| AICHSyncThread | `IMPLEMENT_DYNCREATE` | `theApp.downloadqueue`, `sharedfiles`, `knownfiles` | `CSingleLock` / `hashing_mut` |
| UploadDiskIOThread | `AfxBeginThread(RunProc)` | `theApp.uploadqueue`, IOCP | `GetQueuedCompletionStatus` |
| PartFileWriteThread | `AfxBeginThread(RunProc)` | Part file buffers | IOCP + `m_lockFlushList` |
| PartFileConvert | Implied | `theApp.downloadqueue` | Unclear |
| HttpDownloadDlg | Per-download thread | Posts to main window | `PostMessage` |
| CreditsThread | `AfxBeginThread` | `CGDIThread::m_csGDILock` | `WaitForSingleObject` |
| GDIThread | `AfxBeginThread` | GDI resources | Event handles |
| FileInfoDialog | Per-file thread | Posts results via `SendMessage` | `SendMessage` (**risk**) |
| Kademlia Indexed | Cleanup thread | `InterlockedExchange` | Interlocked ops |
| PipeApiServer | Named pipe thread | Completion events | `WaitForSingleObject` |
| UPnPImplMiniLib | Discovery thread | UPnP state | Unknown |
| DownloadQueue resolver | `std::thread` (**modern**) | `std::deque` + `std::mutex` | `std::condition_variable` |

**Status:** **[DONE]**

---

### CPP_028 — Busy-Wait / Sleep Polling

**Count:** 4 remaining polling loops
**Severity:** Medium
**Priority:** LOW

| File | Line | Current | Recommended |
|---|---|---|---|
| ClientUDPSocket.cpp | 466 | `Sleep(20)` in DNS resolution loop | Event-based notification |
| SharedFileList.cpp | 431 | `Sleep(100)` "give time to write" | Completion event |
| EMSocket.cpp | 1079 | `Sleep(20)` for send buffer space | Throttler event |

**Positive:** `UploadDiskIOThread` and `PartFileWriteThread` already use `GetQueuedCompletionStatus(INFINITE)` — correct event-based wait.

**Status:** **[PARTIAL]**

---

### CPP_029 — Thread Pool Opportunities

**Count:** 4 short-lived thread patterns
**Severity:** Low
**Priority:** LOW

| Thread | File | Frequency | Recommendation |
|---|---|---|---|
| HTTP download | HttpDownloadDlg.cpp | 5–50/session | Reusable worker pool |
| Part file convert | PartFileConvert.cpp | 1–10/session | Single conversion worker |
| Media info extraction | FileInfoDialog.cpp | 100+/batch | 2–4 worker pool |
| UPnP discovery | UPnPImplMiniLib.cpp | 1–3/session | Single UPnP worker |

**Positive:** DownloadQueue hostname resolver already uses `std::thread` with queue — good pattern to replicate.

**Status:** **[PARTIAL]**

---

### CPP_030 — std::mutex Migration Candidates

**Count:** 5 highest-impact migrations
**Severity:** Medium
**Priority:** MEDIUM

| Current | File | Recommended | Benefit |
|---|---|---|---|
| `CCriticalSection m_csUploadList…` | UploadQueue.h:152 | `std::shared_mutex` | Concurrent readers (DiskIO thread) |
| `theApp.hashing_mut` | SharedFileList.cpp:480 | `std::mutex` + `std::lock_guard` | Exception safety |
| Manual Lock/Unlock on flush | PartFileWriteThread.cpp:82 | `std::mutex` + `std::scoped_lock` | Automatic unlock on exception |
| `sendLocker` (8 sites) | EMSocket.cpp:421 | `std::mutex` + `std::lock_guard` | Prevents unlock omission |
| `m_queueLock` | Emule.cpp:1422 | `std::mutex` + `std::lock_guard` | Exception-safe logging |

**Migration order:** Start with `m_queueLock` (lowest risk, most isolated), then `sendLocker` (highest contention), then `hashing_mut`, then `flush`, finally `uploadList` (requires `shared_mutex`).

**Status:** **[PARTIAL]**

---

## 4. Error Handling & Safety

### CPP_031 — Unchecked Return Values

**Count:** 12+ critical sites
**Severity:** High
**Priority:** HIGH

| File | Line | Call | Issue |
|---|---|---|---|
| EMSocket.cpp | 602 | `CEncryptedStreamSocket::Send(...)` | `sent` counter updated without validating result |
| EMSocket.cpp | 250 | `Receive(GlobalReadBuffer + ...)` | Checked for `SOCKET_ERROR` only |
| AsyncSocketEx.cpp | 160 | `WSAPoll(...)` | Error details not captured on failure |
| ClientUDPSocket.cpp | 78 | `ReceiveFrom(buffer, ...)` | Checked for `SOCKET_ERROR`, relies on caller |
| SafeFile.cpp | 253 | `CFile::Write(lpBuf, nCount)` | Return value not checked |
| Packets.cpp | 114 | `new char[size + 10]` | Can throw `bad_alloc`, not handled |
| Packets.cpp | 237 | `new BYTE[nNewSize]` | Same — in loop |
| DownloadClient.cpp | 1050 | `new BYTE[lenUnzipped]` | `lenUnzipped` not validated against limits |
| EMSocket.cpp | 706 | `new CHAR[pCurBuf.len]` | `pCurBuf.len` never validated |
| PartFile.cpp | 397 | `m_hpartfile.Open(...)` | Failure detected but execution continues |

**Status:** **[PARTIAL]**

---

### CPP_032 — Exception Safety

**Count:** 5+ remaining resource/exception sites; several high-risk packet, AICH maintenance, and credits/collection paths already hardened
**Severity:** High
**Priority:** HIGH

**Resource leaks on exception:**

| File | Line | Pattern |
|---|---|---|
| EMSocket.cpp | 363 | `new char[nPacketBufferSize]` | Allocation still relies on surrounding caller cleanup and exception propagation |

**Silent exception swallowing (`catch(...)`):**

| File | Line | Pattern |
|---|---|---|
| BaseClient.cpp | 2289 | `catch (...) { ASSERT(0); }` — silent in release |
| DownloadClient.cpp | 1293 | `catch (...) { ASSERT(0); }` — exception discarded |
| Collection.cpp | 267 | Text-write path still uses `catch (...) { ASSERT(0); }` |
| KnownFile.cpp | 1438 | `catch (...)` remains in a hash/metadata path |

**Status:** **[PARTIAL]**

---

### CPP_033 — Buffer Overflows

**Count:** 11+ risky sites
**Severity:** Critical
**Priority:** CRITICAL

| File | Line | Pattern | Risk |
|---|---|---|---|
| EMSocket.cpp | 268 | `memcpy(GlobalReadBuffer, pendingHeader, pendingHeaderSize)` | Size validated only by prior math |
| EMSocket.cpp | 341 | `memcpy(&pendingPacket->pBuffer[pendingPacketSize], rptr, toCopy)` | Assumes `pendingPacketSize` valid |
| EMSocket.cpp | 369 | `memcpy(pendingHeader, rptr, pendingHeaderSize)` | Could exceed `PACKET_HEADER_SIZE` |
| ClientUDPSocket.cpp | 220 | `memcpy(response->pBuffer, packet + 10, nCallbackPayloadSize)` | `size > buffer` not validated first |
| Packets.cpp | 117, 147, 173, 191 | Series of `memcpy` with calculated sizes | Garbage `size` → overflow |
| DownloadClient.cpp | 93 | `pcNextPendingBlks[uPart] = 1` | `uPart` not checked < `m_nPartCount` |
| DownloadClient.cpp | 543–544 | `m_abyPartStatus[done] = ...` | Assumes `m_nPartCount` correct |
| Packets.cpp | 303–305 | `*(uint16*)&m_lpBuffer[m_nPosition]` | Unaligned pointer cast |
| DownloadClient.cpp | 937–942 | `*(uint32*)&packet[16...]` | No alignment/bounds checks |

**Status:** **[PARTIAL]**

---

### CPP_034 — Integer Overflow Risks

**Count:** 6+ remaining sites
**Severity:** High
**Priority:** HIGH

| File | Line | Pattern | Risk |
|---|---|---|---|
| Packets.cpp | 432 | `m_nBlobSize((uint32)nSize)` | `size_t` → `uint32` truncation |
| Packets.cpp | 678, 685, 736 | `WriteUInt16((uint16)... )` | `size_t` / tag length truncation to 16-bit wire fields |
| FileIdentifier.cpp | 241, 390 | `WriteUInt16((uint16)uParts)` | Part-count truncation if unbounded input reaches serialization |
| DownloadClient.cpp | 481 | `new char[m_nPartCount + 1]` | `UINT_MAX` overflow in debug/status rendering path |
| EMSocket.cpp | 932 | `(uint32)(sizeleft - decval + 1)` | Signed subtraction to unsigned cast |
| SafeFile.cpp | 178, 204 | `WriteUInt16((uint16)uLen)` | String-length truncation to 16-bit serialized size |

**Status:** **[PARTIAL]**

---

### CPP_035 — Resource Leaks

**Count:** 1+ remaining leak / manual-lifetime sites
**Severity:** High
**Priority:** HIGH

| Resource | File | Line | Issue |
|---|---|---|---|
| `new` | EMSocket.cpp | 363 | Pending packet buffer ownership is still manual and coupled to surrounding queue cleanup |

**Status:** **[PARTIAL]**

---

### CPP_036 — Null Pointer Dereference Risks

**Count:** 5+ remaining sites
**Severity:** High
**Priority:** MEDIUM

| File | Line | Pattern |
|---|---|---|
| PartFile.cpp | 88 | `m_kadNotes.GetNext(pos)` — list corruption → null |
| EMSocket.cpp | 173 | `delete pendingPacket; pendingPacket = NULL;` teardown still depends on surrounding queue/receive invariants |
| PartFile.cpp | 301–305 | `BufferedData_list` iteration — concurrent modification risk |
| BaseClient.cpp | 1818 | `_tcsdup()`-based username lifetime still relies on allocation succeeding outside the bounded parser pass |

**Status:** **[PARTIAL]**

---

### CPP_037 — Use-After-Free Patterns

**Count:** 7+ sites
**Severity:** Critical
**Priority:** CRITICAL

| File | Line | Pattern |
|---|---|---|
| BaseClient.cpp | 289–323 | Destructor: `m_Friend->SetLinkedClient(NULL)` then `delete m_Friend` — other thread may call `GetFriend()` |
| BaseClient.cpp | 293–296 | `socket->client = NULL; socket->Safe_Delete()` — socket deleted, client still references |
| ListenSocket.cpp | 177–181 | `client->socket = NULL; delete temp` — temp is client, stale refs |
| EMSocket.cpp | 155–160 | `ClearQueues` deletes packets — other code may hold stale refs |
| DownloadClient.cpp | 1215 | `delete[] *unzipped` — if pointer already freed |

**Status:** **[PARTIAL]**

---

### CPP_038 — RAII Wrapper Opportunities

**Count:** 4 remaining highest-impact wrappers
**Severity:** Medium
**Priority:** MEDIUM

| Wrapper | Target | Impact Files |
|---|---|---|
| `AutoSelectObject` (GDI) | `SelectObject` / restore pairs | Drawing code |
| `std::unique_ptr<BYTE[]>` | Raw `new BYTE[]` buffers | EMSocket.cpp, ClientUDPSocket.cpp, Packets.cpp |
| `PacketPtr` | `Packet*` ownership transfer | Packets.cpp, EMSocket.cpp |
| Scope guard for `CSafeMemFile` | Temp allocation lifetime | BaseClient.cpp |

**Status:** **[PARTIAL]**

---

### CPP_039 — noexcept Opportunities

**Count:** 3+ destructors missing `noexcept`
**Severity:** Low
**Priority:** LOW

| File | Class | Issue |
|---|---|---|
| Packets.cpp | `Packet::~Packet()` | Calls `delete[]`, should be `noexcept` |
| EMSocket.cpp | `CEMSocket::~CEMSocket()` | Calls cleanup methods, should be `noexcept` |
| BaseClient.cpp | `CUpDownClient::~CUpDownClient()` | Complex cleanup, should be `noexcept` |

**Status:** Open

---

### CPP_040 — [[nodiscard]] Candidates

**Count:** 10+ functions with commonly-ignored return values
**Severity:** Low
**Priority:** LOW

| File | Function | Return |
|---|---|---|
| SafeFile.cpp | `CSafeFile::Read()` | `UINT` — callers rely on exceptions instead |
| EMSocket.cpp | `Receive()` chain | `int` — must check for `SOCKET_ERROR` |
| AsyncSocketEx.cpp | `WSAPoll()` | `int` — some callers only check `<= 0` |
| ClientUDPSocket.cpp | `DecryptReceivedClient()` | Packet length — unchecked → crash |
| Packets.cpp | `UnPackPacket()` | `bool` — easy to forget |
| BaseClient.cpp | `AskForDownload()` | `bool` — ignored in some call sites |
| ListenSocket.cpp | `AttachToAlreadyKnown()` | `bool` — ignored then re-assigned |

**Status:** Open

---

## Consolidated Item Index

| ID | Summary | Severity | Priority | Status |
|---|---|---|---|---|
| CPP_001 | C-style casts (100+) | Medium | HIGH | Open |
| CPP_002 | Raw arrays / C strings (50+) | Medium | HIGH | Open |
| CPP_003 | Manual memory management (30+) | High | MEDIUM | Open |
| CPP_004 | Missing const / constexpr (20+) | Low | MEDIUM | Open |
| CPP_005 | Range-based for loop candidates (534) | Low | HIGH | Open |
| CPP_006 | Non-scoped enums (5) | Low | LOW | Open |
| CPP_007 | NULL → nullptr (2,262) | Low | HIGH | Open |
| CPP_008 | auto keyword opportunities (40+) | Low | MEDIUM | Open |
| CPP_009 | Structured bindings (3–5) | Low | LOW | Open |
| CPP_010 | Init statements in if/switch | Low | LOW | Open |
| CPP_011 | MFC → std container migration (159) | Medium | HIGH | Open |
| CPP_012 | Unsafe string functions (60) | High | HIGH | **[PARTIAL]** |
| CPP_013 | Algorithm opportunities (534 loops) | Medium | HIGH | Open |
| CPP_014 | GetTickCount / time() → std::chrono (297) | Medium | MEDIUM | Open |
| CPP_015 | CFile / CreateFile → std::filesystem (50+) | Low | MEDIUM | Open |
| CPP_016 | atoi / strtoul → std::from_chars (47) | Medium | LOW | Open |
| CPP_017 | rand() / srand() → \<random\> (40) | High | MEDIUM | Open |
| CPP_018 | Sentinel values → std::optional (20+) | Low | MEDIUM | Open |
| CPP_019 | LPCTSTR → std::string_view (100+) | Low | MEDIUM | Open |
| CPP_020 | Missing \<algorithm\> / \<numeric\> | Low | LOW | Open |
| CPP_021 | Shared mutable state without locks (12+) | Critical | CRITICAL | **[PARTIAL]** |
| CPP_022 | Lock ordering / deadlock risks (10+) | Critical | HIGH | **[PARTIAL]** |
| CPP_023 | TOCTOU races (12+) | High | HIGH | **[PARTIAL]** |
| CPP_024 | Thread-unsafe MFC calls (10+) | High | MEDIUM | **[PARTIAL]** |
| CPP_025 | Missing volatile / std::atomic (8+) | High | HIGH | **[PARTIAL]** |
| CPP_026 | CSingleLock anti-patterns (5+) | Medium | MEDIUM | **[PARTIAL]** |
| CPP_027 | Thread inventory (12 threads) | Info | Reference | **[DONE]** |
| CPP_028 | Sleep-based polling (5) | Medium | LOW | **[PARTIAL]** |
| CPP_029 | Thread pool opportunities (4) | Low | LOW | **[PARTIAL]** |
| CPP_030 | std::mutex migration candidates (5) | Medium | MEDIUM | **[PARTIAL]** |
| CPP_031 | Unchecked return values (12+) | High | HIGH | **[PARTIAL]** |
| CPP_032 | Exception safety (15+ leaks, 12+ silent catches) | High | HIGH | **[PARTIAL]** |
| CPP_033 | Buffer overflows (11+) | Critical | CRITICAL | **[PARTIAL]** |
| CPP_034 | Integer overflow risks (12+) | High | HIGH | **[PARTIAL]** |
| CPP_035 | Resource leaks (10+) | High | HIGH | **[PARTIAL]** |
| CPP_036 | Null pointer dereference risks (10+) | High | MEDIUM | **[PARTIAL]** |
| CPP_037 | Use-after-free patterns (7+) | Critical | CRITICAL | **[PARTIAL]** |
| CPP_038 | RAII wrapper opportunities (4) | Medium | MEDIUM | **[PARTIAL]** |
| CPP_039 | noexcept opportunities (3+) | Low | LOW | Open |
| CPP_040 | [[nodiscard]] candidates (10+) | Low | LOW | Open |

---

## Summary Statistics

| Category | Items | Critical | High | Medium | Low |
|---|---|---|---|---|---|
| Language Modernization | 10 | 0 | 0 | 3 | 7 |
| Standard Library Usage | 10 | 0 | 3 | 4 | 3 |
| Threading Risks | 10 | 2 | 3 | 4 | 1 |
| Error Handling & Safety | 10 | 3 | 5 | 1 | 1 |
| **Total** | **40** | **5** | **11** | **12** | **12** |

**Critical items (fix first):**
- CPP_021 — Shared mutable state without locks
- CPP_022 — Lock ordering / deadlock risks
- CPP_033 — Buffer overflows
- CPP_037 — Use-after-free patterns
- (CPP_025 — partially addressed with existing `std::atomic` usage)

---

## Modernization Roadmap

### Phase 1 — Safety-Critical Fixes (immediate)

| Action | Items | Effort |
|---|---|---|
| Add `std::atomic` to all shared flags | CPP_025 | Low |
| Audit/fix buffer overflows in packet parsing | CPP_033 | Medium |
| Fix use-after-free in destructors/deletion | CPP_037 | Medium |
| Document lock-ordering DAG | CPP_022 | Low |
| Validate sizes from network before `memcpy`/`new` | CPP_031, CPP_034 | Medium |

### Phase 2 — Mechanical Refactoring (low risk)

| Action | Items | Effort |
|---|---|---|
| Replace all `NULL` with `nullptr` | CPP_007 | Low (tooling) |
| Replace C-style casts | CPP_001 | Medium (tooling) |
| Replace `#define` constants with `constexpr` | CPP_004 | Low |
| Replace `rand()`/`srand()` with `<random>` | CPP_017 | Low |
| Replace `sprintf` with safe alternatives | CPP_012 | Low |

### Phase 3 — RAII & Exception Safety (medium risk)

| Action | Items | Effort |
|---|---|---|
| Replace `new[]`/`delete[]` with `std::unique_ptr<T[]>` or `std::vector` | CPP_003, CPP_032, CPP_035 | Medium |
| Add RAII wrappers for Win32 handles | CPP_038 | Low |
| Add `noexcept` to destructors | CPP_039 | Low |
| Replace silent `catch(...)` with logging | CPP_032 | Low |
| Add `[[nodiscard]]` to critical functions | CPP_040 | Low |

### Phase 4 — Container & Algorithm Migration (high effort)

| Action | Items | Effort |
|---|---|---|
| Migrate `CTypedPtrList` → `std::list` | CPP_011, CPP_005, CPP_013 | High |
| Migrate `CArray` → `std::vector` | CPP_011 | Medium |
| Migrate `CMap` → `std::unordered_map` | CPP_011 | Medium |
| Add `begin()`/`end()` adapters for remaining MFC containers | CPP_005 | Low |
| Replace `GetTickCount()` with `std::chrono` | CPP_014 | Medium |

### Phase 5 — Threading Modernization (highest risk)

| Action | Items | Effort |
|---|---|---|
| Migrate `CCriticalSection` → `std::mutex` / `std::shared_mutex` | CPP_030 | Medium |
| Replace `Sleep()` polling with condition variables | CPP_028 | Medium |
| Add thread pool for short-lived threads | CPP_029 | Medium |
| Fix TOCTOU races with proper lock scoping | CPP_023 | High |
| Route all worker→UI calls through `PostMessage` | CPP_024 | Medium |

---

*End of audit.*
