# eMule P2P Application — MFC / Dead Code / Cleanup Analysis

**Date:** 2026-03-24
**Branch:** v0.72a-broadband-dev
**Scope:** 556 C/C++ source files in `srchybrid/`

---

## Table of Contents

- [Executive Summary](#executive-summary)
- [1. MFC-Specific Dead Code Patterns](#1-mfc-specific-dead-code-patterns)
- [2. Deprecated Protocol Code](#2-deprecated-protocol-code)
- [3. ASSERT(0) Dead / Unhandled Code Paths](#3-assert0-dead--unhandled-code-paths)
- [4. TODO / FIXME — Abandoned Work Inventory](#4-todo--fixme--abandoned-work-inventory)
- [5. Stale Compatibility Comments](#5-stale-compatibility-comments--deadlake-proxysupport)
- [6. Legacy Windows Compatibility Code](#6-legacy-windows-compatibility-code)
- [7. Commented-Out Code Blocks (Large)](#7-commented-out-code-blocks-large)
- [8. Upload Compression Removal](#8-upload-compression-removal-done) **[DONE]**
- [9. Files With the Highest Dead Code Density](#9-files-with-the-highest-dead-code-density)
- [10. Summary Metrics](#10-summary-metrics)
- [11. Recommendations](#11-recommendations)

---

## Executive Summary

This report covers MFC-specific patterns, deprecated protocol handlers, dead code, abandoned features, and technical debt across the eMule codebase. The code shows clear signs of long iterative evolution: backward-compatibility layers accumulate, features are partially removed, and protocol deprecations are flagged but never followed through to completion.

**Estimated dead / effectively dead code:** 1,000–1,500 lines of unconditionally dead code plus 500–1,000 additional lines of conditional/deprecated paths.

**Cleanup potential:** A focused sweep of Priority-1 items could reduce codebase complexity by 30–50% in the affected subsystems.

### Update 2026-03-30

Many Priority-1 items have been completed:
- **[DONE]** `#if 0` blocks — all removed (REFAC_011, commit `ceb8edf`)
- **[DONE]** PeerCache opcode handlers — removed (REFAC_012, commit `0c5811d`)
- **[DONE]** `deadlake PROXYSUPPORT` comments — removed (REFAC_014, commit `fc0d12e`)
- **[DONE]** Windows 95 detection — removed (REFAC_015, commit `1771c30`)
- **[DONE]** Legacy INI key reads — removed (REFAC_016, commit `bf41753`)
- **[PARTIAL]** Encryption ASSERT(0) audit — most converted (REFAC_017, commit `2b9837c`)
- **[DONE]** Upload compression remnants — audited (REFAC_018, commit `683d19e`)
- **Remaining:** Source Exchange v1 branches (REFAC_013)

---

## 1. MFC-Specific Dead Code Patterns

*See REFAC_011 in REFACTOR-TASKS.md*

### 1.1 Commented-Out Message Handler Declarations

**File:** `srchybrid/AddSourceDlg.h` (lines 46–54)

```cpp
afx_msg void OnBnClickedRadio1();
//afx_msg void OnBnClickedRadio2();   // COMMENTED OUT
//afx_msg void OnBnClickedRadio3();   // COMMENTED OUT
afx_msg void OnBnClickedRadio4();
//afx_msg void OnBnClickedRadio5();   // COMMENTED OUT
//afx_msg void OnBnClickedRadio6();   // COMMENTED OUT
//afx_msg void OnBnClickedRadio7();   // COMMENTED OUT
```

Handlers 2, 3, 5, 6, 7 are declared as comments with no corresponding `ON_BN_CLICKED` entries in the message map and no implementation. These are UI design leftovers from a simplified radio-button group.

**Action:** Remove the commented lines entirely. *See REFAC_011 in REFACTOR-TASKS.md*

### 1.2 Never-Used Member Functions

**File:** `srchybrid/WebServer.h` (line 376)

```cpp
bool _GetIsTempDisabled() const { return m_bIsTempDisabled; } //never used
```

Self-documented as unused. The comment was left in rather than deleting the line.

**Action:** Delete the declaration and definition. *See REFAC_011 in REFACTOR-TASKS.md*

### 1.3 Explicitly-Unused Member Variable Cast

**File:** `srchybrid/BaseClient.cpp` (line 2249)

```cpp
(void)m_bySourceExchange1Ver;  // explicitly unused member variable
```

This is a suppression cast to silence a compiler warning about an unused variable. It signals the variable is dead in this code path. It belongs to the broader Source Exchange v1 deprecation (see Section 2.1).

### 1.4 Empty / Minimal Constructors

The following MFC dialog classes have constructors with empty bodies. These are not bugs per se — MFC dialogs commonly defer initialization to `OnInitDialog` — but several of these classes have so little implementation that the entire class should be audited for necessity:

| File | Notes |
|------|-------|
| `srchybrid/AddFriend.cpp:43` | Empty constructor `{ }` |
| `srchybrid/ArchivePreviewDlg.cpp:53` | Empty constructor |
| `srchybrid/ChatWnd.cpp:67` | Empty constructor |
| `srchybrid/ColorButton.cpp:59, 67` | Two empty constructors |
| `srchybrid/AddSourceDlg.cpp:47` | Empty constructor |
| `srchybrid/CollectionCreateDialog.cpp:62` | Empty constructor |
| `srchybrid/CollectionViewDialog.cpp:49` | Empty constructor |
| `srchybrid/CollectionListCtrl.cpp:140` | Empty constructor |

### 1.5 `#if 0` Disabled Code Blocks

*See REFAC_012 in REFACTOR-TASKS.md*

Remaining completely dead code, gated with `#if 0`. None of these compile. They represent abandoned experiments or design alternatives that were never removed:

| File | Lines | Description |
|------|-------|-------------|
| `srchybrid/EmuleDlg.cpp` | ~344–356 | Abandoned font-size UI experiment — comment says "introduces new glitches" |
| `srchybrid/DialogMinTrayBtn.cpp` | ~25–36 | Template/non-template compilation switch (always uses `#else` path) |
| `srchybrid/MuleListCtrl.cpp` | ~366, ~452 | Two dead blocks |
| `srchybrid/OtherFunctions.cpp` | ~3175 | Dead utility code |
| `srchybrid/SelfTest.cpp` | ~22 | Disabled self-test |
| `srchybrid/kademlia/io/DataIO.cpp` | ~494 | Dead Kad I/O path |

**Action:** Delete all `#if 0` ... `#endif` blocks. *See REFAC_012 in REFACTOR-TASKS.md*

The `EmuleDlg.cpp` block is the most interesting — it's a full alternative code path for dialog font sizing that was abandoned with a specific explanation. That explanation should be preserved in a commit message, then the code deleted.

---

## 2. Deprecated Protocol Code

### 2.1 Source Exchange v1 — Deprecated, Still Present

*See REFAC_013 in REFACTOR-TASKS.md*

**Status:** Active in the network path but flagged as deprecated.

The eMule Source Exchange protocol has two versions. v2 superseded v1 years ago. The codebase still sends and handles v1 for backward compatibility with very old clients, with heavy conditional branching throughout.

**Key locations:**

| File | Lines | Pattern |
|------|-------|---------|
| `srchybrid/BaseClient.cpp` | 473, 486, 768, 819, 884, 978, 994 | Comment: "4 Source Exchange — deprecated"; `m_bySourceExchange1Ver` throughout |
| `srchybrid/BaseClient.cpp` | 978 | `const UINT uSourceExchange1Ver = 4;` — hardcoded v1 constant |
| `srchybrid/DownloadClient.cpp` | 229, 316–327, 393–408 | `if (SupportsSourceExchange2())` vs v1 conditional branching |
| `srchybrid/DownloadClient.cpp` | 327 | Debug log: "SXSend (%s): Client source request; %s" |
| `srchybrid/DownloadClient.cpp` | 1421ff | `wasSkippedDueToSourceExchange` tracking — v1 swap logic |
| `srchybrid/DownloadClient.cpp` | 1754–1766 | "SourceExchange-Swap" debug logging |
| `srchybrid/UpdownClient.h` | — | `bool SupportsSourceExchange2() const` gating |

**Estimated removable code:** ~200–300 lines.

**Action:** Once minimum supported client version can be set to v2-capable, remove all `m_bySourceExchange1Ver` branches and `uSourceExchange1Ver` constant. *See REFAC_013 in REFACTOR-TASKS.md*

### 2.2 Deprecated Protocol Opcodes — Still Handled

*See REFAC_014 in REFACTOR-TASKS.md*

**File:** `srchybrid/Opcodes.h` (lines 253–286)

The following opcodes are explicitly marked deprecated or defunct in the header but still have active handler cases in `ListenSocket.cpp`:

```cpp
#define OP_REQUESTSOURCES      0x81  // *DEPRECATED*
#define OP_ANSWERSOURCES       0x82  // *DEPRECATED*
#define OP_MULTIPACKET         0x92  // *DEPRECATED*
#define OP_MULTIPACKETANSWER   0x93  // *DEPRECATED*
#define OP_PEERCACHE_QUERY     0x94  // *DEFUNCT*
#define OP_PEERCACHE_ANSWER    0x95  // *DEFUNCT*
#define OP_PEERCACHE_ACK       0x96  // *DEFUNCT*
#define OP_MULTIPACKET_EXT     0xA4  // *DEPRECATED*
#define OP_AICHFILEHASHANS     0x9D  // *DEPRECATED*
#define OP_AICHFILEHASHREQ     0x9E  // *DEPRECATED*
```

**Handler code:** `srchybrid/ListenSocket.cpp` (lines 834–1070)
- Lines 834–835: `case OP_MULTIPACKET` / `case OP_MULTIPACKET_EXT` — marked deprecated, still processes
- The `OP_PEERCACHE_*` opcodes are *DEFUNCT* — no current peer cache infrastructure exists

**Outgoing code:** `srchybrid/DownloadClient.cpp` (line 343)
```cpp
packet->opcode = OP_MULTIPACKET; // falls back to v1 if client doesn't support newer
```

The defunct `OP_PEERCACHE_*` handlers are pure dead weight — there is no peer cache to talk to. The deprecated opcode handlers are compatibility-only for outgoing traffic.

**Estimated removable code:** 100–150 lines.

**Action:**
1. Remove `OP_PEERCACHE_*` handlers entirely (defunct — no network infrastructure remains). *See REFAC_014 in REFACTOR-TASKS.md*
2. Keep deprecated opcode *receive* handlers for now (ancient clients may still send them).
3. Stop *sending* deprecated opcodes once minimum client version is enforced.

### 2.3 Kademlia v1 — Explicitly Rejected, Code Partially Retained

*See REFAC_015 in REFACTOR-TASKS.md*

**Status:** The code explicitly rejects Kad v1 connections at the protocol level, but some compatibility checks and comments remain.

| File | Lines | Content |
|------|-------|---------|
| `srchybrid/KademliaUDPListener.cpp` | ~395 | Comment: "old Kad1 opcodes which we don't handle any more" |
| `srchybrid/KademliaUDPListener.cpp` | ~1657 | "deprecated since KadVersion 7+, the result is now sent per TCP instead of UDP" |
| `srchybrid/RoutingZone.cpp` | 510, 516 | "legacy kad2 contacts" and `ASSERT(!pContact->IsIpVerified())` |

Since Kad1 is actively rejected at the connection gate, the remaining compatibility comment paths are safe to remove entirely.

**Estimated removable code:** 50–100 lines.

### 2.4 Obsolete Multi-Language Website Reference

**File:** `srchybrid/BaseClient.cpp` (line 937)

```cpp
// TODO implement multi language website which informs users of the effects of bad mods
```

Refers to an external website infrastructure that was never built. The TODO has been in the code for years with no action.

**Action:** Remove the comment.

---

## 3. `ASSERT(0)` Dead / Unhandled Code Paths

*See REFAC_016 in REFACTOR-TASKS.md*

`ASSERT(0)` is used throughout to mark code paths that "should never be reached." In release builds these assertions are compiled out, meaning the code falls through silently. The following files have the highest concentration:

| File | Count | Context |
|------|-------|---------|
| `srchybrid/BaseClient.cpp` | 17 | Protocol state machine, connection states, message parsing failures |
| `srchybrid/EncryptedStreamSocket.cpp` | 14 | Encryption handshake — comments say "must be a bug" |
| `srchybrid/ArchiveRecovery.cpp` | 14 | Archive parsing — many unhandled format cases |
| `srchybrid/DownloadClient.cpp` | 12 | Source exchange negotiation, block requests |
| `srchybrid/ListenSocket.cpp` | 10+ | Protocol parsing edge cases |
| `srchybrid/AICHSyncThread.cpp` | 5 | AICH hash synchronization edge cases |
| `srchybrid/AsyncSocketEx.cpp` | 9 | Async socket layer unreachable paths |

**Total across codebase:** 100+ instances across 100+ files.

**Key concern — `EncryptedStreamSocket.cpp`:** The comment "must be a bug" appears next to several `ASSERT(0)` calls. In a release build these become silent no-ops that may leave the socket in a corrupted state. These should be converted to proper error handling with `OnError()` and disconnect. *See REFAC_016 in REFACTOR-TASKS.md*

**Key concern — `ArchiveRecovery.cpp`:** Line 233 contains `ASSERT(0); // FIXME`. This is an acknowledged incomplete implementation. The `// FIXME` annotation was never acted on.

**Action:** Audit each `ASSERT(0)` in networking/encryption code. Those with "must be a bug" comments should become hard error paths. Those in dead feature code (`ArchiveRecovery`, `FrameGrabThread`) should be replaced with graceful error returns.

---

## 4. TODO / FIXME — Abandoned Work Inventory

107+ TODO/FIXME markers in the codebase. Actionable subset:

### 4.1 Acknowledged Incomplete Features

| File | Line | Note |
|------|------|------|
| `srchybrid/AICHSyncThread.cpp` | 367 | `// FIXME LARGE FILES (uncomment)` — large file AICH incomplete |
| `srchybrid/MetaDataDlg.cpp` | 345 | `// FIXME LARGE FILES` — same issue in metadata |
| `srchybrid/SearchList.cpp` | 703 | `// FIXME LARGE FILES` |
| `srchybrid/ArchiveRecovery.cpp` | 233 | `ASSERT(0); // FIXME` — unhandled archive format case |
| `srchybrid/IPFilter.cpp` | 229, 396 | "overlapping entries are not yet handled" — known correctness gap |
| `srchybrid/ClientList.cpp` | 607 | "Kad buddies won't work with RequireCrypt" — design conflict never resolved |
| `srchybrid/BaseClient.cpp` | 1458 | "FIXME: We don't know which kad version the buddy has" |

### 4.2 Debug Code Left In

| File | Line | Note |
|------|------|------|
| `srchybrid/DownloadQueue.cpp` | 481, 557 | `//if (thePrefs.GetDebugSourceExchange()) // TODO: Uncomment after testing` — debug call commented out, never re-enabled |

### 4.3 API Design Debt

| File | Line | Note |
|------|------|------|
| `srchybrid/DownloadListCtrl.cpp` | 1756 | "'GetCategory' SHOULD be a 'const' function" — const-correctness issue noted but not fixed |

### 4.4 Obsolete Configuration Loading [DONE]

*Handled by REFAC_016 in REFACTOR-TASKS.md*

The legacy `FileBufferSizePref` and `QueueSizePref` compatibility reads have already been removed
from `srchybrid/Preferences.cpp`. Keep this section only as historical context so the older audit
notes match the current tree.

---

## 5. Stale Compatibility Comments — "deadlake PROXYSUPPORT"

*See REFAC_018 in REFACTOR-TASKS.md*

The string "deadlake PROXYSUPPORT" appears 20+ times across multiple files as an inline attribution comment from an old patch. These comments are not dead code but are documentation noise and confuse readers unfamiliar with the history.

**Affected files:**

| File | Occurrences |
|------|------------|
| `srchybrid/EMSocket.cpp` | Lines 136, 140, 149, 236, 251, 629, 904 |
| `srchybrid/ServerConnect.h` | Line 28 — also in a `#define` |
| `srchybrid/Preferences.h` | Lines 102, 548, 1260 |
| `srchybrid/ServerConnect.cpp` | Line 514 |

**Action:** Remove all `// deadlake PROXYSUPPORT` attribution comments. The proxy support code itself is functional; only the attribution noise needs to go. *See REFAC_018 in REFACTOR-TASKS.md*

---

## 6. Legacy Windows Compatibility Code

### 6.1 Windows 95 Check [DONE]

*Handled by REFAC_015 in REFACTOR-TASKS.md*

The old Windows 95 / NT4 detection block has already been removed from `srchybrid/OtherFunctions.cpp`.

### 6.2 Obsolete Pref Keys [DONE]

As noted above, the stale `FileBufferSizePref` and `QueueSizePref` imports are no longer present in the live tree.

---

## 7. Commented-Out Code Blocks (Large)

Beyond `#if 0` blocks, several large sections are commented out with `//`:

- **`srchybrid/CorruptionBlackBox.cpp`** (lines 206–214): A `#ifdef _DEBUG` block that was converted to a comment block rather than deleted — a partial and inconsistent removal.

- **`srchybrid/BaseClient.cpp`**: Multiple lines of `(void)m_bySourceExchange1Ver;` style suppression casts and commented predecessor code mixed with live code, making the file harder to read.

---

## 8. Upload Compression Removal [DONE]

**Status:** Completed (REFAC_018, commit `683d19e`).

The upload compression audit was finalized. The unused `GetDataCompressionVersion()` accessor was removed. Live protocol compression infrastructure (`OP_PACKEDPROT` receive, source-exchange compression, server `OP_OFFERFILES` compression, Kad UDP packed-packet support) was intentionally kept. See REFAC_018 in REFACTOR-TASKS.md for the full scope.

---

## 9. Files With the Highest Dead Code Density

These files warrant a dedicated cleanup pass:

| File | Issues |
|------|--------|
| `srchybrid/BaseClient.cpp` | 17x ASSERT(0), Source Exchange v1 dead branches, unused member casts, obsolete mod comments |
| `srchybrid/EncryptedStreamSocket.cpp` | 14x ASSERT(0) with "must be a bug" comments — should be real error handling |
| `srchybrid/ArchiveRecovery.cpp` | 14x ASSERT(0) including acknowledged FIXME, partial feature |
| `srchybrid/DownloadClient.cpp` | 12x ASSERT(0), Source Exchange v1/v2 branching |
| `srchybrid/ListenSocket.cpp` | 10x ASSERT(0), defunct opcode handlers |
| `srchybrid/Preferences.cpp` | Obsolete config keys, Windows 9x compatibility |
| `srchybrid/OtherFunctions.cpp` | `#if 0` block, Windows 95 check |

---

## 10. Summary Metrics

| Category | Count | Lines Affected |
|----------|-------|---------------|
| `#if 0` dead blocks | 10 | ~300–400 |
| `ASSERT(0)` unreachable paths | 100+ | Scattered |
| TODO/FIXME markers | 107+ | Technical debt |
| Deprecated opcodes handled | 8 | ~100–150 |
| Defunct opcode handlers (`OP_PEERCACHE_*`) | 3 | ~30–50 |
| Source Exchange v1 conditional code | — | ~200–300 |
| Kademlia v1 remnants | Several | ~50–100 |
| `deadlake PROXYSUPPORT` comments | 20+ | Comments only |
| Windows 9x compat code | 1 block | ~20 |
| Obsolete config reads | 2 | ~5 |
| Commented-out message handlers | 5+ | UI debris |

**Total conservatively removable code:** ~1,000–1,500 lines of unconditionally dead code.
**Total simplifiable conditional code:** ~500–1,000 additional lines.

---

## 11. Recommendations

### Priority 1 — Safe, High Impact

1. **Delete all `#if 0` blocks** — Confirmed dead, no compilation risk, ~300–400 lines gone. *See REFAC_012 in REFACTOR-TASKS.md*

2. **Remove `OP_PEERCACHE_*` handlers** in `ListenSocket.cpp` — Infrastructure is defunct network-wide. *See REFAC_014 in REFACTOR-TASKS.md*

3. **Remove Source Exchange v1 branches** — Once minimum client floor can be set to v2-capable. Simplifies ~200–300 lines in `BaseClient.cpp` and `DownloadClient.cpp`. *See REFAC_013 in REFACTOR-TASKS.md*

4. **Remove `deadlake PROXYSUPPORT` attribution comments** — 20+ instances, comments only, zero risk. *See REFAC_018 in REFACTOR-TASKS.md*

5. **Remove Windows 95 check** in `OtherFunctions.cpp:624` — Dead on any supported OS. *See REFAC_017 in REFACTOR-TASKS.md*

### Priority 2 — Medium Effort

6. **Convert `ASSERT(0)` + "must be a bug" paths in `EncryptedStreamSocket.cpp` to `OnError()`** — In release builds these silently fail; they should disconnect cleanly. *See REFAC_016 in REFACTOR-TASKS.md*

7. **Replace `ASSERT(0); // FIXME` in `ArchiveRecovery.cpp:233`** with a graceful error return.

8. **Remove Kademlia v1 residual comments and dead branches** in `KademliaUDPListener.cpp`. *See REFAC_015 in REFACTOR-TASKS.md*

9. **Audit and clean up commented-out code** in `BaseClient.cpp` and `CorruptionBlackBox.cpp`.

10. **Remove or document** the large-file FIXME markers (`AICHSyncThread.cpp`, `MetaDataDlg.cpp`, `SearchList.cpp`) — either implement or formally defer.

### Priority 3 — Low Impact / Long Term

11. **Remove obsolete `.ini` key loading** in `Preferences.cpp` once migration window is closed. *See REFAC_017 in REFACTOR-TASKS.md*

12. **Resolve `ClientList.cpp:607`** — "Kad buddies won't work with RequireCrypt" is an unresolved design conflict that affects security posture.

13. **Audit upload compression remnants** after the WIP compression-removal commit is finalized.

14. **Remove never-used `WebServer.h:_GetIsTempDisabled()`**. *See REFAC_011 in REFACTOR-TASKS.md*

15. **Remove commented-out radio handler declarations** in `AddSourceDlg.h`. *See REFAC_011 in REFACTOR-TASKS.md*

---

*End of report.*
