# Refactor & Task Roadmap

**Branch:** `v0.72a-broadband-dev`
**Last updated:** 2026-03-29

This file consolidates all refactoring tasks, feature gaps, and actionable work items
with globally unique identifiers. Items marked **[DONE]** are completed and kept for
historical reference.

---

## Task Index

| ID | Category | Status | Summary |
|---|---|---|---|
| REFAC_001 | IRC Removal | **[DONE]** | Remove built-in IRC client (~5,300 LOC) |
| REFAC_002 | ZIP Handling | Planned | Replace custom CZIPFile with zlib minizip |
| REFAC_003 | GZIP Wrapper | Deferred | Inline or keep CGZIPFile wrapper |
| REFAC_004 | MIME Detection | Planned | Expand GetMimeType magic-byte table |
| REFAC_005 | MIME BZ2 Bug | Planned | Fix BZ2 signature matching bug |
| REFAC_006 | MIME Buffer | Planned | Reduce buffer size, reorder detection |
| REFAC_007 | MIME Forward Decl | Planned | Clean up PPgSecurity.cpp forward declaration |
| REFAC_008 | MIME WebM/MKV | Optional | Disambiguate WebM vs MKV (EBML DocType) |
| REFAC_009 | First-Start Socket | Planned | Remove startup wizard, unify socket init |
| REFAC_010 | Property Store | Exploratory | Windows Property Store for file metadata |
| REFAC_011 | Dead Code Sweep | Planned | Delete `#if 0` blocks (~300-400 lines) |
| REFAC_012 | PeerCache Opcodes | Planned | Remove defunct OP_PEERCACHE_* handlers |
| REFAC_013 | Source Exchange v1 | Planned | Remove deprecated SX v1 branches |
| REFAC_014 | Proxy Comments | Planned | Remove `deadlake PROXYSUPPORT` attribution noise |
| REFAC_015 | Win95 Compat | Planned | Remove Windows 95/NT4 detection code |
| REFAC_016 | Legacy INI Keys | Planned | Remove obsolete FileBufferSizePref/QueueSizePref reads |
| REFAC_017 | ASSERT(0) Audit | Planned | Convert "must be a bug" ASSERTs to real error handling |
| REFAC_018 | Upload Compression | Planned | Audit/remove compression stubs after WIP removal |

---

## REFAC_001 — Remove IRC Module [DONE]

**Status:** Completed (commits `a639213`, `b981984`)
**Lines removed:** ~5,300 across 14 source files, 2 icons, ~60 string resources, ~25 preference keys

### What was done

- Deleted all 14 IRC source files (.cpp/.h pairs): `IrcMain`, `IrcSocket`, `IrcWnd`,
  `IrcChannelTabCtrl`, `IrcChannelListCtrl`, `IrcNickListCtrl`, `PPgIRC`
- Removed IRC icon resources (`IRC.ico`, `IRCClipboard.ico`)
- Removed IRC tab/window from `EmuleDlg`
- Removed IRC toolbar button from `MuleToolBarCtrl`
- Removed IRC preferences page from `PreferencesDlg`
- Removed ~24 preference variables and accessors from `Preferences.h/.cpp`
- Removed ~50 `IDS_IRC*` string resources from `Resource.h` and `emule.rc`
- Removed menu command IDs from `MenuCmds.h`
- Removed IRC context-menu entries from `SharedFilesCtrl.cpp` and `ServerListCtrl.cpp`
- Removed help IDs from `HelpIDs.h`
- Removed `MAX_IRC_MSG_LEN` from `Opcodes.h`

### Remaining (minor)

4 string resources intentionally kept for WebServer/ChatSelector compatibility:
`IDS_IRC_CONNECT`, `IDS_IRC_DISCONNECT`, `IDS_IRC_PERFORM`, `IDS_IRC_ADDTOFRIENDLIST`.
These need renaming to generic labels in a future pass.

---

## REFAC_002 — Replace Custom ZIP Reader with minizip

**Status:** Planned
**Effort:** Low (3 consumer sites, well-documented migration)
**Files:** `ZIPFile.cpp/.h` → delete; add minizip sources from `eMule-zlib/contrib/minizip/`

### Background

The codebase contains `CZIPFile` (~530 lines, origin: Shareaza 2002-2004), a hand-rolled
ZIP central-directory parser + deflate extractor. This can be replaced by **minizip**
(`unzip.h`) which is already present in the zlib dependency tree.

### Consumers (3 call sites)

1. **`IPFilterDlg.cpp`** — IP filter import from local `.zip` file
2. **`PPgSecurity.cpp`** — IP filter download + extract from `.zip`
3. **`OtherFunctions.cpp`** — Skin package (.zip) installation

### Implementation phases

1. Add `unzip.c`, `ioapi.c`, `iowin32.c` from `eMule-zlib/contrib/minizip/` to vcxproj
2. Optionally write thin `CMiniZipReader` wrapper matching CZIPFile API surface
3. Migrate all 3 consumer sites
4. Delete `ZIPFile.cpp/.h` from project

### Verification

- [ ] Project compiles with minizip sources
- [ ] IP filter import from `.zip` works (local file dialog)
- [ ] IP filter download as `.zip` works (PPgSecurity)
- [ ] Skin package installation works
- [ ] `ZIPFile.cpp/.h` deleted, zero remaining references

---

## REFAC_003 — Simplify or Keep GZIPFile Wrapper

**Status:** Deferred (low value)
**Effort:** Trivial

`CGZIPFile` (~120 lines) is already a thin wrapper around zlib's `gz*` API.
Recommendation: **keep it** — it's trivial, correct, and uses zlib directly.
Only revisit if the ZIP refactor creates a natural opportunity to inline.

---

## REFAC_004 — Expand GetMimeType Magic-Byte Table

**Status:** Planned
**Effort:** Low (~100 lines of new table entries)
**File:** `srchybrid/MediaInfo.cpp:504`

### Current state

Two-stage detection:
1. `FindMimeFromData` (Windows urlmon.dll) — ~26 hardcoded types
2. Manual magic-byte checks for RAR, BZ2, ACE, LHA (4 formats only)

### Planned expansion

Replace the `if`/`memcmp` chain with a static `MagicEntry` lookup table covering:

| Format | Magic | MIME |
|---|---|---|
| RAR 4 | `52 61 72 21 1A 07 00` | `application/x-rar-compressed` |
| RAR 5 | `52 61 72 21 1A 07 01 00` | `application/x-rar-compressed` |
| 7z | `37 7A BC AF 27 1C` | `application/x-7z-compressed` |
| BZ2 | `42 5A 68` + digit `1-9` | `application/x-bzip2` |
| XZ | `FD 37 7A 58 5A 00` | `application/x-xz` |
| GZ | `1F 8B` | `application/gzip` |
| ZIP | `50 4B 03 04` | `application/zip` |
| ACE | offset 7: `2A 2A 41 43 45 2A 2A` | `application/x-ace-compressed` |
| LHA/LZH | offset 2: `2D 6C 68 35 2D` | `application/x-lha-compressed` |
| ISO 9660 | offset 0x8001: `43 44 30 30 31` | `application/x-iso9660-image` |
| MKV/WebM | `1A 45 DF A3` | `video/x-matroska` |
| OGG | `4F 67 67 53` | `audio/ogg` |
| FLAC | `66 4C 61 43` | `audio/flac` |
| MP4/M4A | offset 4: `66 74 79 70` | `video/mp4` |
| FLV | `46 4C 56` | `video/x-flv` |
| ASF (WMV/WMA) | `30 26 B2 75 8E 66 CF 11` | `video/x-ms-asf` |
| TORRENT | `64 38 3A 61 6E 6E 6F 75 6E 63 65` | `application/x-bittorrent` |

Run the table **before** `FindMimeFromData` (faster, more reliable). Fall back to
`FindMimeFromData` only for types not covered.

---

## REFAC_005 — Fix BZ2 Signature Bug

**Status:** Planned
**Effort:** Trivial (1-line fix)

Current code checks for `"BZh19"` (5 bytes). Real BZ2 header is `"BZh"` followed by
block-size digit `1`-`9`. Current check only matches block size 1 (rare).

**Fix:** Match `"BZh"` (3 bytes) then verify `buffer[3] >= '1' && buffer[3] <= '9'`.

---

## REFAC_006 — Reduce MIME Buffer and Reorder Detection

**Status:** Planned
**Effort:** Low

- Shrink buffer from 8 KB to 512 bytes (all non-ISO signatures fit in first 16 bytes)
- For ISO check, do a targeted seek only when no earlier signature matched
- Run magic-byte table before `FindMimeFromData` (no COM call overhead)

---

## REFAC_007 — Clean Up MIME Forward Declaration

**Status:** Planned
**Effort:** Trivial

`PPgSecurity.cpp:40` has a bare `bool GetMimeType(...)` declaration instead of
including `MediaInfo.h`. Replace with `#include "MediaInfo.h"`.

---

## REFAC_008 — WebM vs MKV Disambiguation (Optional)

**Status:** Optional
**Effort:** Low

MKV and WebM share EBML header `1A 45 DF A3`. To distinguish, parse the EBML DocType
element deeper in the header (`webm` vs `matroska`). Low priority — returning
`video/x-matroska` for both is fine for file-info dialog use.

---

## REFAC_009 — First-Start Socket Rework

**Status:** Planned (workaround in place)
**Effort:** Medium

### Current issue

On fresh config, the first-time wizard pre-opens TCP/UDP sockets via `Rebind()`. Later,
normal startup tries to create the same sockets again, causing debug assertions in
`AsyncSocketEx.cpp` and MFC `sockcore.cpp`.

### Current workaround

Same-port second creation made a no-op in `ListenSocket.cpp` and `ClientUDPSocket.cpp`
(commit `9b13906`).

### Planned fix

- Remove the startup wizard entirely
- Replace with a single, non-duplicated first-run initialization path
- Own port selection, socket startup, and first-run defaults deterministically

---

## REFAC_010 — Windows Property Store Metadata (Exploratory)

**Status:** Exploratory — not committed to implementation
**Effort:** Unknown

Explore using the Windows Property Store (`IPropertyStore`) as the first metadata path
for non-audio/video file types (images, documents, archives). Keep `MediaInfo.dll` as
optional fallback where it adds coverage beyond the Windows property system.

---

## REFAC_011 — Delete `#if 0` Dead Code Blocks

**Status:** Planned
**Effort:** Low (~300-400 lines, zero risk)

10 blocks of completely dead code gated with `#if 0`:

| File | Description |
|---|---|
| `EmuleDlg.cpp` | Abandoned font-size UI experiment |
| `DialogMinTrayBtn.cpp` | Template/non-template compilation switch |
| `IESecurity.cpp` | Disabled security code |
| `MiniMule.cpp` (×2) | Two dead blocks |
| `MuleListCtrl.cpp` (×2) | Two dead blocks |
| `OtherFunctions.cpp` | Dead utility code |
| `SelfTest.cpp` | Disabled self-test |
| `kademlia/io/DataIO.cpp` | Dead Kad I/O path |

---

## REFAC_012 — Remove Defunct PeerCache Opcode Handlers

**Status:** Planned
**Effort:** Low (~30-50 lines)

`OP_PEERCACHE_QUERY`, `OP_PEERCACHE_ANSWER`, `OP_PEERCACHE_ACK` are marked *DEFUNCT* in
`Opcodes.h` — PeerCache infrastructure was fully removed in v0.70b. The handler cases
in `ListenSocket.cpp` are pure dead weight.

---

## REFAC_013 — Remove Source Exchange v1 Branches

**Status:** Planned
**Effort:** Medium (~200-300 lines across BaseClient.cpp, DownloadClient.cpp)

Source Exchange v2 superseded v1 years ago. All `m_bySourceExchange1Ver` branches and
the `uSourceExchange1Ver = 4` constant can be removed once minimum client version is
set to v2-capable.

---

## REFAC_014 — Remove `deadlake PROXYSUPPORT` Comments

**Status:** Planned
**Effort:** Trivial (20+ comment-only removals)

Attribution comments from an old patch, scattered across `EMSocket.cpp`, `ServerConnect.h`,
`Preferences.h`, `ServerConnect.cpp`. The proxy code itself stays; only noise removed.

---

## REFAC_015 — Remove Windows 95/NT4 Detection

**Status:** Planned
**Effort:** Trivial

`OtherFunctions.cpp:624` — Windows 95 (NT 4.0) detection. Dead on any supported OS
(minimum is Windows 10).

---

## REFAC_016 — Remove Legacy INI Key Reads

**Status:** Planned
**Effort:** Trivial

`Preferences.cpp:2185, 2194` — `FileBufferSizePref` and `QueueSizePref` are deprecated
import keys from a prior configuration format. No write path exists. Remove after
confirming minimum supported config file age.

---

## REFAC_017 — ASSERT(0) Audit in Networking/Encryption

**Status:** Planned
**Effort:** Medium

Convert `ASSERT(0)` + "must be a bug" paths in `EncryptedStreamSocket.cpp` (14 instances)
to proper `OnError()` + disconnect. In release builds these are silent no-ops that leave
sockets in indeterminate state.

Also: replace `ASSERT(0); // FIXME` in `ArchiveRecovery.cpp:233` with graceful error return.

---

## REFAC_018 — Audit Upload Compression Remnants

**Status:** Planned (blocked on WIP compression removal finalization)
**Effort:** Low

After WIP commit `6c6fd3f` is finalized, grep for `zlib`, `compress`, `uncompress`,
`OP_PACKEDPROT` and remove any now-dead references.

---

## Priority Ranking

### Immediate (low risk, high cleanup value)

1. REFAC_011 — Delete `#if 0` blocks
2. REFAC_012 — Remove PeerCache opcode handlers
3. REFAC_014 — Remove proxy attribution comments
4. REFAC_015 — Remove Win95 detection
5. REFAC_005 — Fix BZ2 signature bug

### Short-term (moderate effort, good payoff)

6. REFAC_002 — Replace CZIPFile with minizip
7. REFAC_004 — Expand MIME detection table
8. REFAC_006 — Reduce MIME buffer + reorder
9. REFAC_007 — Clean up MIME forward declaration
10. REFAC_009 — First-start socket rework

### Medium-term (requires more testing)

11. REFAC_013 — Remove Source Exchange v1
12. REFAC_017 — ASSERT(0) audit
13. REFAC_016 — Legacy INI key removal
14. REFAC_018 — Compression remnant audit
