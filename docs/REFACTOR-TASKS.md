# Refactor & Task Roadmap

**Branch:** `v0.72a-broadband-dev`
**Last updated:** 2026-03-31

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
| REFAC_004 | MIME Detection | **[DONE]** | Expand GetMimeType magic-byte table |
| REFAC_005 | MIME BZ2 Bug | **[DONE]** | Fix BZ2 signature matching bug |
| REFAC_006 | MIME Buffer | **[DONE]** | Reduce buffer size, reorder detection |
| REFAC_007 | MIME Forward Decl | **[DONE]** | Clean up PPgSecurity.cpp forward declaration |
| REFAC_008 | MIME WebM/MKV | Optional | Disambiguate WebM vs MKV (EBML DocType) |
| REFAC_009 | First-Start Socket | **[DONE]** | Remove startup wizard, unify socket init |
| REFAC_010 | Property Store | Exploratory | Windows Property Store for file metadata |
| REFAC_011 | Dead Code Sweep | **[DONE]** | Delete `#if 0` blocks (~300-400 lines) |
| REFAC_012 | PeerCache Opcodes | **[DONE]** | Remove defunct OP_PEERCACHE_* handlers |
| REFAC_013 | Source Exchange v1 | Planned | Remove deprecated SX v1 branches |
| REFAC_014 | Proxy Comments | **[DONE]** | Remove `deadlake PROXYSUPPORT` attribution noise |
| REFAC_015 | Win95 Compat | **[DONE]** | Remove Windows 95/NT4 detection code |
| REFAC_016 | Legacy INI Keys | **[DONE]** | Remove obsolete FileBufferSizePref/QueueSizePref reads |
| REFAC_017 | ASSERT(0) Audit | **[PARTIAL]** | Convert "must be a bug" ASSERTs to real error handling |
| REFAC_018 | Upload Compression | **[DONE]** | Audit upload-removal remnants and keep live protocol compression |

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

## REFAC_004 — Expand GetMimeType Magic-Byte Table [DONE]

**Status:** Completed (commit `2f026c7`)

Replaced the chain of `if`/`memcmp` blocks with a static `MagicEntry` lookup table
covering 17 format types: RAR (3 variants), 7z, BZ2, XZ, GZ, ZIP, ACE, LHA, EBML
(MKV/WebM), OGG, FLAC, MP4, FLV, ASF, TORRENT. Table is checked before
`FindMimeFromData` for speed and reliability.

---

## REFAC_005 — Fix BZ2 Signature Bug [DONE]

**Status:** Completed (commit `2f026c7`)

Replaced the old `"BZh19"` 5-byte check with a dedicated `MatchMimeMagicBZip()` function
that correctly matches `"BZh"` followed by any digit `1`-`9`.

---

## REFAC_006 — Reduce MIME Buffer and Reorder Detection [DONE]

**Status:** Completed (commit `2f026c7`)

Magic-byte table now runs before `FindMimeFromData`. Buffer handling optimized.

---

## REFAC_007 — Clean Up MIME Forward Declaration [DONE]

**Status:** Completed (commit `2f026c7`)

`PPgSecurity.cpp` now properly uses `GetMimeType()` at line 231 instead of a bare
forward declaration.

---

## REFAC_008 — WebM vs MKV Disambiguation (Optional)

**Status:** Optional
**Effort:** Low

MKV and WebM share EBML header `1A 45 DF A3`. To distinguish, parse the EBML DocType
element deeper in the header (`webm` vs `matroska`). Low priority — returning
`video/x-matroska` for both is fine for file-info dialog use.

---

## REFAC_009 — First-Start Socket Rework [DONE]

**Status:** Completed (commit `83ff501`)

### What was done

- Removed the startup wizard entirely (`PShtWiz1.cpp`, 854 lines deleted)
- Removed wizard menu command and associated UI
- The `m_bFirstStart` flag in Preferences is retained solely for detecting first
  application launch for initialization logic (not a UI wizard)
- Socket initialization unified into a single non-duplicated startup path
- Connection presets dialog also removed in follow-up commit `dbf9133`

---

## REFAC_010 — Windows Property Store Metadata (Exploratory)

**Status:** Exploratory — not committed to implementation
**Effort:** Unknown

Explore using the Windows Property Store (`IPropertyStore`) as the first metadata path
for non-audio/video file types (images, documents, archives). Keep `MediaInfo.dll` as
optional fallback where it adds coverage beyond the Windows property system.

---

## REFAC_011 — Delete `#if 0` Dead Code Blocks [DONE]

**Status:** Completed (commit `ceb8edf`)

Removed all 10 `#if 0` blocks across: `AddSourceDlg.h`, `DialogMinTrayBtn.cpp`,
`EmuleDlg.cpp`, `IESecurity.cpp`, `MiniMule.cpp`, `MuleListCtrl.cpp`,
`OtherFunctions.cpp`, `SelfTest.cpp`, `WebServer.h`, `kademlia/io/DataIO.cpp`.

---

## REFAC_012 — Remove Defunct PeerCache Opcode Handlers [DONE]

**Status:** Completed (commit `0c5811d`)

All `OP_PEERCACHE_QUERY`, `OP_PEERCACHE_ANSWER`, `OP_PEERCACHE_ACK` references removed
from `Opcodes.h` and `ListenSocket.cpp`.

---

## REFAC_013 — Remove Source Exchange v1 Branches

**Status:** Planned
**Effort:** Medium (~200-300 lines across BaseClient.cpp, DownloadClient.cpp)

Source Exchange v2 superseded v1 years ago. All `m_bySourceExchange1Ver` branches and
the `uSourceExchange1Ver = 4` constant can be removed once minimum client version is
set to v2-capable.

---

## REFAC_014 — Remove `deadlake PROXYSUPPORT` Comments [DONE]

**Status:** Completed (commit `fc0d12e`)

Removed all 20+ `deadlake PROXYSUPPORT` attribution comments from `EMSocket.cpp`,
`ServerConnect.h`, `Preferences.h`, `ServerConnect.cpp`.

---

## REFAC_015 — Remove Windows 95/NT4 Detection [DONE]

**Status:** Completed (commit `1771c30`)

Removed Windows 95 detection code and the fake Windows TCP half-open limit helper
(commit `879b081`).

---

## REFAC_016 — Remove Legacy INI Key Reads [DONE]

**Status:** Completed (commit `bf41753`)

Removed `FileBufferSizePref`, `QueueSizePref`, and other deprecated import keys from
`Preferences.cpp`.

---

## REFAC_017 — ASSERT(0) Audit in Networking/Encryption [PARTIAL]

**Status:** Partially done (commit `2b9837c`)

Most "must be a bug" ASSERT(0) paths in `EncryptedStreamSocket.cpp` were converted to
proper error handling. 4 defensive assertions remain at lines 149, 699, 711, 753 —
these are in critical failure paths (`FailEncryptedStream`, buffer state machine edge
cases, worst-case RNG failure) where the ASSERT is intentionally retained as a debug
diagnostic.

The `ArchiveRecovery.cpp:233` ASSERT(0) FIXME has **not** been addressed.

---

## REFAC_018 — Audit Upload Compression Remnants [DONE]

**Status:** Completed (commit `683d19e`)
**Effort:** Low

Audited the current tree after WIP commit `6c6fd3f` and narrowed the task to true
upload-removal remnants instead of broad protocol compression removal.

### What was done

- Removed the now-unused `CUpDownClient::GetDataCompressionVersion()` accessor, which no
  longer has any callers after the upload compressed-part path was deleted.
- Confirmed that the upload thread now always emits standard upload packets and no longer
  retains dead compressed-part generation helpers.
- Locked the task scope to dead upload-removal remnants only, not unrelated zlib or
  packet-compression infrastructure.

### Intentionally kept

- `OP_PACKEDPROT` receive/unpack compatibility on the ED2K TCP path
- Source-exchange packet compression
- Server `OP_OFFERFILES` compression
- Kad UDP packed-packet send/receive support
- Current client hello/mule-info compression capability advertisement

---

## Priority Ranking

### Remaining work (items not yet [DONE])

**Immediate (low risk, high cleanup value):**
1. REFAC_002 — Replace CZIPFile with minizip
2. REFAC_013 — Remove Source Exchange v1 branches

**Optional / exploratory:**
4. REFAC_008 — WebM/MKV disambiguation
5. REFAC_010 — Windows Property Store metadata
6. REFAC_003 — GZIPFile wrapper (deferred, low value)
