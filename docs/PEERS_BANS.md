# Peer Banning — eMuleAI Feature Analysis

**Date:** 2026-03-28
**Source:** `eMuleAI/srchybrid/eMuleAI/`
**Target branch:** `v0.72a`

eMuleAI has two distinct peer banning systems and one lightweight identity guard. All three are independent from the UI/theme/translation scope.

---

## 1. CShield — ED2K Anti-Leecher Engine

**Files:** `Shield.h`, `Shield.cpp` (1079 lines), `PPgProtectionPanel.h/.cpp`, `PPgBlacklistPanel.h/.cpp`

### Detection categories (`EBadClientCategory`)

44 named categories, each independently configurable to a punishment level:

| Category | Threat |
|---|---|
| `PR_TCPERRORFLOODER` | Peer repeatedly opens TCP connections and immediately errors — floods the listen socket. Detected separately via `CheckTCPErrorFlooder()` |
| `PR_WRONGTAGINFOSIZE` / `PR_WRONGTAGHELLOSIZE` | Extra bytes in hello/info packets — DarkMule fingerprint, warrants hard ban |
| `PR_HASHTHIEF` | Steals another peer's user hash to farm their upload credits |
| `PR_UPLOADFAKER` | Lies about uploaded bytes to accumulate credits without contributing |
| `PR_FILEFAKER` | Reports availability on files it does not have; wastes source exchange slots |
| `PR_AGGRESSIVE` | Over-requests upload slots and sources beyond protocol limits |
| `PR_XSEXPLOITER` | Extended Sources protocol abuse |
| `PR_ANTIP2PBOT` | Anti-P2P crawlers that consume source exchange capacity without ever uploading |
| `PR_SPAMMER` | Chat and message spam |
| `PR_FAKEMULEVERSION` / `PR_FAKEMULEVERSIONVAGAA` | Lies about eMule version to exploit version-gated features |
| `PR_NONSUIMLDONKEY` / `PR_NONSUIEMULE` / etc. | Clients skipping Secure User Identification |
| `PR_WRONGTAGFORMAT` / `PR_WRONGTAGINFOFORMAT` | Malformed protocol tags — score reduction |
| `PR_BADMODSOFT` / `PR_BADMODUSERHASHHARD` | Known leecher mod names |
| `PR_GHOSTMOD` | Webcache tag without modstring |
| `PR_MODCHANGER` / `PR_USERNAMECHANGER` | Identity cycling to evade per-peer tracking |
| `PR_OFFICIALBAN` | eMule's own official ban list |
| `PR_MANUAL` | User-triggered ban from client context menu |

### Punishment levels (`EPunishment`)

```
P_IPUSERHASHBAN   — hard ban by IP + user hash
P_USERHASHBAN     — ban by user hash only
P_UPLOADBAN       — blocked from receiving uploads, still allowed otherwise
P_SCOREX01–X09    — score multiplier reductions (fine-grained queue demotion)
P_NOPUNISHMENT    — log only
```

Score reduction is the key difference from eMule's binary ban: a borderline client stays at the bottom of the upload queue rather than being hard-banned, which is more appropriate for protocol quirks vs. confirmed abuse.

### Integration hooks

```cpp
void CheckClient(CUpDownClient* client);         // called on client connect
void CheckLeecher(CUpDownClient* client);        // called during active session
bool CheckSpamMessage(CUpDownClient* client, const CString& strMessage);
void CheckHelloTag(CUpDownClient* client, CTag& tag);   // intercepts at handshake
void CheckInfoTag(CUpDownClient* client, CTag& tag);    // intercepts at handshake
void CheckTCPErrorFlooder(CUpDownClient* client);
```

`CheckHelloTag` and `CheckInfoTag` fire before the client is fully connected, catching malformed packets at the earliest possible point.

### Configuration backend

- `CPPgProtectionPanel` — MFC tree-options page, per-category punishment assignment, timed bans, friend exemption, "recheck now" trigger
- `CPPgBlacklistPanel` — manages `blacklist.conf`; banned hashes/IPs/usernames with a 0–100 spam rating
- Both declared as `friend` of `CPreferences`, consistent with the existing preferences architecture

---

## 2. SafeKad2 — Kademlia Node ID Spoofing Detection

**Files:** `SafeKad.h`, `SafeKad.cpp` (262 lines)

Protects the Kademlia routing table from eclipse and poisoning attacks. Completely independent from ED2K and from `CAddress` — can be ported standalone.

### Three tracking maps

```cpp
std::map<NodeAddress, Tracked*>     m_mapTrackedNodes;      // cap: 10,000
std::map<NodeAddress, Problematic*> m_mapProblematicNodes;  // cap: 10,000
std::map<uint32, Banned*>           m_mapBannedIPs;         // cap: 1,000
```

### Key constants

```cpp
static const time_t s_tMinimumIDChangeInterval = 3600;    // 1 hour min between ID changes
static const time_t s_tMaximumBanTime          = 4*3600;  // 4 hour ban
static const time_t s_tMaximumProblematicTime  = 5*60;    // 5 min problematic window
```

### Public API

```cpp
void TrackNode(uint32 uIP, uint16 uPort, const CUInt128& uID, bool isIDVerified = false);
void TrackProblematicNode(uint32 uIP, uint16 uPort, const CUInt128& uID = CUInt128(0UL));
void BanIP(uint32 uIP);
bool IsBadNode(uint32 uIP, uint16 uPort, const CUInt128& uID, uint8 kadVersion,
               bool isIDVerified = false, bool onlyOneNodePerIP = true);
bool IsBanned(uint32 uIP);
bool IsProblematic(uint32 uIP, uint16 uPort);
void Cleanup(time_t tNodeMaxRefAge = 3600, time_t tBanMaxRefAge = 3600,
             time_t tProblematicMaxRefAge = 300);
```

A node that changes its Kademlia ID more frequently than once per hour is flagged as bad. This directly prevents routing table poisoning (a node cycling IDs to occupy multiple routing slots) and eclipse attacks (a set of colluding nodes surrounding a target ID). All three maps are bounded and cleaned by age, preventing unbounded memory growth in long-running sessions.

---

## 3. CAntiNick — Username Theft Detection

**Files:** `AntiNick.h`, `AntiNick.cpp` (~150 lines)

Lightweight guard against impersonation attacks where a malicious peer copies the local client's username to bypass per-user rate limiting or reputation scoring.

### Mechanism

- Generates a random tag `[AI-XXXXXXXX]` (4–8 hex chars) on startup
- Tag is rotated daily
- `FindOurTagIn(CString& nick)` scans every incoming client nickname for the tag
- On match: flags the client as an impersonator and invokes the Shield punishment path
- Static singleton: `CAntiNick::instance`
- `UnInit()` called at shutdown

Overhead is minimal — the check fires only during nick comparison, which already happens on every client connect.

---

## Integration Priority

| Module | LOC | Dependencies | Effort | Payoff |
|---|---|---|---|---|
| **SafeKad2** | 262 | None (uses `uint32` internally) | Low | High — real Kad attack mitigation, no equivalent in stock eMule |
| **CAntiNick** | ~150 | None | Very low | Low-medium — narrow but zero-cost |
| **CShield** | 1079 + 3 UI files | `CUpDownClient` integration points | Medium | High — `PR_TCPERRORFLOODER` and credit fakers address real upload throughput impact |

Recommended order: **SafeKad2 → CAntiNick → CShield**.

SafeKad2 and CAntiNick are fully standalone and can be dropped in without touching any other eMuleAI feature. CShield requires wiring `CheckClient()` / `CheckLeecher()` / `CheckHelloTag()` / `CheckInfoTag()` into the client lifecycle, and adding the `CShield* m_pShield` member to `CemuleApp` with init/shutdown calls.

The `PR_TCPERRORFLOODER` category in CShield is the item with the most direct networking impact beyond fairness — it addresses a denial-of-service pattern against the listen socket that stock eMule has no specific defense for.
