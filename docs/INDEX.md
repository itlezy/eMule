# eMule Documentation Index

**Branch:** `v0.72a-broadband-dev`
**Last updated:** 2026-04-02

---

## Table of Contents

- [Architecture](#architecture)
- [Audits](#audits)
- [Dependencies](#dependencies)
- [Features](#features)
- [Guides](#guides)
- [History](#history)
- [Plans](#plans)
- [Refactoring](#refactoring)
- [Consolidated Item Index](#consolidated-item-index)

---

## Architecture

| Document | Description |
|---|---|
| [ARCH-NETWORKING](ARCH-NETWORKING.md) | Full networking stack analysis — sockets, `WSAPoll`, UPnP, throttling, encryption |
| [ARCH-THREADING](ARCH-THREADING.md) | Threading model analysis — thread inventory, sync primitives, IOCP patterns, migration roadmap (FEAT_029, FEAT_030) |
| [ARCH-PREFERENCES](ARCH-PREFERENCES.md) | Preferences reference — all INI keys, UI exposure, hidden prefs, modes, defaults |

## Audits

| Document | Description | Items |
|---|---|---|
| [AUDIT-BUGS](AUDIT-BUGS.md) | Full static bug audit — buffer overflows, use-after-free, protocol parsing, GDI leaks, logic bugs | BBUG_001–050 (all triaged) |
| [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) | Code review of v0.72a upstream changes — Ring.h, CaptchaGenerator, BarShader, WebSocket | CODEREV_001–012 |
| [AUDIT-CODEQUALITY](AUDIT-CODEQUALITY.md) | Code quality roadmap — CMake migration, clang-tidy, clang-format, cppcheck, sanitizers |  |
| [CPP-AUDIT](CPP-AUDIT.md) | C++ language & safety audit — casts, RAII, threading risks, buffer safety, std library usage | CPP_001–040 |
| [AUDIT-DEADCODE](AUDIT-DEADCODE.md) | Dead code / cleanup analysis — MFC patterns, deprecated opcodes, ASSERT(0) paths, TODO inventory | Cross-refs REFAC_011–018 |
| [AUDIT-DEFECTS](AUDIT-DEFECTS.md) | Preferences persistence audit — INI key mismatches, load-only hidden prefs | BUG_010–011 |
| [AUDIT-KAD](AUDIT-KAD.md) | Kademlia security/routing audit — bootstrap trust, SafeKad, FastKad, abuse budgets, libtorrent comparison | AUD_KAD_001–022 |
| [AUDIT-SECURITY](AUDIT-SECURITY.md) | Security code review — TLS, crypto algorithms, RNG, buffer safety, protocol obfuscation | BUG_001–003, GAP_001–004 |
| [AUDIT-WWMOD](AUDIT-WWMOD.md) | "What Would a Modern OS Do?" — Win10+ compatibility audit of archaic patterns | WWMOD_001–050 |

## Dependencies

| Document | Description | Items |
|---|---|---|
| [DEP-STATUS](DEP-STATUS.md) | Dependency health check — pinned versions, upstream activity, recommendations | |
| [DEP-REMOVAL](DEP-REMOVAL.md) | Full dependency removal analysis — usage depth, impact matrix, alternatives | DEP_001–006 |
| [DEP-REMOVAL-DLL](DEP-REMOVAL-DLL.md) | Static-to-DLL conversion feasibility per dependency | DEP_001–006 |

## Features

| Document | Description | Items |
|---|---|---|
| [FEATURE-BROADBAND](FEATURE-BROADBAND.md) | Broadband upload slot controller — BBMaxUpClientsAllowed, session rotation, slow-slot reclamation | FEAT_001 **[DONE]** |
| [FEATURE-KAD](FEATURE-KAD.md) | Kad improvement plan — SafeKad2, FastKad, routing quality, observability | FEAT_002–008 |
| [FEATURE-MODERN-LIMITS](FEATURE-MODERN-LIMITS.md) | Modern limits plan — connection budget, socket buffers, queue/source caps, timeouts | FEAT_013–019 |
| [FEATURE-PEERS-BANS](FEATURE-PEERS-BANS.md) | Peer banning analysis from eMuleAI — CShield, SafeKad2, CAntiNick | FEAT_009–012 |
| [FEATURE-THUMBS](FEATURE-THUMBS.md) | Retired thumbnail-preview capability notes and remaining MediaInfo context | FEAT_022–024 |

## Guides

| Document | Description |
|---|---|
| [GUIDE-LONGPATHS](GUIDE-LONGPATHS.md) | Long path support implementation guide — registry detection, `\\?\` prefix, CRT-bypass helpers |

## History

| Document | Description |
|---|---|
| [HISTORY-070-VS-072](HISTORY-070-VS-072.md) | Detailed comparison report: eMule 0.70b vs 0.72a — CRing, ARM64, CxImage removal, API modernization |
| [HISTORY-CHANGELOG](HISTORY-CHANGELOG.md) | Community build code review: v0.60d → v0.70b → v0.72a — IOCP, TLS 1.3, PeerCache removal, UNC paths |

## Plans

| Document | Description | Items |
|---|---|---|
| [PLAN-API-SERVER](PLAN-API-SERVER.md) | REST API server design — named pipe IPC + Node.js sidecar, qBittorrent-compatible endpoints, SSE | |
| [PLAN-CMAKE](PLAN-CMAKE.md) | MSBuild → CMake + Ninja + vcpkg migration plan | |
| [PLAN-MODERNIZATION-2026](PLAN-MODERNIZATION-2026.md) | Full 12-month modernization roadmap across 15 engineering pillars | |
| [PLAN-RESTRUCTURE](PLAN-RESTRUCTURE.md) | Module restructuring guidance — srchybrid/ folder split into core/net/ui/media/web/platform | PLAN_001 |

## Refactoring

| Document | Description | Items |
|---|---|---|
| [REFACTOR-TASKS](REFACTOR-TASKS.md) | Refactor & task roadmap — IRC removal, ZIP/GZIP, MIME, dead code, PeerCache, Source Exchange, compression | REFAC_001–018 |

---

## Consolidated Item Index

### Refactoring Tasks (REFAC_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| REFAC_001 | Remove built-in IRC client (~5,300 LOC) | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_002 | Replace CZIPFile with minizip | Planned | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_003 | Inline or keep CGZIPFile wrapper | Deferred | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_004 | Expand GetMimeType magic-byte table | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_005 | Fix BZ2 signature matching bug | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_006 | Reduce buffer size, reorder MIME detection | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_007 | Clean up PPgSecurity.cpp forward declaration | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_008 | Disambiguate WebM vs MKV (EBML DocType) | Optional | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_009 | Remove startup wizard, unify socket init | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_010 | Windows Property Store for file metadata | Exploratory | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_011 | Delete `#if 0` blocks (~300-400 lines) | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_012 | Remove defunct OP_PEERCACHE_* handlers | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_013 | Remove deprecated Source Exchange v1 branches | Planned | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_014 | Remove `deadlake PROXYSUPPORT` attribution noise | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_015 | Remove Windows 95/NT4 detection code | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_016 | Remove obsolete INI key reads | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_017 | Convert ASSERT(0) to real error handling | **[PARTIAL]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |
| REFAC_018 | Audit upload compression remnants | **[DONE]** | [REFACTOR-TASKS](REFACTOR-TASKS.md) |

### Feature Work (FEAT_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| FEAT_001 | Broadband upload slot controller | **[DONE]** | [FEATURE-BROADBAND](FEATURE-BROADBAND.md) |
| FEAT_002 | CSafeKad2 import and refactor | **[PARTIAL]** | [FEATURE-KAD](FEATURE-KAD.md) |
| FEAT_003 | CFastKad import and refactor | **[PARTIAL]** | [FEATURE-KAD](FEATURE-KAD.md) |
| FEAT_004 | Kad integration into entry points | **[PARTIAL]** | [FEATURE-KAD](FEATURE-KAD.md) |
| FEAT_005 | Routing quality improvements | Not started | [FEATURE-KAD](FEATURE-KAD.md) |
| FEAT_006 | Kad observability/diagnostics | Not started | [FEATURE-KAD](FEATURE-KAD.md) |
| FEAT_007 | Persisted trust cache (optional) | Not started | [FEATURE-KAD](FEATURE-KAD.md) |
| FEAT_008 | Search-mode adaptation (optional) | Not started | [FEATURE-KAD](FEATURE-KAD.md) |
| FEAT_009 | SafeKad2 integration (peer banning) | Not started | [FEATURE-PEERS-BANS](FEATURE-PEERS-BANS.md) |
| FEAT_010 | CAntiNick integration | **[REJECTED]** | [FEATURE-PEERS-BANS](FEATURE-PEERS-BANS.md) |
| FEAT_011 | CShield integration | Not started | [FEATURE-PEERS-BANS](FEATURE-PEERS-BANS.md) |
| FEAT_012 | PR_TCPERRORFLOODER (listen socket DoS defense) | Not started | [FEATURE-PEERS-BANS](FEATURE-PEERS-BANS.md) |
| FEAT_013 | Connection budget defaults | **[DONE]** | [FEATURE-MODERN-LIMITS](FEATURE-MODERN-LIMITS.md) |
| FEAT_014 | Per-client upload cap → 8 MB/s | **[DONE]** | [FEATURE-MODERN-LIMITS](FEATURE-MODERN-LIMITS.md) |
| FEAT_015 | Socket buffer sizes | **[DONE]** | [FEATURE-MODERN-LIMITS](FEATURE-MODERN-LIMITS.md) |
| FEAT_016 | Disk buffering defaults → 64 MiB | **[DONE]** | [FEATURE-MODERN-LIMITS](FEATURE-MODERN-LIMITS.md) |
| FEAT_017 | Queue/source limits | **[DONE]** | [FEATURE-MODERN-LIMITS](FEATURE-MODERN-LIMITS.md) |
| FEAT_018 | Timeout adjustments | **[DONE]** | [FEATURE-MODERN-LIMITS](FEATURE-MODERN-LIMITS.md) |
| FEAT_019 | Advanced tree UI exposure | **[DONE]** | [FEATURE-MODERN-LIMITS](FEATURE-MODERN-LIMITS.md) |
| FEAT_022 | Windows Media Foundation migration | **[REJECTED]** thumbnail preview retired | [FEATURE-THUMBS](FEATURE-THUMBS.md) |
| FEAT_023 | FFmpeg alternative (long-term) | **[REJECTED]** thumbnail preview retired | [FEATURE-THUMBS](FEATURE-THUMBS.md) |
| FEAT_024 | MediaInfo static embedding | **[REJECTED]** | [FEATURE-THUMBS](FEATURE-THUMBS.md) |
| FEAT_029 | Track B — Worker thread hygiene | Not started | [ARCH-THREADING](ARCH-THREADING.md) |
| FEAT_030 | Track A — Network IOCP migration | Not started | [ARCH-THREADING](ARCH-THREADING.md) |

### Bug Fixes & Security (BUG_, GAP_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| BUG_001 | `strcpy()` without bounds in Emule.cpp | **[DONE]** | [AUDIT-SECURITY](AUDIT-SECURITY.md) |
| BUG_002 | `rand()` for crypto challenge value | **[REJECTED]** | [AUDIT-SECURITY](AUDIT-SECURITY.md) |
| BUG_003 | `srand(time(NULL))` predictable seed | **[ACCEPTED]** | [AUDIT-SECURITY](AUDIT-SECURITY.md) |
| BUG_010 | Upload overhead INI key mismatch | **[DONE]** | [AUDIT-DEFECTS](AUDIT-DEFECTS.md) |
| BUG_011 | Web server allowed IPs load-only | **[STALE]** WebServer removed | [AUDIT-DEFECTS](AUDIT-DEFECTS.md) |
| GAP_001 | 3DES in SMTP SendMail | **[STALE]** SendMail removed | [AUDIT-SECURITY](AUDIT-SECURITY.md) |
| GAP_002 | `inet_addr()` deprecated API | **[DONE]** | [AUDIT-SECURITY](AUDIT-SECURITY.md) |
| GAP_003 | Potential XSS in web server templates | **[STALE]** WebServer removed | [AUDIT-SECURITY](AUDIT-SECURITY.md) |
| GAP_004 | 768-bit DH parameters (weak) | **[REJECTED]** | [AUDIT-SECURITY](AUDIT-SECURITY.md) |

### Code Review (CODEREV_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| CODEREV_001 | CaptchaGenerator SelectObject before NULL check | Open | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_002 | CaptchaGenerator `rand() & 8` bimodal jitter | Open | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_003 | Ring.h `m_pTail` initialized to UB pointer | **[DONE]** | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_004 | Ring.h `operator[]` no bounds check | **[DONE]** | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_005 | CreditsThread mask bitmap depth change | Open | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_006 | WebSocket.cpp ULONGLONG → UINT truncation | **[STALE]** WebSocket removed | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_007 | MBEDTLS_ALLOW_PRIVATE_ACCESS tech debt | **[STALE]** MbedTLS removed | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_008 | bmp2mem exception-unsafe | Open | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_009 | CaptchaGenerator local var named `m_LF` | Open | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_010 | BarShader CDC* → CDC& breaking change | Open | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_011 | Ring.h SetBuffer copy count bug | **[DONE]** | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |
| CODEREV_012 | CreditsThread DrawText `-1` length | Open | [AUDIT-CODEREVIEW](AUDIT-CODEREVIEW.md) |

### Bug Audit (BBUG_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| BBUG_001–007 | Packet/parser hardening | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_008–009 | delete-this in TryToConnect/re-ask | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_010–012 | Upload-entry two-phase retire | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_013 | Socket self-deletion in CClientReqSocket | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_014–016, 043 | GetCurrentServer() TOCTOU | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_017 | WebServer.cpp | **[STALE]** removed | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_018, 020, 026, 028–029, 035 | Audit-driven guard/test pass | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_019 | TaskbarNotifier double-free | **[STALE]** after inspection | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_021 | SendMail.cpp | **[STALE]** removed | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_022 | inet_ntoa() static buffer | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_023–025 | CList/CMap/CColourPopup patterns | **[STALE]** after inspection | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_027 | WebServer.cpp | **[STALE]** removed | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_030–034 | GDI/DC cleanup | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_036–041 | UI dialog crash hardening | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_042, 045–046 | Small runtime cleanup | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_044 | Runtime guard cleanup | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_047–048 | Resource cleanup (toolbar DC, captcha) | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_049 | UDPReaskFNF() client liveness | **[DONE]** | [AUDIT-BUGS](AUDIT-BUGS.md) |
| BBUG_050 | CDeletedClient destruction order | **[STALE]** after inspection | [AUDIT-BUGS](AUDIT-BUGS.md) |

### Kad Audit (AUD_KAD_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| AUD_KAD_001 | Default nodes.dat bootstrap still uses plain HTTP | **[REJECTED]** | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_002 | Add authenticated bootstrap sources | **[REJECTED]** | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_003 | Treat imported bootstrap contacts as probationary | **[REJECTED]** | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_004 | Keep and extend FastKad | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_005 | FastKad bootstrap ranking diversity-aware | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_006 | Same-IP rejection too blunt for CGNAT | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_007 | Evolve SafeKad toward layered trust | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_008 | Response-usefulness scoring | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_009 | Subnet-diversity controls and adaptive fanout | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_010 | Keep KadPublishGuard and PUBLISH_SOURCE validation | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_011 | Generalize expensive-op throttling | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_012 | Restore network-change grace handling | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_013 | Add Kad trust/budget/bootstrap counters | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_014 | Expand Kad integration/fuzz test coverage | Open | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_015 | Do not port eMuleAI multi-buddy logic | Guidance | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_016 | Do not port partial Kad IPv6 tag plumbing | Guidance | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_017 | Full dual-stack design if IPv6 Kad pursued | Guidance | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_018 | Borrow libtorrent storage-trust principle | Guidance | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_019 | IP/ID consistency as scoring signal only | Guidance | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_020 | Borrow explicit DHT resource budgets | Guidance | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_021 | No new mandatory Kad wire-level rules | Guidance | [AUDIT-KAD](AUDIT-KAD.md) |
| AUD_KAD_022 | Prioritize bootstrap trust, grace, budgets | Guidance | [AUDIT-KAD](AUDIT-KAD.md) |

### C++ Audit (CPP_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| CPP_001–010 | Language modernization — casts, arrays, RAII, constexpr, range-for, enum class, nullptr, auto | Open | [CPP-AUDIT](CPP-AUDIT.md) |
| CPP_011–020 | Standard library — MFC→std containers, string safety, algorithms, chrono, filesystem, random | **[PARTIAL]** (`CPP_012` **[PARTIAL]**) | [CPP-AUDIT](CPP-AUDIT.md) |
| CPP_021–030 | Threading — shared state, lock ordering, TOCTOU, atomics, CSingleLock, thread inventory, mutex | **[PARTIAL]** (`CPP_021`–`026`, `CPP_028`–`030` partially addressed, including bounded AICH maintenance wait cleanup; `CPP_027` **[DONE]**) | [CPP-AUDIT](CPP-AUDIT.md) |
| CPP_031–040 | Safety — unchecked returns, exception safety, buffer overflows, integer overflow, RAII, noexcept | **[PARTIAL]** (`CPP_031`–`038` partially addressed, including bounded `CPP_034` numeric hardening, AICH maintenance RAII cleanup, client part-status ownership hardening, credits/collection ownership hardening, and Win32 file-handle RAII hardening) | [CPP-AUDIT](CPP-AUDIT.md) |

### Dependencies (DEP_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| DEP_001 | Crypto++ | Keep (critical) | [DEP-REMOVAL](DEP-REMOVAL.md) |
| DEP_002 | id3lib | **[DONE]** Removed | [DEP-REMOVAL](DEP-REMOVAL.md) |
| DEP_003 | miniupnpc | Keep (easy to DLL-ify) | [DEP-REMOVAL](DEP-REMOVAL.md) |
| DEP_004 | zlib | Keep | [DEP-REMOVAL](DEP-REMOVAL.md) |
| DEP_005 | ResizableLib | Keep (low priority) | [DEP-REMOVAL](DEP-REMOVAL.md) |
| DEP_006 | Mbed TLS + TF-PSA-Crypto | **[DONE]** Removed | [DEP-REMOVAL](DEP-REMOVAL.md) |

### WWMOD (WWMOD_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| WWMOD_001–005 | Dead Win9x/NT4/MSVC guards | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_006 | Dynamic loading of always-present Win10 APIs | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_007 | Dead _WINSOCK_DEPRECATED_NO_WARNINGS | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_008 | _CRT_SECURE_NO_DEPRECATE blanket suppression | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_009 | WinNT/Win98 comments in active code | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_010 | No DPI awareness (P0) | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_011 | Legacy MFC list controls (no virtual mode) | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_012 | Legacy CPropertySheet preferences | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_013 | Historical WSAAsyncSelect networking model | **[PARTIAL]** helper-window migration done; IOCP/soak follow-up remains | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_014–015 | MFC sync primitives, AfxBeginThread | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_016–017 | Custom type aliases, MFC containers | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_018 | GDI-only drawing (no Direct2D) | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_019 | Unsafe string formatting functions | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_020 | MD4 as primary file hash | Open (protocol) | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_021 | No C++ language standard specified | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_022–023 | #define → constexpr, no /permissive- | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_024 | MAX_PATH (260) path length limit | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_025 | IPv4-only networking | Open (protocol) | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_026–027 | 9.28 MB part size, 256 GB max file size | **[REJECTED]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_028 | Conservative connection/queue limits | **[PARTIAL]** via FEAT_013–019 | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_029 | 384-bit RSA keys | **[REJECTED]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_030 | Deprecated Winsock APIs | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_031 | _T()/TCHAR dual-path macros | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_032 | No ETW/structured logging | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_033 | Binary file formats with limited versioning | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_034 | No memory-mapped I/O for large files | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_035 | UpgradeFromVC71.props import | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_036 | Static MFC linking | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_037 | No ASan/static analysis in CI | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_038 | Delay-loaded DLLs that are always present | **[DONE]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_039 | qedit.h bundled header | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_040 | ResizableLib third-party dep | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_041 | No accessibility support | **[REJECTED]** | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_042 | CAsyncSocketEx custom socket library | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_043 | Pinger.cpp raw ICMP | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_044 | No HTTP/2 or modern TLS for HTTP downloads | Open | [AUDIT-WWMOD](AUDIT-WWMOD.md) |
| WWMOD_045–050 | CRichEditCtrl log, taskbar progress, toast, VC_EXTRALEAN, OmitFramePointers, time macros | **[PARTIAL]** — WWMOD_046 stale, WWMOD_049 done | [AUDIT-WWMOD](AUDIT-WWMOD.md) |

### Plans (PLAN_)

| ID | Summary | Status | Doc |
|---|---|---|---|
| PLAN_001 | Module restructuring (srchybrid/ folder split) | Planning | [PLAN-RESTRUCTURE](PLAN-RESTRUCTURE.md) |

---

## Progress Summary

| Category | Total | Done | Partial | Stale | Open |
|---|---|---|---|---|---|
| CPP_ | 40 | 1 | 18 | 0 | 21 |
| REFAC_ | 18 | 12 | 1 | 0 | 5 |
| FEAT_ | 22 | 3 | 4 | 0 | 15 |
| BUG_/GAP_ | 8 | 3 | 0 | 3 | 2 |
| CODEREV_ | 12 | 3 | 0 | 2 | 7 |
| BBUG_ | 50 | 42 | 0 | 8 | 0 |
| AUD_KAD_ | 22 | 0 | 0 | 0 | 22 |
| DEP_ | 6 | 2 | 0 | 0 | 4 |
| WWMOD_ | 50 | 9 | 1 | 0 | 40 |

---

*End of index.*
