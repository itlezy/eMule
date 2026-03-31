# eMule 2026 — Full Modernization Engineering Roadmap

**Audience:** Engineering team
**Horizon:** 12 months of sustained effort
**Baseline:** eMule 0.72a + current eMulebb `v0.72a-broadband-dev` branch
**Reference material:** eMuleAI fork, eMule-mods-archive (24 mods, 2006–2016)
**Date:** 2026-03-29

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Current State Inventory](#2-current-state-inventory)
3. [Modernization Pillars](#3-modernization-pillars)
4. [Pillar A — Build, Toolchain & Platform](#pillar-a--build-toolchain--platform)
5. [Pillar B — Core Architecture & Threading](#pillar-b--core-architecture--threading)
6. [Pillar C — Network Protocol & Connectivity](#pillar-c--network-protocol--connectivity)
7. [Pillar D — Kademlia DHT Overhaul](#pillar-d--kademlia-dht-overhaul)
8. [Pillar E — Upload & Download Engine](#pillar-e--upload--download-engine)
9. [Pillar F — Anti-Leecher, Spam & Trust Systems](#pillar-f--anti-leecher-spam--trust-systems)
10. [Pillar G — File Integrity & Storage](#pillar-g--file-integrity--storage)
11. [Pillar H — Search & Metadata](#pillar-h--search--metadata)
12. [Pillar I — Web Server & REST API](#pillar-i--web-server--rest-api)
13. [Pillar J — UI & User Experience](#pillar-j--ui--user-experience)
14. [Pillar K — Security & Cryptography](#pillar-k--security--cryptography)
15. [Pillar L — AI / ML Integration](#pillar-l--ai--ml-integration)
16. [Pillar M — Observability & Diagnostics](#pillar-m--observability--diagnostics)
17. [Pillar N — Configuration & Preferences](#pillar-n--configuration--preferences)
18. [Pillar O — Packaging, Distribution & CI/CD](#pillar-o--packaging-distribution--cicd)
19. [Cross-cutting Concerns](#19-cross-cutting-concerns)
20. [12-Month Sprint Plan](#20-12-month-sprint-plan)
21. [Risk Register](#21-risk-register)
22. [Success Metrics](#22-success-metrics)

---

## 1. Executive Summary

eMule is a 24-year-old C++ MFC application that, remarkably, still serves millions of users on the eD2K/Kademlia P2P network. Its core protocol is battle-tested, its credit system is elegant, and its Kademlia DHT implementation is production-grade. But the surrounding application code is showing its age: a single-threaded UI/network hybrid, MD5-hashed passwords, `rand()`-based session IDs, `CONNECTION: close` on every HTTP request, no JSON API, and dozens of conservative defaults tuned for dial-up modems.

The goal of this roadmap is **not** to rewrite eMule. It is to surgically modernize it across 15 engineering pillars while keeping the network protocol compatible, the user data safe, and the build green. Every item below is grounded in one of three sources:

- **eMulebb** — active work already in progress on `v0.72a-broadband-dev`
- **eMuleAI** — the leading modern fork (IPv6, dark mode, virtual lists, Shield, uTP, NAT traversal, nlohmann/json, GeoLite2, embedded MediaInfo)
- **Mods archive** — 24 mods spanning 2006–2016, representing ten years of community innovation (MorphXT, Mephisto, UltiMatiX, StulleMule, NeoMule, Xtreme, ScarAngel, …)

Where we have proven implementations to port from, we say so. Where we are proposing genuinely new work, we say that too.

---

## 2. Current State Inventory

### What eMulebb has already done (do not redo)

| Area | Status |
|------|--------|
| VS 2022 + v143 toolset, x64 primary | ✅ Done |
| Windows 10+ baseline (XP/Vista shims removed) | ✅ Done |
| ~~TLS 1.3 via mbedTLS 4.0 + PSA Crypto~~ | Removed (web server + SMTP purged, commit `6a1c440`) |
| CxImage/libpng → GDI+ | ✅ Done |
| IOCP async file writes (PartFileWriteThread) | ✅ Done |
| Async overlapped upload disk reads | ✅ Done |
| PeerCache removed | ✅ Done |
| MiniMule / IE-hosted mini window removed | ✅ Done |
| ARM64 manifest | ✅ Done |
| CList → CRing for rate tracking | ✅ Done |
| Broadband upload slot controller (BBMaxUpClientsAllowed) | ✅ Done |
| IP2Country via MMDB | ✅ Done |
| MediaInfo refactored into per-format files | ✅ In progress |
| Preferences documentation (40 keys) | ✅ Done |
| DEFECTS.md catalogued | ✅ Done |
| firewall-opener.ps1 / enable-long-paths.ps1 | ✅ Done |

### Known defects to fix first (from DEFECTS.md / CODEREVIEW.md)

| # | Defect | File | Severity |
|---|--------|------|----------|
| D-01 | `strcpy()` (single remaining unbounded call) | `Emule.cpp:844` | HIGH |
| D-02 | `srand(time(NULL))` weak RNG seed | `Emule.cpp` | HIGH |
| D-03 | `rand()` for crypto challenge value | `ClientUDPSocket.cpp` | HIGH |
| D-04 | `inet_addr()` deprecated **[DONE]** | 2 socket files | MEDIUM |
| D-05 | Upload overhead counters don't persist (INI key mismatch) | `Preferences.cpp` | MEDIUM |
| ~~D-06~~ | ~~Web server allowed IPs not saved back~~ | ~~`PPgWebServer.cpp`~~ | **STALE** — removed |
| D-07 | 19+ hidden prefs loaded but not saved | `Preferences.cpp` | MEDIUM |
| ~~D-08~~ | ~~`MBEDTLS_ALLOW_PRIVATE_ACCESS` technical debt~~ | ~~`WebSocket.cpp`~~ | **STALE** — removed |
| D-09 | GDI handle leak in CAPTCHA generator | `CaptchaGenerator.cpp` | LOW |
| D-10 | UNC share prepend missing in sorted insertion | `SharedFileList.cpp` | LOW |

---

## 3. Modernization Pillars

The 15 pillars below are grouped by engineering domain. Each pillar has:
- **Why** — motivation
- **What** — concrete deliverables
- **How** — implementation approach, with code references or port targets
- **Effort** — T-shirt size (S/M/L/XL)
- **Dependencies** — prerequisites

---

## Pillar A — Build, Toolchain & Platform

### A-01 ARM64 Native Build

**Why:** Windows-on-ARM laptops (Snapdragon X Elite) are shipping mainstream. The v0.72a tag added ARM64 detection but the eMulebb branch removed it to reduce scope. It belongs back in.

**What:**
- Add `ARM64` configuration to `emule.vcxproj`
- Audit all x86-specific intrinsics (`__cpuid`, MSVC `__int64` alignment assumptions)
- Ensure mbedTLS 4.0 ARM64 is built correctly (PSA Crypto has ARM assembly paths)
- Add ARM64 CI runner

**How:**
```xml
<!-- emule.vcxproj — add configuration -->
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|ARM64'">
  <PlatformToolset>v143</PlatformToolset>
  <WholeProgramOptimization>true</WholeProgramOptimization>
</PropertyGroup>
```

**Effort:** M
**Dependency:** A-02

---

### A-02 CMake as Parallel Build System

**Why:** `emule.vcxproj` is opaque to non-MSVC toolchains (LLVM/clang-cl, vcpkg integration). CMake enables reproducible builds, better dependency management, and future Linux/Wine cross-compilation.

**What:**
- `CMakeLists.txt` at repo root
- `cmake/FindCryptoPP.cmake`, `cmake/FindMbedTLS.cmake`, etc.
- Keep `.vcxproj` as primary for IDE users; CMake is the CI path
- vcpkg manifest (`vcpkg.json`) for dependency pinning

**vcpkg.json skeleton:**
```json
{
  "name": "emule",
  "version": "0.72.1",
  "dependencies": [
    "cryptopp",
    "mbedtls",
    "miniupnpc",
    "zlib",
    "nlohmann-json",
    "libmaxminddb",
    "libutp"
  ]
}
```

**Effort:** L
**Dependency:** none

---

### A-03 Static Analysis & Sanitizers in CI

**Why:** 24-year-old codebases have latent memory issues. Address/thread sanitizers catch them before users do.

**What:**
- PVS-Studio or clang-tidy configuration (`.clang-tidy`)
- AddressSanitizer build configuration for debug
- UBSan (undefined behaviour sanitizer)
- GitHub Actions workflow (or equivalent) gating PRs

**Effort:** M
**Dependency:** A-02

---

### A-04 Windows Long-Path & Unicode Path Hardening

**Why:** eMulebb added `enable-long-paths.ps1` but the code still uses `MAX_PATH`-sized `TCHAR` arrays in many file operations.

**What:**
- Audit all `TCHAR szPath[MAX_PATH]` declarations
- Replace with `std::wstring` or heap-allocated buffers where paths may exceed 260 chars
- Use `\\?\` prefix for all file operations via a helper `fs::path ToExtendedPath(const std::wstring&)`
- Regression: verify download paths with 300-char names work end-to-end

**Effort:** M
**Dependency:** none

---

## Pillar B — Core Architecture & Threading

This is the highest-risk, highest-reward pillar. The eMule main thread currently drives **everything**: network events, disk I/O scheduling, GUI repaints, Kademlia routing, upload slot decisions, and the web server. The 100 ms `UploadQueue` timer heartbeat cascades into all other subsystems.

### B-01 Separate Network Thread from UI Thread

**Why:** Any slow disk operation, GUI redraw, or plugin callback stalls the entire network stack. On Windows 11 with NVMe + fast Ethernet, this is the primary cause of throughput variance.

**What:**
- Create `CNetworkThread` owning all socket `Select()`/`IOCP` operations
- Main UI thread communicates via a lock-free MPSC queue (`CNetworkMessage`)
- Network thread posts to UI via `PostMessage()` (already used for web server; extend to full network layer)
- Initial scope: TCP client connections + UDP socket. Kademlia UDP listener is a natural second candidate.

**Architecture:**
```
UI Thread (MFC message loop)
    ↕ PostMessage / SendMessage
Network Thread (owns select/IOCP)
    ↕ shared data via CNetworkMessage queue
Upload/Download Engine (can run on Network Thread)

Separate worker threads (already exist):
    PartFileWriteThread
    UploadDiskIOThread
    CAddFileThread (hashing)
    WebSocketListeningFunc
```

**How:** Port the pattern from eMuleAI's `ReadWriteLock.cpp/h` for shared-state access. The `CRing<>` already provides lock-free reading for rate stats.

**Effort:** XL (6–8 weeks, two engineers)
**Dependency:** D-01 through D-04 fixed first; B-02

---

### B-02 Lock-Free Rate Statistics

**Why:** The `CRing<>` buffers for rate tracking are already introduced but accessed without synchronisation once B-01 separates threads.

**What:**
- Wrap `CRing<>` reads in `std::atomic` snapshot pattern
- Or promote to a dedicated `CAtomicRing<>` using `std::atomic<size_t>` write index
- Rate aggregation functions (`GetDataRate()`, `GetAvgUploadRate()`) become safe for cross-thread calls

**Effort:** S
**Dependency:** B-01

---

### B-03 Thread Pool for Background Work

**Why:** Each file hash, AICH computation, video frame grab, and shared-file rescan spawns an ad-hoc `CWinThread`. Under heavy load (hundreds of shared files rescanned) this creates hundreds of threads.

**What:**
- Introduce a `CThreadPool` (port `ThreadpoolWrapper.h` from eMuleAI) wrapping Windows Thread Pool API (`CreateThreadpoolWork`)
- Route: hashing tasks, AICH sync, media info extraction, geo-lookup
- Keep dedicated long-running threads (WriteThread, UploadDiskIO) as they are

**Effort:** M
**Dependency:** B-01

---

### B-04 Event-Driven Scheduler (Replace 100 ms Timer)

**Why:** The `UploadQueue` timer fires every 100 ms regardless of load. On an idle client with no transfers it wastes CPU; under heavy load, 100 ms is too coarse for responsive slot control.

**What:**
- Replace with a priority queue of `ScheduledEvent{timestamp, callback}`
- Process on Network Thread: dequeue events whose `timestamp <= now`
- Slot decisions fire immediately on upload completion events rather than waiting up to 100 ms

**Effort:** L
**Dependency:** B-01

---

## Pillar C — Network Protocol & Connectivity

### C-01 Full IPv6 Support

**Why:** ISPs increasingly assign IPv6-only or DS-Lite addresses. LowID on IPv4 + HighID on IPv6 is a real topology. eMuleAI has early-alpha IPv6; the mods archive's NeoMule/neomule_reloaded pioneered this.

**What:**
- Port `eMuleAI/Address.cpp/h` (16 KB, full IPv6 address abstraction replacing all `uint32` IP fields)
- Update `CPartFile::AddSources()` to accept IPv6 source entries
- Update `CClientList` to key on `(IPv6Addr, port)` pairs
- Update server connection logic (eD2K servers are IPv4-only; Kad nodes can be IPv6)
- Update `CUpDownClient` handshake to negotiate IPv6 capability
- Update web interface to display IPv6 addresses
- Dual-stack listening socket (bind to `::` for both IPv4 and IPv6)
- Update IP filter to handle CIDR IPv6 ranges

**Protocol notes:**
- eD2K client-to-client: new optional tag `CT_EMULE_IPV6ADDR` (already in eMuleAI)
- Kademlia: contact records extended to carry IPv6 address (see Pillar D)

**Effort:** XL (8–10 weeks)
**Dependency:** D-04 (`inet_addr` → `InetPton`)

---

### C-02 uTP (Micro Transport Protocol) Transport

**Why:** uTP runs over UDP, self-throttles to avoid congesting TCP traffic, and works through more NATs. BitTorrent switched to uTP ~2010; eMule has no equivalent. eMuleAI includes `eMuleAI/UtpSocket.cpp/h`.

**What:**
- Integrate `libutp` (already in eMuleAI's dependency list)
- Add `CUtpClientSocket` alongside `CClientReqSocket` (TCP)
- Negotiate uTP capability via existing client hello extension bits
- Fall back to TCP if uTP handshake fails
- Upload/download can mix TCP and uTP clients in the same session

**Effort:** XL (6–8 weeks)
**Dependency:** B-01 (network thread), C-01 (unified address type)

---

### C-03 NAT Traversal — eServer Buddy Path

**Why:** LowID ↔ LowID connections currently fail silently. eMuleAI implemented a **client-side eServer buddy path** that does NOT require any Lugdunum NAT opcodes — i.e., it works with any existing server.

**What (port from eMuleAI):**
- `CSafeKad.cpp/h` — safe Kademlia operations during NAT events
- `CConChecker.cpp/h` — connectivity tester
- eServer buddy path: LowID client A registers itself on a shared server as a buddy for LowID client B; HighID peers serve up to 100 LowID clients (configurable, default 3)
- Keep-alive with rate limiting
- External UDP port discovery (punched holes)
- Stronger hole-punch + uTP retry logic

**Effort:** L (3–4 weeks, can port directly from eMuleAI)
**Dependency:** C-01, C-02

---

### C-04 HTTP Source Support (Hardening)

**Why:** HTTP sources (direct URL sources for partial files) work in principle but have edge cases with redirects, range request failures, and slow servers that block slots.

**What:**
- Timeout HTTP sources if they don't respond within 30 s
- Handle `302`/`307` redirects up to 3 hops
- Honour `Retry-After` header
- Decouple HTTP source download from eMule slot system (HTTP sources don't consume eD2K upload slots)
- Surface HTTP source count separately in UI

**Effort:** M
**Dependency:** none

---

### C-05 Server Connection Resilience

**Why:** eMule connects to one eD2K server at a time and silently stalls if it fails. The archive's AcKroNiC and UltiMatiX both implement automatic server list looping.

**What:**
- Automatic failover to next server on connection failure (configurable attempt count before rotating)
- Exponential backoff per server (track `lastFailedAt`, `failureCount`)
- Prefer servers with high user count and low ping
- "Repeat Server List" feature (loop back to top after exhausting list)
- Surface connectivity state in status bar: `[Server: Example | 12 s | retrying…]`

**Effort:** S
**Dependency:** none

---

### C-06 UPnP / NAT-PMP / PCP Modernization

**Why:** miniupnpc is already used, but NAT-PMP (for Apple AirPort and some Huawei routers) and its successor PCP (Port Control Protocol, RFC 6887) are not implemented.

**What:**
- Add NAT-PMP client (libnatpmp is tiny, ~800 lines)
- Try UPnP first, then NAT-PMP, then PCP, report whichever succeeds
- Re-map on IP change (already partially in eMuleAI's IP change handler)
- Expose mapping result in status bar tooltip

**Effort:** M
**Dependency:** none

---

## Pillar D — Kademlia DHT Overhaul

### D-01 Kademlia IPv6 Extension

**Why:** IPv6 Kad nodes can reach peers that IPv4-only Kad cannot. This is the Kad analogue of C-01.

**What:**
- Extend `CContact` to carry an optional IPv6 address
- Update routing zone splits to consider IPv6 addresses
- Dual Kad ID space (maintain a single Kad ID but store contacts in per-family buckets)
- `KademliaUDPListener` binds on both `0.0.0.0:4672` and `[::]:4672`

**Effort:** L
**Dependency:** C-01

---

### D-02 Kad Search Limit & Quality Control

**Why:** Uncontrolled Kad searches waste bandwidth and return low-quality results when running in parallel.

**What (from eMuleAI):**
- Configurable `KadSearchLimit` — max answers before auto-stop (default 200)
- Deduplication of results by hash before surfacing to search list
- Spam Rating integration (see Pillar F): Kad results with low trust score are demoted
- Search job priority queue: interactive user searches run before background source lookups

**Effort:** S
**Dependency:** none

---

### D-03 Fast Kad / Safe Kad [DONE: `4798953`]

**Why:** Cold-start Kad bootstrap is slow (can take 5+ minutes to reach `Connected` state).

**What (port from eMuleAI's FastKad.cpp/h):**
- Learn Kad response times and use them to shorten or extend pending search timeout cleanup
- Persist Fast Kad bootstrap-priority metadata in a sidecar beside `nodes.dat`
- Seed the startup bootstrap queue from the best recent healthy contacts loaded from the routing table
- Track Kad node identity by `IP:UDPPort`, reject conflicting IDs, and temporarily ban verified fast-flipping identities when enabled
- Clear Fast Kad / Safe Kad caches on Kad reconnect-style firewall rechecks and full shutdown
- Expose Kad bootstrap progress as status-bar text while the bootstrap list is being consumed

**Effort:** M
**Dependency:** none

---

### D-04 Kademlia Flood Protection [DONE: `4798953`]

**Why:** Malicious nodes can flood a Kad ID with fake publish operations, poisoning source lists.

**What:**
- Rate-limit incoming `KADEMLIA2_PUBLISH_SOURCE_REQ` per source IP
- Validate published sources, including source type, source port, and low-ID buddy metadata completeness
- Track abusive senders through Safe Kad and expire matching routing contacts when the publish flood threshold escalates to a ban
- Add configurable flood-threshold and bad-node-ban toggles in advanced preferences

**Effort:** M
**Dependency:** none

---

### D-05 nodes.dat Management & Bootstrap URL [DONE: `0877a46`]

**Why:** `nodes.dat` becomes stale. Users with a cold/empty `nodes.dat` cannot bootstrap.

**What:**
- Configurable bootstrap nodes URL (HTTP fetch of a fresh `nodes.dat` on first launch or when stale)
- From eMuleAI / UltiMatiX: `NodesDateUpdateUrl` preference
- Background fetch, atomic replace of `nodes.dat`
- Show timestamp of `nodes.dat` in Kademlia dialog
- Validate downloaded `nodes.dat` candidates before replacing the persisted file
- Keep the Fast Kad sidecar separate and preserve dormant bootstrap metadata across imported/downloaded `nodes.dat` changes

**Effort:** S
**Dependency:** none

---

## Pillar E — Upload & Download Engine

### E-01 Broadband Upload Slot Controller (Complete the eMulebb Work)

**Why:** eMulebb's `BBMaxUpClientsAllowed` controller is in progress. Needs hardening and UI exposure.

**What:**
- **Slot targeting:** `target_bytes_per_slot = effective_budget / slot_count`
- **Slow-slot eviction:** drop slots where `bytes_this_tick < target * 0.2` for > N consecutive ticks
- **Cooldown tracking:** evicted clients enter a cooldown set (configurable duration); not re-admitted until cooldown expires
- **Session caps:** `BBSessionMaxTrans` (e.g., 64 GiB) and `BBSessionMaxTime` (e.g., 3 h)
- **All-time ratio column** in upload list and queue list
- **Trickle slot management:** from Mephisto — dedicate 1 trickle slot to ensure every queued client gets some data (prevents starvation)
- **Queue score bias:** low-ratio files get score multiplier; LowID clients get minor bonus

**Effort:** M (mostly wiring existing work into UI)
**Dependency:** B-01 (for accurate rate measurement)

---

### E-02 Multi-Chunk Upload (Port from Mephisto / UltiMatiX)

**Why:** Sending a single chunk per session means slow clients receive only one 180 KB chunk before being rotated. Multi-chunk reduces session overhead.

**What (from Mephisto v3.0 and UltiMatiX):**
- Mode 1: Disabled (vanilla)
- Mode 2: Xtreme Full Chunk — send as many complete chunks as fit in the slot time
- Mode 3: Finish X chunks max (configurable 1–9, each ~9.28 MB)
- Mode 4: Upload X chunks then rotate regardless

**Effort:** M
**Dependency:** E-01

---

### E-03 PowerShare (Priority File Boosting)

**Why:** PowerShare appears in MorphXT, UltiMatiX, ScarAngel, AcKroNiC — it's the most-requested feature in the community. Lets users mark specific files for boosted upload priority without breaking credit fairness.

**What:**
- Per-file `PowerShare` flag in UI (right-click → "Enable PowerShare")
- Shared file list: optional pink/bold colouring for PS files (from UltiMatiX)
- Score modifier: PS files get `score × PSBoostFactor` (configurable 2–500 from UltiMatiX)
- Limit PS to `% of file size uploaded per session` (prevents abuse)
- Stats collection: PS upload bytes tracked separately

**Effort:** M
**Dependency:** E-01

---

### E-04 A4AF (Ask For Another File) Swapping

**Why:** When a source has both a queued file and a partially downloaded file, eMule wastes the connection re-asking for the queued file. A4AF swaps the source to the file where it can actually help.

**What (from MorphXT):**
- Detect when a source is offering an A4AF candidate with better completion for the target file
- Auto-swap sources between files when `completion_target > completion_current + threshold`
- Configurable threshold and cooldown period
- A4AF swap events logged to debug log

**Effort:** M
**Dependency:** none

---

### E-05 Download Checker (Duplicate Detection)

**Why:** Users accidentally queue the same content under different hashes (different encodes, different rips). eMuleAI's `DownloadChecker.cpp/h` catches this.

**What (port from eMuleAI):**
- On adding a download, check known.met + current download list for:
  - Exact hash match (trivial)
  - Same filename + similar size (warn)
  - Same size + similar name (fuzzy match via edit distance)
- Configurable action: Warn / Reject / Auto-blacklist
- Surface warnings as a non-modal toast notification

**Effort:** S
**Dependency:** none

---

### E-06 PBF — Push Before Finish

**Why:** Partial files with only a few chunks remaining are hard to complete because few sources have exactly those chunks. PBF pushes these files to the front of the queue.

**What (from UltiMatiX / Mephisto):**
- Configurable PBF threshold: when `remaining_chunks <= N`, boost file priority automatically
- Separate thresholds for part files (PBF_PARTS) and nearly-finished files (PBF_FINISH)
- Visual indicator in download list when PBF is active
- Revert priority to normal after completion

**Effort:** S
**Dependency:** none

---

### E-07 File Buffer Management — HDD Protect Mode

**Why:** The default file write buffer is tuned for spinning disks. NVMe drives can absorb much larger writes, and SSDs prefer larger sequential writes to minimize write amplification.

**What (from UltiMatiX, up to 40 MB buffer with 500 KB steps):**
- Configurable max file buffer: 1 MB to 256 MB (step 1 MB via slider)
- Auto-detect drive type (`DeviceIoControl(IOCTL_STORAGE_QUERY_PROPERTY)`) and set default:
  - HDD: 4 MB
  - SSD: 16 MB
  - NVMe: 32 MB
- Buffer time limit: 1–30 minutes (flush if buffer age exceeds limit)
- Separate buffer per part file (already partially in IOCP thread; configure per-file)

**Effort:** M
**Dependency:** A-04

---

### E-08 Upload Bandwidth Throttle (UBT) Per Client

**Why:** Some clients have great credit but slow connections that waste slot time. UBT caps individual client transfer rates.

**What (from UltiMatiX):**
- Per-client upload bandwidth cap (in KB/s)
- Configurable via client detail dialog
- Persists per `UserHash` in a small SQLite side-table (or a new `.dat` file)
- Shown in upload list as `[UBT: 100 KB/s]`

**Effort:** M
**Dependency:** none

---

## Pillar F — Anti-Leecher, Spam & Trust Systems

The mod archive contains the most sophisticated anti-leecher ecosystem ever built for eMule. eMuleAI's `Shield.h/cpp` consolidates much of it. This pillar ports it properly.

### F-01 Shield — Comprehensive Leecher Detection Engine

**Why:** Leechers consume upload slots without contributing. eMuleAI's Shield has 74 detection categories and 10 punishment types.

**What (port from eMuleAI's `Shield.h/cpp`):**

**Detection categories (selected critical ones):**
- Mod faker: client claims to be `eMule` but version string mismatches
- Hash thief: client uses a stolen user hash (checked against known leecher hash list)
- Username changer: client changes username between sessions
- Non-SUI: client does not send Secure User Identification
- Block-ratio abuser: client has `upload_ratio < 0.01` (configurable floor)
- TCP error flooder: client generates excessive TCP errors
- XS exploiter: client abuses Extended Source Exchange to drain source lists
- File faker: client sends fake file data (AICH verification fails)
- Upload faker: client claims to have uploaded but AICH blocks don't match
- Community abuser: client is in a known leecher community list

**Punishment types:**
1. Full IP + UserHash ban (configurable duration)
2. UserHash-only ban (allows reconnect from same IP)
3. Upload ban (no slots offered)
4. Score reduction: `score × factor` (factors: ×0.1, ×0.2, ×0.5, ×0.75)
5. Log only (for investigation)
6. Friend exemption (friends always bypass Shield)

**UI:**
- `Protection Panel` page in Preferences
- Per-category toggle + punishment selection
- Ban duration slider (1 min to 24 h)
- "Detected Leechers" counter in status bar

**Effort:** L (port is large but well-defined)
**Dependency:** none

---

### F-02 DLP — Donkey Leecher Program List Integration

**Why:** DLP is the community-maintained list of known leecher client signatures. MorphXT, ScarAngel, UltiMatiX all use it.

**What:**
- Load `anti-leecher.dat` (DLP format, community-maintained)
- HTTP auto-update check (like IP filter update, daily)
- Match against client `ModString`, `ClientSoft`, and version fields
- Configurable action per DLP category

**Effort:** S
**Dependency:** F-01

---

### F-03 Spam Rating & Blacklist Panel

**Why:** Search results contain junk. eMuleAI's spam rating (0–100) and blacklist panel let users define quality thresholds.

**What (port from eMuleAI):**
- `Spam Rating` column in search results
- Customizable threshold: results below threshold are hidden by default
- `Blacklist Panel` in Preferences with category editor
- "Recheck now" button reapplies blacklist to current search without re-searching
- Surface as optional column: `PPgBlacklistPanel.cpp/h`

**Effort:** M
**Dependency:** none

---

### F-04 Source Caching

**Why:** On restart, eMule discards all source information. Reconnecting to sources that were already known wastes time.

**What (port from eMuleAI's `SourceSaver.cpp/h`):**
- Persist source lists for active downloads to `sources.dat` on controlled shutdown
- Reload on startup: re-ask sources before starting fresh Kad/server lookup
- Prune stale sources older than 24 h (configurable)
- Separate from `known.met` — sources.dat is transient, not part of the permanent record

**Effort:** M
**Dependency:** none

---

### F-05 IP Filter Hardening

**Why:** Multiple mods (Xtreme, UltiMatiX, MorphXT) found and fixed IP filter bugs: truncation on save, missing whitelist support, no IPv6 range support.

**What:**
- Fix save truncation (write to temp file, atomic rename)
- Add whitelist file (`ipfilter_whitelist.dat`) that overrides blacklist matches
- CIDR range support for IPv4 and IPv6 entries
- Static IP filter file support (never auto-updated, for admin-managed environments)
- PeerGuardian `*.p2p` file import (from eMuleAI)
- Configurable private IP filtering for home/lab networks

**Effort:** M
**Dependency:** C-01

---

### F-06 Client History

**Why:** eMuleAI's client history enables "once per client" automation (e.g., shared-files query sent only once per client per day).

**What (port from eMuleAI):**
- Lightweight SQLite table: `(user_hash TEXT, first_seen INTEGER, last_seen INTEGER, shared_files_queried INTEGER)`
- Configurable retention period (7–90 days)
- Used by: once-per-client shared-file queries, leecher history, ban audit trail

**Effort:** S
**Dependency:** none

---

## Pillar G — File Integrity & Storage

### G-01 AICH Hashset Optimisation

**Why:** Large shares (10,000+ files) cause `known2_64.met` to grow huge and slow to load. Xtreme mod introduced several mitigations.

**What (from Xtreme v8.1 and MorphXT):**
- **Split known2_64.met**: configurable split threshold (e.g., 1 GB) into `known2_64.part.001.met`, etc.
- **AICH write buffer**: batch writes (configurable 512 KB–4 MB) to reduce disk I/O on large shares
- **PartiallyPurgeOldKnownFiles**: remove AICH entries for files not seen in N days (default 60)
- **Remember unused AICH hashes**: keep orphaned hashes for 30 days in case the file re-appears

**Effort:** M
**Dependency:** none

---

### G-02 Threaded known.met / file settings Save

**Why:** Saving `known.met` on a large share (100,000+ files) blocks the UI for seconds. StulleMule implemented threaded saving.

**What (port from StulleMule v7.0):**
- Serialize known.met to a memory buffer on the main thread (fast, no disk I/O)
- Hand buffer to a dedicated `CKnownFileSaveThread`
- Atomic rename: write to `known.tmp`, rename to `known.met`
- Same pattern for `preferences.ini` (safe config saving from eMuleAI)

**Effort:** M
**Dependency:** none

---

### G-03 Archive Recovery

**Why:** eMuleAI includes `ArchiveRecovery.cpp/h` — ability to recover partial downloads of archive files.

**What:**
- For ZIP/RAR/7z partial downloads: attempt recovery of whatever complete segments are available
- Surface in download context menu: "Try Archive Recovery"
- Show recovered file count
- Port from eMuleAI (already implemented)

**Effort:** S
**Dependency:** none

---

### G-04 FakeAlyzer — File Authenticity Detection

**Why:** MorphXT's FakeAlyzer detects files with mismatched extension/content (e.g., a `.mkv` that is actually a `.exe`).

**What (port from MorphXT v12.7):**
- Read file header magic bytes after download
- Compare against extension database (200+ format signatures)
- Flag mismatches with ⚠ icon in download list
- Auto-quarantine option: move suspicious files to a quarantine folder
- Configurable: warn / quarantine / delete

**Effort:** M
**Dependency:** none

---

### G-05 SIVKA Per-File Settings

**Why:** Different files have different optimal settings (priority, push mode, PowerShare, etc.). SIVKA format persists these per file.

**What (from UltiMatiX, AcKroNiC):**
- Extend per-file metadata in `known.met` or a sidecar `emule-settings.sqlite`
- Settings: priority, PowerShare flag, push mode, file reask time, custom comment
- UI: file settings dialog accessible from download/shared list context menu
- "Follow the Majority" toggle per file (from StulleMule): inherit category settings

**Effort:** M
**Dependency:** none

---

## Pillar H — Search & Metadata

### H-01 Search Window Virtual List & Filtering

**Why:** Search results in vanilla eMule are a flat sorted list; 10,000 results make the UI unusable.

**What (port from eMuleAI's virtual list implementation):**
- Owner-data `CListCtrl` (virtual list) for search results
- On-view filter bar: type-to-filter by filename substring
- Active-tab-only refresh (eMuleAI): only the visible tab is updated on new results
- Cleanup button: remove blacklisted/spam-rated results
- Spam Rating column (from Pillar F-03)
- Client Version, ID Type columns
- Multiple column sort

**Effort:** M
**Dependency:** F-03

---

### H-02 MediaInfo — Full Embedded Library

**Why:** eMuleAI embeds MediaInfo directly (no external DLL). eMulebb refactored MediaInfo into per-format files but still probes for the DLL.

**What:**
- Embed MediaInfoLib as a static library (already in eMuleAI)
- Eliminates DLL probe dance (`MediaInfo_RIFF.cpp` etc. become wrappers over the embedded lib)
- Support modern codecs: AV1, VP9, HEVC, H.264, OPUS, FLAC
- Surface MediaInfo results in file properties dialog with copy-to-clipboard

**Effort:** M
**Dependency:** A-02 (vcpkg for MediaInfoLib)

---

### H-03 Collection Handling Fixes

**Why:** MorphXT v12.7 documents a collection double-extension bug and fakes-list handling for files >4 GB.

**What:**
- Fix double-extension: `file.mp4.emulecollection` should not be served as `file.mp4`
- Fix `uint32` overflow in fake-size detection for files >4 GB (use `uint64` throughout)
- Collection file size validation before accepting

**Effort:** S
**Dependency:** none

---

### H-04 Ed2k Link Extended Metadata

**Why:** ed2k links can carry metadata (filename, size, AICH root hash, sources) but eMule doesn't always parse all of them.

**What:**
- Parse `|sources|` part of ed2k links (pre-load sources before querying network)
- Parse `|AICH|` part if present (skip AICH computation phase for well-known files)
- Friend link recognition (from eMuleAI): detect `|friend|` links from clipboard

**Effort:** S
**Dependency:** none

---

## Pillar I — Web Server & REST API

This builds directly on `WEBSERVER.md` and `WEB_APIs.md` in the analysis repo.

### I-01 Thread Safety (Critical Prerequisite)

**Why:** All web server threads read `theApp.*` without locks. This is documented in the source as a known problem.

**What:**
- Introduce `CCriticalSection g_csWebAccess`
- Wrap every `theApp.downloadqueue`, `theApp.uploadqueue`, `theApp.sharedfiles`, `theApp.serverlist`, `theApp.searchlist` access in `CSingleLock lk(&g_csWebAccess, TRUE)`
- Acquire in data-collection phase of each `_Get*()` function; release before HTML assembly

**Effort:** M (must be done before any other web server work)
**Dependency:** none

---

### I-02 JSON API Layer — `/api/v1/`

**Why:** Every third-party client, mobile app, script, and automation tool needs a machine-readable API. HTML screen-scraping is not viable.

**What:**
Add `nlohmann/json` (already in eMuleAI as `json.hpp`) and a new router before `_ProcessURL()`:

```
OnRequestReceived()
  ↓ path starts with /api/v1/ ?
  ├─ YES → _ProcessAPIRequest() [new]
  └─ NO  → _ProcessURL() [unchanged HTML path]
```

**Endpoints (Phase 1 — read-only):**

```
GET /api/v1/auth/login?p=<md5>     → {"session":"<token>","is_admin":true}
POST /api/v1/auth/logout            → {}

GET /api/v1/transfers               → {downloads:[...], uploads:[...]}
GET /api/v1/servers                 → {servers:[...]}
GET /api/v1/shared                  → {files:[...]}
GET /api/v1/stats                   → {dl_speed, ul_speed, connections, ...}
GET /api/v1/myinfo                  → {hash, id, tcp_port, udp_port}
GET /api/v1/kad                     → {running, firewalled, nodes}
GET /api/v1/log?offset=0&limit=100  → {lines:[...]}
```

**Endpoints (Phase 2 — write):**

```
POST /api/v1/transfers              body: {"link":"ed2k://...","cat":0}
PATCH /api/v1/transfers/<hash>      body: {"op":"pause|resume|cancel|rename|priority|category"}
DELETE /api/v1/transfers/<hash>

POST /api/v1/search                 body: {"q":"...","method":"kademlia","type":"video"}
GET /api/v1/search/<job_id>         → {status:"running|done", results:[...]}

PATCH /api/v1/servers/<ip>/<port>   body: {"op":"connect|remove|priority"}
POST /api/v1/prefs                  body: {max_dl_kbs, max_ul_kbs, ...}
```

**Standard error envelope:**
```json
{"error":"SESSION_EXPIRED","message":"Session has expired. Please log in again."}
```

**HTTP status codes:** `200`, `400`, `401`, `403`, `404`, `409`, `422`, `500`

**Effort:** L (3–4 weeks)
**Dependency:** I-01, nlohmann/json via vcpkg

---

### I-03 Secure Session Tokens

**Why:** `rand()` session IDs are predictable; MD5 passwords have no salt.

**What:**
- Session token: 128-bit from `BCryptGenRandom` encoded as 32-char hex
- Password hash: PBKDF2-SHA256 with 128-bit random salt, 100,000 iterations (Windows CNG `BCryptDeriveKeyPBKDF2`)
- Session struct: `CString sToken` replaces `long lSession`
- Migration: existing MD5 hashes are re-hashed on first successful login

**Effort:** M
**Dependency:** I-01

---

### I-04 HTTP/1.1 Keep-Alive

**Why:** Every page load currently creates a new TCP connection. On high-latency connections the web UI is sluggish.

**What:**
- Parse `Connection: keep-alive` request header
- After sending response: reset receive buffer (`m_dwRecv = 0`) and return to `WaitForMultipleObjects` loop
- Per-connection idle timer (30 s): close stale connections
- `Connection: keep-alive` response header + `Keep-Alive: timeout=30`

**Effort:** M
**Dependency:** I-01

---

### I-05 CSRF Protection

**Why:** All state-changing operations are CSRF-vulnerable (one image tag can cancel all downloads).

**What:**
- Per-session CSRF token (32-char hex from `BCryptGenRandom`)
- Embedded in all forms as `<input type="hidden" name="csrf" value="...">`
- Validated on every POST; reject with `403` if missing or mismatched
- API endpoints use `Authorization: Bearer <session_token>` header instead of query param

**Effort:** M
**Dependency:** I-03

---

### I-06 Web Interface Template Engine Rewrite

**Why:** Current `CString::Replace()` engine scans 115 KB × 200 tokens = 23 MB per page render.

**What:**
- Pre-scan template at load time: build `vector<TemplateChunk>` (literal | token enum)
- Render: iterate chunk vector, emit literals directly, call token handler for token chunks
- Estimated speedup: 10–50× for complex pages

**Effort:** M
**Dependency:** I-01

---

### I-07 Dark Mode Web Interface

**Why:** The desktop app gets dark mode (Pillar J); the web interface should match.

**What:**
- New `eMule Dark.tmpl` alongside `eMule.tmpl` and `eMule Light.tmpl`
- CSS custom properties: `--bg: #1e1e1e; --fg: #d4d4d4; --accent: #569cd6;`
- Auto-detect via `prefers-color-scheme` media query or explicit toggle
- User preference stored in session

**Effort:** M
**Dependency:** I-06

---

### I-08 CORS & External Client Support

**Why:** Browser-based remote management apps and mobile apps need CORS.

**What:**
- `Access-Control-Allow-Origin: <whitelist>` (configurable; default `*` for localhost)
- Handle `OPTIONS` preflight
- Expose `Access-Control-Allow-Methods: GET, POST, PATCH, DELETE`
- Add `X-Content-Type-Options: nosniff` and `X-Frame-Options: SAMEORIGIN`

**Effort:** S
**Dependency:** I-02

---

### I-09 Web Server Rate Limiting & DoS Protection

**Why:** No rate limiting; an attacker or misbehaving script can flood the web server.

**What:**
- Per-IP request rate: configurable (default 60 req/min)
- POST body size cap: `Content-Length > 65536` → `413 Content Too Large`
- Login rate limit: 5 attempts/15 min (already partially implemented; harden with per-IP table cap)
- `Retry-After` header on rate-limited responses

**Effort:** S
**Dependency:** I-01

---

## Pillar J — UI & User Experience

### J-01 Dark Mode

**Why:** eMuleAI's `DarkMode.cpp/h` (84 KB) implements comprehensive dark theming. It's the most-requested visual feature.

**What (port from eMuleAI):**
- `CDarkMode` class: hooks `WM_CTLCOLOR*`, `WM_DRAWITEM`, `WM_PAINT` for all controls
- Custom-drawn: list headers, progress bars, tabs, menus, toolbars, scrollbars
- Hover/pressed states preserved
- Toggle without restart (store in preferences, re-apply on next message pump cycle)
- Respects Windows 10 dark mode system setting (`HKCU\Software\Microsoft\Windows\CurrentVersion\Themes\Personalize\AppsUseLightTheme`)

**Effort:** L (port is large; DarkMode.cpp is 84 KB)
**Dependency:** none

---

### J-02 Virtual Lists (Owner-Data CListCtrl)

**Why:** Large download lists (1,000+ items) with full-data `CListCtrl` mode cause noticeable repaints on scroll.

**What (port pattern from eMuleAI):**
- Enable `LVS_OWNERDATA` style on: Download list, Upload list, Search results, Shared files, Queue list
- Implement `LVN_GETDISPINFO` for on-demand cell rendering
- Implement `LVN_ODCACHEHINT` for prefetch
- Single combined sort pass per column click
- Faster pause/stop/remove/complete operations

**Effort:** L (requires touching each list control class)
**Dependency:** none

---

### J-03 Advanced Transfer Window Layout (ATWL)

**Why:** MorphXT, StulleMule, UltiMatiX all implemented ATWL — a more flexible layout for the transfer window with resizable panes, collapsible sections, and per-section sorting.

**What:**
- Resizable splitter between download list and upload/queue lists
- Collapsible "Upload Queue" section (toggle visibility without hiding uploads)
- Per-section column profiles (downloads have different columns than uploads)
- Persistent layout in preferences (splitter ratios, column widths)

**Effort:** L
**Dependency:** J-02

---

### J-04 Geolocation Display (GeoLite2)

**Why:** IP2Country via MMDB is already in eMulebb. Need UI to surface it.

**What (port from eMuleAI's `GeoLite2.cpp/h` and `GeoLiteDownloadDlg.cpp/h`):**
- Country flag icons in: client list, upload list, queue list, server list, Kad dialog
- City and country tooltip on hover
- `GeoLiteDownloadDlg`: one-click background download of the latest `GeoLite2-Country.mmdb` from MaxMind
- Optional country-column in all lists
- WHOIS provider info (UltiMatiX style: click IP to open WHOIS in browser)

**Effort:** M
**Dependency:** none

---

### J-05 Speed Graph UI

**Why:** eMuleAI's `SpeedGraph.cpp/h` provides a more readable real-time speed display.

**What:**
- Replace fixed-size GDI graph with a scalable, anti-aliased GDI+ speed graph
- Show: download speed, upload speed, connection count on three overlay lines
- Click to open full-size graph dialog
- Export graph as PNG (right-click)

**Effort:** M
**Dependency:** none

---

### J-06 Status Bar Modernization

**Why:** Current status bar is text-only. Users want quick at-a-glance state.

**What:**
- Status bar panes: [DL speed] [UL speed] [Connections/Max] [Server name | ping] [Kad state] [Queue count]
- Color coding: green = good, yellow = limited, red = error
- Click pane to open relevant dialog
- Session transfer total in tooltip (from UltiMatiX: "Show session download/upload")

**Effort:** M
**Dependency:** none

---

### J-07 Toolbar Enhancements

**Why:** eMuleAI adds Save State, Reload Config, Backup buttons + numbered Preview buttons.

**What (port from eMuleAI):**
- **Save State**: forces immediate save of `known.met`, `preferences.ini`, `sources.dat`
- **Reload Config**: re-read `preferences.ini` without restart
- **Backup**: zip current config directory to timestamped backup
- **Preview buttons 1–10**: launch configurable preview applications
- Prevent accidental double-click on toolbar toggle buttons

**Effort:** S
**Dependency:** none

---

### J-08 Migration Wizard

**Why:** Users upgrading from older eMule builds or switching from another P2P client need their data migrated.

**What (port from eMuleAI's `MigrationWizardDlg.cpp/h`):**
- First-launch wizard: detect existing eMule install directories
- Offer to copy: `known.met`, `preferences.ini`, `downloads/`, `incoming/`
- Show copy results with success/failure per file
- Available later via Tools → Migration Wizard

**Effort:** S
**Dependency:** none

---

### J-09 Multi-Language Embedded Resources

**Why:** eMuleAI embeds 116 language resources, eliminating the need for external language DLL files.

**What:**
- Bundle top 20 language packs as embedded resources (reduce installer count)
- Language selector dialog on first launch
- Runtime language switch without restart
- Remaining languages available as separate DLL downloads

**Effort:** M
**Dependency:** none

---

### J-10 CPU & RAM Usage Display

**Why:** UltiMatiX surfaces real-time CPU and RAM usage in the title bar. Users want to know if eMule is impacting system performance.

**What:**
- `GetProcessMemoryInfo()` + `GetProcessTimes()` sampled at 5 s intervals
- Display in title bar: `eMule 0.73 | CPU: 2% | RAM: 148 MB`
- Configurable: title bar / status bar / tooltip only

**Effort:** S
**Dependency:** none

---

## Pillar K — Security & Cryptography

### K-01 Fix All Known Security Defects (D-01 through D-04)

This is non-negotiable. Do this in the first sprint.

| Defect | Fix |
|--------|-----|
| D-01: `strcpy()` | Replace with `strcpy_s()` |
| D-02: `srand(time(NULL))` | Replace with `AutoSeededRandomPool` (Crypto++) |
| D-03: `rand()` for challenge | Replace with `AutoSeededRandomPool::GenerateWord32()` |
| D-04: `inet_addr()` | Replace with `InetPton()` in the two socket files **[DONE: `768559c`]** |

**Effort:** S
**Dependency:** none

---

### K-02 Migrate from MBEDTLS_ALLOW_PRIVATE_ACCESS

**Why:** `MBEDTLS_ALLOW_PRIVATE_ACCESS` is a technical debt flag that accesses mbedTLS internals. Future mbedTLS updates may remove those internal symbols.

**What:**
- Audit all uses of `MBEDTLS_ALLOW_PRIVATE_ACCESS` in `WebSocket.cpp`
- Rewrite using public PSA Crypto API (`psa_*` functions)
- Test with mbedTLS 4.0 strict mode (`MBEDTLS_ALLOW_PRIVATE_ACCESS` not defined)

**Effort:** M
**Dependency:** none

---

### K-03 Secure User Identification (SUI) Hardening

**Why:** SUI uses RSA-1024 + SHA-1 signatures for client identity. RSA-1024 is considered broken (NIST retired it in 2014).

**What:**
- Increase SUI key size to RSA-2048 (backward compatible: old clients verify with their key size)
- Replace SHA-1 signature hash with SHA-256 where both peers advertise support (new capability bit)
- Track SUI version in `CClientCredits`
- Non-SUI clients: flag in Shield (Pillar F-01) with configurable score reduction

**Effort:** L (touches protocol handshake)
**Dependency:** K-01

---

### K-04 Protocol Obfuscation Hardening

**Why:** RC4 + 768-bit DH for protocol obfuscation is known-weak. While the goal is only to obfuscate (not provide confidentiality), broken primitives attract scrutiny.

**What:**
- Upgrade obfuscation DH to 2048-bit (performance impact is one-time per connection)
- Consider ChaCha20 as RC4 replacement for obfuscation stream cipher
- Both changes are backward-compatible (negotiated during obfuscation handshake)

**Effort:** M
**Dependency:** none

---

### K-05 Certificate Pinning for Known Servers

**Why:** Connections to eD2K servers are unencrypted. An active attacker can inject fake source lists.

**What:**
- SHA-256 hash of well-known server TLS certificates pinned in a local `pinned_certs.dat`
- On TLS-capable server connection (if server supports TLS), verify pin
- Log pin failures as warnings; block on configurable policy
- Community-updatable pin list (fetched alongside server.met updates)

**Effort:** L
**Dependency:** K-02

---

## Pillar L — AI / ML Integration

This is the genuinely new territory. eMuleAI's name is aspirational but the AI features are not yet implemented. Here we design them.

### L-01 Smart Source Prediction

**Why:** eD2K source lookup is expensive (roundtrip to server/Kad). ML can predict which sources are most likely to be online and available.

**What:**
- **Model type:** Gradient Boosted Trees (XGBoost, small model, ~500 KB)
- **Features:** file hash popularity (requests/h), time of day, day of week, source IP geo-region, historical availability (from source cache), file type
- **Output:** probability that source X is available for file Y at time T
- **Use:** sort source re-ask order; skip sources with predicted availability < 10%
- **Training:** offline, on aggregated anonymized source-availability logs from opt-in users
- **Inference:** local, `onnxruntime.dll` (~8 MB), CPU-only, < 1 ms per prediction

**Effort:** XL (requires data pipeline, model training, inference integration)
**Dependency:** F-06 (client history), source cache data

---

### L-02 Smart Download Scheduling

**Why:** With 50+ downloads, which file should get priority sources? Currently it's user-defined static priorities. ML can do better.

**What:**
- Predict download completion time for each file based on: source count, source speed distribution, chunk availability map
- Auto-prioritize files closest to completion (PBF, Pillar E-06) and files with rarest chunks
- Surface as a "Smart Priority" toggle (does not override manual priorities)
- No external model — pure heuristic initially, ML enhancement later

**Effort:** M (heuristic version), XL (ML version)
**Dependency:** E-06

---

### L-03 Spam / Fake File Detection with NLP

**Why:** Search results contain files with misleading names (e.g., `ubuntu-24.04.iso` that is 50 MB of garbage). Text-based detection can catch obvious cases.

**What:**
- **Model:** TF-IDF bag-of-words + logistic regression (tiny, < 100 KB)
- **Features:** filename, file size, extension, ratio of digits/special chars in name, source count, completeness ratio
- **Output:** spam probability 0–1
- **Training:** use DLP list + known-good AICH hashes as labels
- Feed into Spam Rating column (Pillar F-03)

**Effort:** L
**Dependency:** F-03, H-02

---

### L-04 Adaptive Upload Slot Tuning

**Why:** The optimal number of upload slots depends on connection speed, peer latency distribution, and file popularity. Static `BBMaxUpClientsAllowed` is a good start; adaptive tuning is better.

**What:**
- Observe: per-slot throughput, slot utilisation rate, queue depth, client RTT
- PID-style controller: if `avg_slot_throughput < target × 0.8` → reduce slots; if `queue_depth > 50` → increase slots
- Configurable: fully manual / fully adaptive / hybrid (manual max, adaptive within range)
- Graph adaptive history in statistics window

**Effort:** M
**Dependency:** E-01, B-02

---

### L-05 AI-Powered File Categorization

**Why:** eMule's category system is manual. ML can suggest categories based on filename and MediaInfo output.

**What:**
- Rule-based first pass (extension → category): trivial, always on
- ML second pass: filename embeddings (sentence-transformers, quantized) → category classification
- Suggest category on "Add download" dialog, user confirms or changes
- Requires: ~50 MB quantized embedding model (ONNX, CPU inference)

**Effort:** L
**Dependency:** H-02 (MediaInfo)

---

### L-06 Network Health Monitoring with Anomaly Detection

**Why:** Kad routing table poisoning, DDoS patterns, and leecher surges are hard to detect manually.

**What:**
- Time-series anomaly detection on: Kad lookup failure rate, source re-ask success rate, connection error rate, upload/download ratio per IP subnet
- Alert types: [WARN] Unusual failure rate from subnet X, [WARN] Possible Kad poisoning detected
- Simple threshold-based initially; Isolation Forest for anomaly detection later
- Log to debug log; optionally show notification

**Effort:** M
**Dependency:** M-01 (metrics)

---

### L-07 Intelligent Search Query Expansion

**Why:** Users search for "ubuntu iso" but may miss "ubuntu-24.04-desktop-amd64.iso". Query expansion helps.

**What:**
- Word stemming and synonym expansion (offline, ~5 MB dictionary)
- Suggest alternative search terms in real-time (type-ahead)
- Hash-based deduplication of equivalent results
- Configurable: off / suggestions only / auto-expand

**Effort:** M
**Dependency:** H-01

---

## Pillar M — Observability & Diagnostics

### M-01 Structured Metrics Export

**Why:** The current statistics page is HTML. No time-series data. No alerting. No integration with Prometheus/Grafana.

**What:**
- Add `GET /api/v1/metrics` in Prometheus text format:
  ```
  emule_dl_speed_bytes_per_sec 102400
  emule_ul_speed_bytes_per_sec 51200
  emule_connections_current 247
  emule_downloads_active 12
  emule_downloads_paused 3
  emule_kad_nodes 1847
  emule_queue_size 423
  ```
- Scrape interval: configurable (default 15 s)
- Optional: push to InfluxDB via line protocol

**Effort:** S
**Dependency:** I-02

---

### M-02 Structured Debug Logging

**Why:** Current debug log is a flat text stream. Hard to filter, hard to grep, hard to correlate.

**What:**
- Structured log entries: `{timestamp, level, component, message, fields...}`
- JSON log output option (for ingestion by Elasticsearch, Loki, etc.)
- Log levels: TRACE, DEBUG, INFO, WARN, ERROR (currently binary debug/non-debug)
- Per-component log level (e.g., `Kademlia=DEBUG, Upload=INFO, WebServer=WARN`)
- Ring buffer for last N lines (configurable, default 10,000)
- Export button in debug log dialog

**Effort:** M
**Dependency:** none

---

### M-03 Crash Reporter

**Why:** eMule crashes are reported by users as vague "it crashed" with no call stack.

**What:**
- Integrate Google Breakpad or Microsoft WER (Windows Error Reporting) API
- On crash: write minidump to `%APPDATA%\eMule\crashdumps\`
- Optional: opt-in upload to telemetry endpoint (GDPR-compliant, anonymized)
- On next launch: detect crashdump, offer to send report
- Symbol upload as part of CI/CD release pipeline

**Effort:** M
**Dependency:** A-03

---

### M-04 Thread Names in Release Builds

**Why:** eMuleAI exposes thread names in release builds. This dramatically improves crash dump readability.

**What (already in eMuleAI — easy port):**
- Call `SetThreadDescription()` (Windows 10+) for every thread
- Thread names: `"eMule-Network"`, `"eMule-Kad"`, `"eMule-WebServer"`, `"eMule-FileWrite"`, etc.
- No performance impact; improves debuggability enormously

**Effort:** S
**Dependency:** none

---

### M-05 Connection Checker

**Why:** eMuleAI's `CConChecker.cpp/h` provides one-click connectivity diagnostics.

**What (port from eMuleAI):**
- Test: DNS resolution, TCP connect to known good server, UDP echo (Kad bootstrap)
- Show results as pass/fail with latency
- Log start/stop lines to debug log
- Button in status bar area or Tools menu

**Effort:** S
**Dependency:** none

---

## Pillar N — Configuration & Preferences

### N-01 Fix All 19 Preference Persistence Defects

**Why:** `DEFECTS.md` lists 19+ preferences that are loaded but not saved back to `preferences.ini`.

**What:**
- Audit every `thePrefs.Get*()` call in UI handlers
- For each: verify corresponding `thePrefs.Set*()` is called on save
- Verify INI key case consistency (D-05: upload overhead counter mismatch)
- Fix D-06: web server allowed IPs save-back

**Effort:** M (tedious but straightforward)
**Dependency:** none

---

### N-02 Modern Defaults Pass

**Why:** eMulebb's `MODERN_LIMITS.md` documents this. Defaults tuned for 2002 dial-up modems hurt broadband users.

**What:**

| Setting | Old Default | New Default | Rationale |
|---------|-------------|-------------|-----------|
| Max connections | 500 | 2000 | Modern OS handles 10k+ |
| Max sources/file | 400 | 2000 | More sources = faster completion |
| Half-open connections | 8 | 50 | Windows 10+ no kernel limit |
| Upload slots | 3 | 10 | 100 Mbps+ lines can serve many |
| File buffer | 256 KB | 16 MB | NVMe default |
| Buffer time limit | 1 min | 10 min | Reduces write amplification |
| Max upload time | 1 h | 3 h | Reduces churn on slow peers |
| Session max trans | 2 GB | 64 GB | Broadband sessions |
| UDP socket buffer | 8 KB | 512 KB | High-throughput Kad |

**Effort:** S
**Dependency:** none

---

### N-03 Preferences Export / Import

**Why:** Power users want to share configurations. There is no export function.

**What:**
- `File → Export Settings` → zip of `preferences.ini` + category definitions + IP filter
- `File → Import Settings` → validate + apply (no restart required for most settings)
- Useful for: fleet management, community "starter configs", backup before changes

**Effort:** S
**Dependency:** G-02 (safe config saving)

---

### N-04 Advanced / Hidden Preferences UI

**Why:** eMuleAI's `PPgMod.cpp/h` (138 KB) exposes power-user settings that are buried in `preferences.ini`. UltiMatiX and MorphXT both had "Advanced Preferences" panels.

**What:**
- New "Advanced" tab in Preferences dialog
- Exposes: all broadband settings, Shield thresholds, buffer sizes, Kad limits, ML toggles
- "Reset to defaults" per section
- Tooltips explaining each setting and its trade-offs

**Effort:** M
**Dependency:** N-02

---

## Pillar O — Packaging, Distribution & CI/CD

### O-01 GitHub Actions CI Pipeline

**Why:** No CI. Every developer's machine is the only test environment.

**What:**
```yaml
# .github/workflows/build.yml
jobs:
  build:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: microsoft/setup-msbuild@v2
      - uses: lukka/run-vcpkg@v11
        with: { vcpkgJsonGlob: vcpkg.json }
      - run: msbuild srchybrid/emule.vcxproj /t:Build /p:Configuration=Release /p:Platform=x64
      - run: .\tests\run_smoke.ps1
```

- On PR: build + static analysis (`clang-tidy` / PVS-Studio)
- On merge to main: build + sign + upload artifact
- On tag: create GitHub Release with installer

**Effort:** M
**Dependency:** A-02

---

### O-02 NSIS / WiX Installer

**Why:** Current distribution is a zip. No installer means no Start Menu entry, no uninstall, no file associations.

**What:**
- WiX v4 or NSIS installer
- Install to `%ProgramFiles%\eMule\`
- Register `ed2k://` URI handler (`HKLM\SOFTWARE\Classes\ed2k`)
- Register `.emulecollection` file association
- Start Menu shortcut + optional Desktop shortcut
- Add Windows Firewall rules during install (replaces `firewall-opener.ps1`)
- Signed with EV code signing certificate

**Effort:** M
**Dependency:** O-01

---

### O-03 Portable Mode

**Why:** Users want to run from a USB drive without installing.

**What:**
- Detect `portable.ini` in executable directory
- If present, store all data in subdirectories of the executable directory
- No registry writes in portable mode
- Portable ZIP artifact in CI alongside installer

**Effort:** S
**Dependency:** none

---

### O-04 Auto-Update

**Why:** Users run years-old builds because they don't know updates exist.

**Current state:** The legacy DNS-based update checker and its UI hooks were removed on 2026-03-30. Any future updater should be implemented as a new code path.

**What:**
- Background version check against GitHub Releases API on startup (configurable interval)
- Show non-modal notification: "eMule 0.73.2 is available. [Download] [Remind me later] [Skip this version]"
- Download installer in background, offer one-click update
- Delta updates (binary patch) for future releases to minimize download size

**Effort:** M
**Dependency:** O-02

---

### O-05 Microsoft Store / WinGet Package

**Why:** Modern Windows users discover software via Store or `winget install`.

**What:**
- WinGet manifest: `emule.yaml` submitted to `microsoft/winget-pkgs`
- Microsoft Store submission (requires MSIX packaging + UWP-compatible icon set)
- Note: Store requires app container isolation — may conflict with MFC architecture; WinGet is easier

**Effort:** M
**Dependency:** O-02

---

## 19. Cross-cutting Concerns

### CC-01 Documentation as Code

Every pillar must produce a `docs/<pillar>/*.md` document covering: what changed, why, how to configure it, and what the default values are. This is already the pattern established in eMulebb (20 markdown files). Extend it.

### CC-02 Configuration Schema

All configuration keys documented in a machine-readable `preferences.schema.json`. Used by:
- The preferences UI (tooltip generation)
- The REST API preferences endpoint (field validation)
- The installer (default value injection)
- Documentation generation

### CC-03 Unicode Throughout

No `char*` strings for user-visible content. All filenames, nicknames, server names, and search queries go through `std::wstring` or `CString` (Unicode build only). Audit for any remaining `CStringA` usage in UI paths.

### CC-04 Memory Safety Budget

Set a goal: zero calls to `malloc`/`free` in new code. Use:
- `std::vector`, `std::string`, `std::unique_ptr`, `std::shared_ptr`
- `CString` for MFC-interop
- `std::span` for buffer views (C++20)

Enable `/analyze` (MSVC Code Analysis) on CI with zero-warning policy for new files.

### CC-05 C++ Standard Baseline

All new code targets **C++20**. Existing code is C++14/17 mixed; do not downgrade. Enable:
- `std::format` (C++20) for string formatting (replace `CString::Format`)
- `std::ranges` for list filtering
- `std::jthread` for cooperative cancellation in new worker threads
- Concepts for template constraints (especially `CRing<>` and `CThreadPool`)

---

## 20. 12-Month Sprint Plan

Sprints are 2 weeks each. Assume a 4-engineer team.

```
Q1 — Foundation & Safety (Sprints 1–6, Weeks 1–12)
────────────────────────────────────────────────────
S1  W01-02  K-01: Fix D-01…D-04. N-01: Fix all 19 pref defects.
            A-03: Set up static analysis in CI.
            M-04: Thread names in release builds.

S2  W03-04  I-01: Web server thread safety (critical section).
            I-03: Secure session tokens (BCryptGenRandom, PBKDF2).
            I-05: CSRF tokens.
            I-09: Web server rate limiting.

S3  W05-06  A-02: CMake + vcpkg build system.
            O-01: GitHub Actions CI pipeline.
            A-04: Long-path hardening audit.

S4  W07-08  N-02: Modern defaults pass.
            E-07: File buffer HDD protect mode.
            G-02: Threaded known.met save.
            D-09/D-10: GDI leak + UNC fix.

S5  W09-10  B-01: Network thread separation (start).
            M-01: Prometheus metrics endpoint.
            M-05: Connection checker port from eMuleAI.

S6  W11-12  B-01: Network thread separation (complete).
            B-02: Lock-free rate statistics.
            B-03: Thread pool introduction.

Q2 — API & Connectivity (Sprints 7–12, Weeks 13–24)
─────────────────────────────────────────────────────
S7  W13-14  I-02: JSON API /api/v1/ Phase 1 (read endpoints).
            I-04: HTTP keep-alive.
            I-08: CORS headers.

S8  W15-16  I-02: JSON API Phase 2 (write endpoints).
            I-06: Template engine rewrite.
            M-02: Structured debug logging.

S9  W17-18  C-01: IPv6 support (start — address abstraction layer).
            D-01: Kad IPv6 contacts.
            K-02: MBEDTLS_ALLOW_PRIVATE_ACCESS migration.

S10 W19-20  C-01: IPv6 (complete — dual-stack listening).
            C-05: Server connection resilience / failover.
            C-06: NAT-PMP / PCP.

S11 W21-22  C-02: uTP transport (start — libutp integration).
            D-03: Fast Kad / Safe Kad port.
            D-05: nodes.dat management + bootstrap URL.

S12 W23-24  C-02: uTP (complete — negotiation + fallback).
            C-03: eServer Buddy NAT path port.
            H-04: Ed2k link extended metadata.

Q3 — Features & UX (Sprints 13–18, Weeks 25–36)
──────────────────────────────────────────────────
S13 W25-26  J-01: Dark mode (start — CDarkMode port from eMuleAI).
            F-05: IP filter hardening.
            G-04: FakeAlyzer port.

S14 W27-28  J-01: Dark mode (complete).
            J-04: GeoLite2 UI (flags, download dialog).
            J-06: Status bar modernization.

S15 W29-30  J-02: Virtual lists (download list + search results).
            H-01: Search window virtual list + filter bar.
            F-04: Source caching (SourceSaver port).

S16 W31-32  F-01: Shield anti-leecher engine (start — port from eMuleAI).
            F-02: DLP list integration.
            F-06: Client history (SQLite table).

S17 W33-34  F-01: Shield (complete).
            F-03: Spam rating + blacklist panel.
            E-03: PowerShare.
            E-04: A4AF swapping.

S18 W35-36  E-01: Broadband slot controller completion + UI wiring.
            E-02: Multi-chunk upload (Mephisto port).
            E-05: Download duplicate checker.
            E-06: PBF push-before-finish.

Q4 — Advanced & AI (Sprints 19–24, Weeks 37–48)
──────────────────────────────────────────────────
S19 W37-38  G-01: AICH hashset optimisation (Xtreme port).
            G-05: SIVKA per-file settings.
            N-04: Advanced preferences UI.
            J-07: Toolbar enhancements port.

S20 W39-40  J-03: ATWL (Advanced Transfer Window Layout).
            J-05: Speed graph UI.
            J-08: Migration wizard port.
            J-09: Embedded multi-language resources (top 20).

S21 W41-42  H-02: Embedded MediaInfo library.
            H-03: Collection handling fixes.
            G-03: Archive recovery port.
            K-03: SUI RSA-2048 hardening.

S22 W43-44  L-02: Smart download scheduling (heuristic version).
            L-03: Spam/fake file detection (TF-IDF + logistic regression).
            L-04: Adaptive upload slot tuning (PID controller).
            L-07: Search query expansion.

S23 W45-46  L-01: Smart source prediction (ONNX model, offline training).
            L-05: AI file categorization.
            L-06: Network health anomaly detection.
            M-03: Crash reporter integration.

S24 W47-48  O-02: NSIS/WiX installer.
            O-03: Portable mode.
            O-04: Auto-update.
            O-05: WinGet package submission.
            I-07: Dark mode web interface.
            K-05: Certificate pinning.

            Release: eMule 0.73.0
```

---

## 21. Risk Register

| ID | Risk | Probability | Impact | Mitigation |
|----|------|------------|--------|-----------|
| R-01 | B-01 network thread separation breaks existing socket code | HIGH | CRITICAL | Incremental extraction; keep both paths gated by compile flag during transition |
| R-02 | C-01 IPv6 breaks existing peer address assumptions | HIGH | HIGH | Introduce `CAddress` abstraction first; compile-time flag IPv6_ENABLED |
| R-03 | F-01 Shield false-positives ban legitimate clients | MEDIUM | HIGH | All punishments configurable; "log only" mode for testing; manual unban |
| R-04 | L-01 ML model quality insufficient for production | MEDIUM | MEDIUM | Ship heuristic version first; ML is opt-in enhancement |
| R-05 | K-03 SUI key upgrade breaks compatibility with old clients | LOW | HIGH | Negotiate upgrade; old clients fall back to RSA-1024 gracefully |
| R-06 | C-02 uTP causes packet loss issues with some ISPs | MEDIUM | MEDIUM | uTP is opt-in; disable if TCP-only preference set |
| R-07 | J-01 Dark mode breaks custom MFC controls | MEDIUM | MEDIUM | Maintain opt-out; test each dialog individually |
| R-08 | A-02 CMake adoption splits team between VS and CMake workflows | LOW | LOW | `.vcxproj` remains primary for IDE users; CMake is CI-only |
| R-09 | I-02 JSON API exposes private data | LOW | HIGH | All endpoints require valid session; add IP whitelist to API |
| R-10 | eD2K network shrinkage makes protocol work moot | LOW | CRITICAL | Kademlia is self-sustaining; eD2K servers are secondary |

---

## 22. Success Metrics

After 12 months, the project should demonstrably achieve:

| Metric | Baseline (0.72a) | Target (0.73.0) |
|--------|-----------------|-----------------|
| Download throughput on 1 Gbps line | ~50–100 MB/s | > 300 MB/s |
| Max upload slots | 50 (hardcoded) | 200 (broadband controller) |
| Cold-start Kad bootstrap time | 5+ min | < 90 s (FastKad) |
| Web API response time (transfers list) | N/A (HTML only) | < 50 ms (JSON) |
| Search result spam rate | ~30% junk | < 10% junk (Spam Rating) |
| LowID peer connectivity rate | ~15% | > 40% (NAT traversal) |
| Crash rate (per 1000 user-hours) | Unknown | < 0.5 (Breakpad) |
| Build reproducibility | Manual only | 100% CI green |
| Static analysis warnings | Unknown | 0 (new code) |
| Lines with `rand()` in security context | 3+ | 0 |
| Preference defects (not saved) | 19 | 0 |
| Platform support | x64 Windows 10+ | x64 + ARM64 Windows 10+ |

---

## Appendix A — Feature Origin Matrix

| Feature | eMulebb | eMuleAI | MorphXT | Mephisto | UltiMatiX | Xtreme | StulleMule | NeoMule | New |
|---------|---------|---------|---------|---------|----------|--------|-----------|---------|-----|
| TLS 1.3 | ✅ | | | | | | | | |
| ARM64 build | ✅ | | | | | | | | |
| Broadband slot controller | ✅ | | | | ✅ | | | | |
| CRing rate tracking | ✅ | | | | | | | | |
| IPv6 | | ✅(α) | | | | | | ✅ | |
| uTP | | ✅ | | | | | | | |
| eServer Buddy NAT | | ✅ | | | | | | | |
| Dark mode | | ✅ | | | | | | | |
| Virtual lists | | ✅ | | | | | | | |
| Shield / Anti-leecher | | ✅ | ✅ | | ✅ | | | | |
| DLP list | | | ✅ | | ✅ | | | | |
| GeoLite2 / IP2Country | ✅ | ✅ | ✅ | | ✅ | | | | |
| PowerShare | | | ✅ | ✅ | ✅ | ✅ | | | |
| Multi-chunk upload | | | | ✅ | ✅ | | | | |
| A4AF | | | ✅ | | | | | | |
| AICH optimization | | | | | | ✅ | | | |
| Threaded known.met save | | | | | | | ✅ | | |
| File watcher / ASFU | | ✅ | | | ✅ | | ✅ | | |
| SIVKA per-file settings | | | | | ✅ | | ✅ | | |
| ATWL | | | ✅ | | ✅ | | ✅ | | |
| FakeAlyzer | | | ✅ | | | | | | |
| PBF push-before-finish | | | | ✅ | ✅ | | | | |
| Source caching | | ✅ | | | | | | | |
| Download checker | | ✅ | | | | | | | |
| Archive recovery | | ✅ | | | | | | | |
| JSON API | | | | | | | | | ✅ |
| Prometheus metrics | | | | | | | | | ✅ |
| Crash reporter | | | | | | | | | ✅ |
| AI source prediction | | | | | | | | | ✅ |
| AI spam detection | | | | | | | | | ✅ |
| Adaptive slot tuning | | | | | | | | | ✅ |
| NAT-PMP / PCP | | | | | | | | | ✅ |
| CMake build | | | | | | | | | ✅ |
| WinGet package | | | | | | | | | ✅ |

---

## Appendix B — Dependency Upgrade Matrix

| Library | Current | Target | Notes |
|---------|---------|--------|-------|
| mbedTLS | 4.0.0 | 4.x (track) | MBEDTLS_ALLOW_PRIVATE_ACCESS to be removed (K-02) |
| Crypto++ | 8.9.0 | 8.x (track) | No breaking changes expected |
| miniupnpc | 2.3.3 | 2.x (track) | NAT-PMP via libnatpmp addition |
| zlib | 1.3.2 | 1.3.x | Stable |
| id3lib | v3.9.1 | Frozen | Consider replacement with TagLib |
| nlohmann/json | (add) | 3.11+ | For REST API and config schema |
| libmaxminddb | (add) | 1.9+ | GeoLite2 MMDB reader |
| libutp | (add) | latest | Micro Transport Protocol |
| libnatpmp | (add) | 20230423 | NAT-PMP for C-06 |
| onnxruntime | (add) | 1.19+ | AI inference (Pillar L) |
| MediaInfoLib | (add) | 24.x | Embed, eliminate DLL probe |
| SQLite | (add) | 3.45+ | Client history, per-file settings |
| Breakpad | (add) | latest | Crash reporting |

---

## Appendix C — Protocol Compatibility Promises

The following changes are **backward compatible** (old clients continue to work):

- All new eD2K client-capability bits are optional negotiation
- IPv6 addresses are additional optional fields
- uTP is negotiated; TCP fallback always available
- SUI RSA-2048 falls back to RSA-1024 for old peers
- New Kad contact IPv6 field is optional
- API (`/api/v1/`) is new, does not affect existing protocol

The following changes are **not backward compatible** and would require a protocol version bump (explicitly out of scope for this roadmap):

- Replacing MD4 file hashing (would break all existing sharing)
- Replacing the eD2K server protocol (server software would need updating)
- Replacing the Kademlia wire format entirely

---

*This document represents approximately 52 person-weeks of planning. Every item above is grounded in existing source code in eMulebb, eMuleAI, or the mods archive — nothing here is science fiction. The hardest items are B-01 (network thread) and C-01 (IPv6), both because they touch the entire codebase. Everything else can be done incrementally by one or two engineers per sprint.*

---

## Feature Identifier

### PLAN_002: Overall Modernization Roadmap

This document is the master modernization plan for eMulebb, covering C++20 migration, dependency updates, build system changes, and architectural improvements planned for 2026 and beyond.

**Status:** Active planning. Individual items are tracked by their own FEAT_* and PLAN_* identifiers where applicable.
