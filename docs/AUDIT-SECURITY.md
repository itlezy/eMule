# eMule P2P Application — Security Code Review

**Date:** 2026-03-24
**Branch:** v0.72a-broadband-dev
**Scope:** 556 C/C++ source files

---

## Table of Contents

- [Executive Summary](#executive-summary)
- [1. TLS / SSL / HTTPS](#1-tls--ssl--https) — **[STALE]** web server + SMTP removed
- [2. Cryptographic Algorithms](#2-cryptographic-algorithms)
- [3. Random Number Generation](#3-random-number-generation)
- [4. Buffer Overflow & Memory Safety](#4-buffer-overflow--memory-safety)
- [5. Network Security](#5-network-security)
- [6. Protocol Obfuscation — Detailed Analysis](#6-protocol-obfuscation--detailed-analysis)
- [7. Input Validation & Injection](#7-input-validation--injection)
- [8. Hardcoded Values & Secrets](#8-hardcoded-values--secrets)
- [9. Known Issues & TODOs in Code](#9-known-issues--todos-in-code)
- [10. Consolidated Findings Table](#10-consolidated-findings-table)
- [11. Recommendations](#11-recommendations)

---

## Executive Summary

This eMule P2P application implements custom obfuscation/encryption, protocol handling, and an embedded web server. The codebase shows awareness of security best practices in many areas (use of `strcpy_s`, Crypto++ `AutoSeededRandomPool`, proper mutex locking, packet size guards) but has notable inconsistencies in application — particularly mixing weak `rand()` with CSPRNG, and the deep entanglement of broken hash algorithms (MD4/MD5) inherited from the eDonkey/eMule protocol specification.

**Overall Risk Assessment: MEDIUM**

- Cryptographic limitations (MD4, MD5, RC4, 768-bit DH) are protocol-legacy artifacts, not new mistakes
- No externally-authenticated session or login mechanism exposed to the network
- Several fixable issues existed: weak RNG for crypto values, one unguarded `strcpy`, deprecated socket API
- **Update 2026-03-30:** BUG_001 (strcpy) fixed in commit `0cb4d1e`; GAP_002 (`inet_addr`) fixed in commit `768559c`; BUG_003 (srand) fix attempted and intentionally reverted — accepted as low-priority legacy risk
- **Update 2026-03-31 (staleness review):** The embedded web server and SMTP notifier were fully removed (commit `6a1c440`). This makes GAP_001 (3DES in SMTP), GAP_003 (XSS in web templates), and all TLS/web server findings **stale**. They are kept below for historical reference only.

---

## 1. TLS / SSL / HTTPS

### 1.1 Local Web Server — mbedTLS ~~(GOOD)~~ **[STALE — REMOVED]**

**Status:** The embedded web server and all associated TLS code were fully removed (commit `6a1c440`). This section is kept for historical reference only.

~~**Files:** `srchybrid/PPgWebServer.cpp`, `srchybrid/WebSocket.h`~~

### 1.2 SMTP Email — ~~mbedTLS + Windows Crypto API~~ **[STALE — REMOVED]**

**Status:** The SMTP notifier was fully removed (commit `6a1c440`). GAP_001 is no longer applicable.

~~**Files:** `srchybrid/SendMail.cpp`~~

```cpp
#include "mbedtls/net_sockets.h"
#include "mbedtls/ssl_cache.h"
#include "mbedtls/ssl_ticket.h"
```

- Uses Windows Crypto API (PKCS-7 / S/MIME) for email encryption
- **Triple-DES (3DES/DES3-CBC)** referenced for symmetric encryption
- Certificate lookup via `CertOpenSystemStore(hCryptProv, pszCertStore)` (Windows AddressBook store)

~~**Severity: MEDIUM** — **`GAP_001`** 3DES is deprecated (NIST SP 800-131A Rev. 2 disallows it after 2023). Should be replaced with AES-128 or AES-256.~~ **[STALE — feature removed]**

### 1.3 Protocol Obfuscation — NOT TLS

**Files:** `srchybrid/EncryptedStreamSocket.cpp`, `srchybrid/EncryptedDatagramSocket.cpp`

The "encrypted" stream/datagram sockets do **not** use TLS. They implement a custom handshake for protocol-level obfuscation. This is an explicit design decision documented in the code. See Section 7 for full analysis.

---

## 2. Cryptographic Algorithms

### 2.1 Hash Functions in Use

| Algorithm | File | Purpose | Severity |
|-----------|------|---------|----------|
| **MD4** | `srchybrid/MD4.h` | ed2k file identification hash | CRITICAL |
| **MD5** | `srchybrid/MD5Sum.h`, `EncryptedStreamSocket.cpp` | File ID, RC4 key derivation | CRITICAL |
| **SHA-1** | `srchybrid/SHA.h`, `srchybrid/SHAHashSet.h` | AICH (Advanced Integrity Check Hash) | HIGH |
| **SHA-256** | `srchybrid/PPgWebServer.cpp` | TLS certificate signing | LOW |

All three weak algorithms are imported from Crypto++ and explicitly acknowledged:

```cpp
// MD4.h:22
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
// MD5Sum.h:24
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
// SHA.h:37
#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
```

**Assessment:** MD4 and MD5 are cryptographically broken (collision attacks are practical). However, they are baked into the eDonkey/eMule network protocol — replacing them would break compatibility with the entire network. SHA-1 is deprecated but still used in AICH for similar protocol-legacy reasons.

**Severity: CRITICAL (protocol-inherited, not fixable without protocol break)**

### 2.2 RC4 Stream Cipher

**File:** `srchybrid/EncryptedDatagramSocket.cpp` (lines 175–276)

- RC4 is used for obfuscation of UDP datagrams
- Key derived via MD5 from server/client constants
- RC4 is deprecated and broken (biased keystream, RFC 7465 prohibits it in TLS)
- However, context here is obfuscation-only (hiding traffic patterns), not confidentiality

**Severity: MEDIUM** — Obfuscation purpose is acceptable; would be critical if used for confidentiality.

### 2.3 Diffie-Hellman Key Agreement — 768-bit Prime

**File:** `srchybrid/EncryptedStreamSocket.cpp` (lines 98–110)

```cpp
static unsigned char dh768_p[PRIMESIZE_BYTES] = {
    0xF2,0xBF,0x52,0xC5,0x5F,0x58,0x7A,0xDD,
    ...
};
```

- 768-bit prime, generator 2, exponent size 128 bits
- Used only for the client-to-server obfuscation handshake
- **768-bit DH is broken** — NIST and IETF deprecate anything below 2048 bits
- Comment in code explicitly states this is an obfuscation layer, not a security guarantee

**Severity: MEDIUM** — **`GAP_004`** **[REJECTED]** 768-bit is weak but the protocol is obfuscation-only, not authenticated encryption. A larger DH group was considered and explicitly rejected for this branch because it would break compatibility with older peers.

### 2.4 Crypto++ Usage — Summary

**Good CSPRNG usage:**
```cpp
// srchybrid/ClientCredits.cpp:395, 427, 525
AutoSeededRandomPool rng;
```

**Bad — weak RNG:** See Section 3.

---

## 3. Random Number Generation

### 3.1 `srand(time(NULL))` — Predictable Seed

**File:** `srchybrid/Emule.cpp` (line 304)

```cpp
srand((unsigned)time(NULL));
```

- Time-based seed — predictable to within ~1 second by any observer
- All subsequent `rand()` output is deterministic once seed is known

**Severity: HIGH** — **`BUG_003`**

### 3.2 `rand()` Used for Cryptographic Challenge Values

**File:** `srchybrid/BaseClient.cpp` (lines 2004–2005)

```cpp
uint32 dwRandom = rand() + 1;
m_dwCryptRndChallengeFor = dwRandom;
```

- `m_dwCryptRndChallengeFor` is used in the client authentication challenge
- Using weak `rand()` (seeded with time) for a crypto challenge significantly reduces its security
- The codebase already has `GetRandomUInt32()` backed by `AutoSeededRandomPool` — this should be used here instead

**Severity: HIGH** — **`BUG_002`** **[REJECTED]**

### 3.3 `rand()` for Timing Jitter

**File:** `srchybrid/BaseClient.cpp` (line 237)

```cpp
m_random_update_wait = (DWORD)(rand() % SEC2MS(1));
```

- Used only for scheduling jitter, not cryptographic purposes
- Low risk in context

**Severity: LOW**

---

## 4. Buffer Overflow & Memory Safety

### 4.1 Unbounded `strcpy` — **Single Critical Instance**

**File:** `srchybrid/Emule.cpp` (line 844)

```cpp
strcpy(pGlobalA, strTextA);
```

- No bounds check on the destination buffer
- The rest of the codebase uses `strcpy_s()` extensively (e.g., `AsyncProxySocketLayer.cpp` lines 661, 666, 667, 837)
- This single instance appears to be an oversight

**Severity: HIGH** — **`BUG_001`** Should be replaced with `strcpy_s()` or `strncpy()`.

### 4.2 Packet Size Guard

**File:** `srchybrid/EMSocket.cpp` (line 351)

```cpp
if (reinterpret_cast<Header_Struct*>(rptr)->packetlength - 1 > sizeof GlobalReadBuffer) {
    OnError(ERR_TOOBIG);
    return;
}
```

- Global 2MB read buffer (`static char GlobalReadBuffer[2000000]`)
- Incoming packet length is validated before use
- Properly triggers disconnect on oversized packets

**Severity: LOW** — Properly guarded.

### 4.3 Packet Header Protocol Validation

**File:** `srchybrid/EMSocket.cpp` (lines 340–348)

```cpp
switch (reinterpret_cast<Header_Struct*>(rptr)->eDonkeyID) {
case OP_EDONKEYPROT:
case OP_PACKEDPROT:
case OP_EMULEPROT:
    break;
default:
    OnError(ERR_WRONGHEADER);
    return;
}
```

- Only known protocol identifiers accepted
- Unknown headers cause disconnect

**Severity: LOW** — Properly validated.

### 4.4 Dynamic Memory Allocation

| File | Pattern | Status |
|------|---------|--------|
| `AICHSyncThread.cpp:204` | `new BYTE[nHashCount * CAICHHash::GetHashSize()]` | SAFE — deleted at line 211 |
| `AsyncSocketExLayer.cpp:395` | `new char[nSockAddrLen]()` | SAFE — properly sized |
| `AsyncProxySocketLayer.cpp:453` | `new char[10 + nlen + 1]{}` | SAFE — zero-initialized |

**Severity: LOW** — Dynamic allocation patterns appear properly managed.

### 4.5 Packet Structure Alignment

**File:** `srchybrid/Packets.h`

```cpp
#pragma pack(push, 1)
struct Header_Struct {
    uint8  eDonkeyID;
    uint32 packetlength;
    uint8  command;
};
#pragma pack(pop)
```

- Packed structs used for wire protocol parsing
- Correct use of `#pragma pack` prevents padding-induced misreads

**Severity: LOW** — Properly handled.

---

## 5. Network Security

### 5.1 IP Filtering

**Files:** `srchybrid/IPFilter.h`, `srchybrid/IPFilter.cpp`

- IP range filtering with ban level support
- Per-IP hit counting (`uint32 hits` at IPFilter.h:33)
- `bool IsFiltered(uint32 ip)` gate on all incoming connections

**Severity: LOW** — Properly implemented.

### 5.2 Deprecated Socket API — `inet_addr()`

**Files:**
- `srchybrid/AsyncProxySocketLayer.cpp` (line 732)
- `srchybrid/AsyncSocketEx.cpp` (line 897)

```cpp
sockAddr.sin_addr.s_addr = inet_addr(sAscii);
```

**Status:** **[DONE]** in commit `768559c` — replaced with `InetPtonA()` in the two D-04 socket paths while preserving the existing hostname-resolution fallback.

- `inet_addr()` is deprecated (POSIX.1-2008)
- Returns `-1` (0xffffffff) on error — ambiguous since `255.255.255.255` is a valid address that maps to the same value
- Should be replaced with `InetPton()` (Windows) or `inet_pton()` (POSIX)

**Severity: MEDIUM** — **`GAP_002`**

### 5.3 `SO_REUSEADDR`

**File:** `srchybrid/AsyncSocketEx.cpp` (line 671)

```cpp
SetSockOpt(SO_REUSEADDR, reinterpret_cast<const void*>(&value), sizeof value);
```

- Allows port reuse on restart
- On Windows this is safer than on Linux (no `SO_REUSEPORT` ambiguity)
- Necessary for practical operation

**Severity: LOW** — Acceptable.

### 5.4 Synchronization / Race Conditions

**File:** `srchybrid/EMSocket.cpp` (lines 126–129, 193–203)

```cpp
sendLocker.Lock();
byConnected = EMS_DISCONNECTED;
CleanUpOverlappedSendOperation(true);
sendLocker.Unlock();
```

- Critical sections on socket state and packet queues are properly locked
- No detected unsynchronized access to shared state

**Severity: LOW** — Properly guarded.

---

## 6. Protocol Obfuscation — Detailed Analysis

### 6.1 Client-to-Client Handshake

**File:** `srchybrid/EncryptedStreamSocket.cpp` (lines 18–46)

```
Client A → Client B:
  <SemiRandomNotProtocolMarker 1 byte>
  <RandomKeyPart 4 bytes>
  <MagicValue 4 bytes>
  <EncryptionMethods 1 byte>
  <Preferred 1 byte>
  <Random Padding 0-15 bytes>
```

Key derivation:
```
SendKey = MD5(UserHash || MagicValue(34) || RandomKeyPart)   // 16 bytes
RecvKey = MD5(UserHash || MagicValue(203) || RandomKeyPart)  // 16 bytes
```

First 1024 bytes of RC4 keystream discarded (mitigates known RC4 weakness).

**Issues:**
- MD5 for key derivation (broken)
- RC4 cipher (deprecated)
- Magic values are public protocol constants (security by obscurity)

**Context:** Explicitly documented as obfuscation to disguise P2P traffic from ISPs — not intended to provide cryptographic security.

### 6.2 Client-to-Server Handshake (Diffie-Hellman)

**File:** `srchybrid/EncryptedStreamSocket.cpp` (lines 48–74)

```
Client → Server:
  <SemiRandomNotProtocolMarker 1 byte>
  <G^A 96 bytes (768-bit DH public key)>
  <RandomBytes 0-15 bytes>

Server → Client:
  <G^B 96 bytes>
  <MagicValue 4 bytes>
  <EncMethods 1 byte>
  <Preferred 1 byte>
  <Padding>
```

DH parameters (lines 101–110):
```cpp
static unsigned char dh768_p[96] = { 0xF2, 0xBF, ... }; // 768-bit prime
// Generator: 2, Exponent size: 128 bits
```

**Issues:**
- 768-bit DH prime — factorable with sufficient resources (Logjam attack demonstrated this for 768-bit in research contexts)
- No certificate validation — only obfuscation, not authentication

### 6.3 UDP Obfuscation

**File:** `srchybrid/EncryptedDatagramSocket.cpp` (lines 48–57, 175–276)

Key derivation: `MD5(ServerConstant S (96 bytes) || Magic || MagicValue)`

- RC4 encryption applied per-packet
- No replay protection noted

**Severity:** MEDIUM — Same MD5/RC4 limitations; obfuscation purpose acknowledged.

---

## 7. Input Validation & Injection

### 7.1 URL Parsing

**File:** `srchybrid/AddSourceDlg.cpp` (lines 142–147)

```cpp
TCHAR szScheme[INTERNET_MAX_SCHEME_LENGTH];
TCHAR szHostName[INTERNET_MAX_HOST_NAME_LENGTH];
if (::InternetCrackUrl(...) && Url.dwHostNameLength > 0 &&
    Url.dwHostNameLength < INTERNET_MAX_HOST_NAME_LENGTH)
```

- Uses Windows API with defined length constants
- Properly bounds-checked

**Severity: LOW** — Properly validated.

### 7.2 Web Server Template XSS

**File:** `srchybrid/WebServer.cpp`

- Embedded web server renders HTML templates
- Template variables populated from internal state (file names, search results, peer addresses)
- File names from peers could contain HTML/JS characters
- No explicit HTML-escaping code observed in the agent's scan

~~**Severity: MEDIUM** — **`GAP_003`** If file names or peer-supplied strings are injected into HTML responses without escaping, reflected XSS is possible in the local web UI.~~ **[STALE — WebServer removed]**

### 7.3 Shell Execution

**Files:** `srchybrid/OtherFunctions.cpp` (lines 322, 327, 337), `srchybrid/ArchiveRecovery.cpp` (line 192)

```cpp
ShellExecute(NULL, NULL, lpURL, NULL, lpDirectory, SW_SHOWDEFAULT);
ShellExecuteEx(&SE); // Archive extraction via WinRAR
```

- URL opening delegates to OS shell — no custom command construction
- Archive extraction launches WinRAR on a user-selected file
- No shell metacharacter injection vector identified since paths come from internal state, not raw user text

**Severity: LOW** — Not exploitable via network; user-controlled local operation.

### 7.4 Format String Safety

No `printf`-family calls with unsanitized user-controlled format strings were detected. All format-string arguments are literals or use `%s`/`%d` with explicit variables.

**Severity: LOW** — No findings.

---

## 8. Hardcoded Values & Secrets

### 8.1 Public DH Prime

**File:** `srchybrid/EncryptedStreamSocket.cpp` (lines 101–110)

The 768-bit DH prime is hardcoded. This is **correct by design** — DH primes are public parameters. No secret is exposed.

**Severity: INFO**

### 8.2 Protocol Magic Constants

```cpp
#define MAGICVALUE_REQUESTER    34
#define MAGICVALUE_SERVER       203
#define MAGICVALUE_SYNC         0x835E6FC4
```

Public protocol constants — security by obscurity only, as documented.

**Severity: INFO**

### 8.3 No Hardcoded Passwords or API Keys Found

Scan did not reveal hardcoded credentials, API keys, or private keys in source files.

**Severity: NONE**

---

## 9. Known Issues & TODOs in Code

The following are explicitly flagged in the codebase and represent acknowledged technical debt with security implications:

| File | Line | Issue |
|------|------|-------|
| `srchybrid/ArchiveRecovery.cpp` | 233 | `ASSERT(0); // FIXME` in archive recovery |
| `srchybrid/BaseClient.cpp` | 473, 478, 513 | "Source Exchange — deprecated" |
| `srchybrid/BaseClient.cpp` | 976 | "deprecated — will be set back to 3 with next release" |
| `srchybrid/BaseClient.cpp` | 1458 | "FIXME: We don't know which kad version the buddy has" |
| `srchybrid/ClientList.cpp` | 607 | "TODO 0.49b: Kad buddies won't work with RequireCrypt" |

---

## 10. Consolidated Findings Table

| # | ID | Severity | Category | Finding | Location |
|---|---|----------|----------|---------|----------|
| 1 | — | **CRITICAL** | Hash | MD4 cryptographically broken (protocol-required) | `srchybrid/MD4.h` throughout |
| 2 | — | **CRITICAL** | Hash | MD5 used for key derivation (broken) | `srchybrid/EncryptedStreamSocket.cpp` |
| 3 | **BUG_003** | **HIGH** | RNG | `srand(time(NULL))` — predictable seed | `srchybrid/Emule.cpp:304` |
| 4 | **BUG_002** **[REJECTED]** | **HIGH** | RNG | `rand()` used for cryptographic challenge | `srchybrid/BaseClient.cpp:2004-2005` |
| 5 | **BUG_001** | **HIGH** | Memory | `strcpy()` without bounds | `srchybrid/Emule.cpp:844` |
| 6 | — | **HIGH** | Hash | SHA-1 used in AICH (deprecated) | `srchybrid/SHA.h`, `SHAHashSet.h` |
| 7 | **GAP_004** **[REJECTED]** | **MEDIUM** | DH | 768-bit DH parameters (weak) | `srchybrid/EncryptedStreamSocket.cpp:101-110` |
| 8 | — | **MEDIUM** | Cipher | RC4 deprecated cipher for obfuscation | `srchybrid/EncryptedDatagramSocket.cpp` |
| 9 | **GAP_001** **[STALE]** | ~~MEDIUM~~ | Crypto | ~~3DES (DES3-CBC) in SMTP~~ — SendMail removed | ~~`srchybrid/SendMail.cpp`~~ |
| 10 | **GAP_002** **[DONE]** | **MEDIUM** | Network | `inet_addr()` deprecated API | `srchybrid/AsyncProxySocketLayer.cpp:732`, `AsyncSocketEx.cpp:897` |
| 11 | **GAP_003** **[STALE]** | ~~MEDIUM~~ | Web | ~~Potential XSS in web server templates~~ — WebServer removed | ~~`srchybrid/WebServer.cpp`~~ |
| 12 | — | **LOW** | TLS | Self-signed cert for local web server | `srchybrid/PPgWebServer.cpp` |
| 13 | — | **LOW** | RNG | `rand()` for timing jitter (non-crypto) | `srchybrid/BaseClient.cpp:237` |
| 14 | — | **INFO** | Design | Custom obfuscation protocol (not TLS) | `srchybrid/EncryptedStreamSocket.cpp` |
| 15 | — | **INFO** | Design | Hardcoded public DH prime | `srchybrid/EncryptedStreamSocket.cpp` |

---

## 11. Recommendations

### Priority 1 — Fix Immediately

1. **`BUG_001`** **[DONE]** (commit `0cb4d1e`) — `strcpy(pGlobalA, strTextA)` replaced with bounded alternative.

2. **`BUG_003`** **[ACCEPTED RISK]** — Fix attempted (commit `71e298d`) and intentionally reverted (commit `e9e0be6`). The `srand((unsigned)time(NULL))` seed is kept for legacy `rand()` callers. All crypto-sensitive operations already use `AutoSeededRandomPool`. The remaining `rand()` usage is non-crypto timing jitter only.

3. **`BUG_002`** **[REJECTED]** — Keep the legacy challenge RNG unchanged for this branch.

### Priority 2 — Should Fix

4. **`GAP_001`** ~~**`srchybrid/SendMail.cpp`** — Replace 3DES (DES3-CBC) with AES-128-CBC or AES-256-GCM.~~ **[STALE — SendMail removed]**

5. **`GAP_002`** **[DONE]** (commit `768559c`) — `inet_addr()` replaced with `InetPtonA()` in `srchybrid/AsyncProxySocketLayer.cpp` and `srchybrid/AsyncSocketEx.cpp`.

6. **`GAP_003`** ~~**`srchybrid/WebServer.cpp`** — Audit all template variable insertions to ensure HTML encoding is applied to peer-supplied strings (file names, search results, IP addresses rendered in HTML context).~~ **[STALE — WebServer removed]**

### Priority 3 — Consider for Future Releases

7. **`GAP_004`** **[REJECTED]** — Keep the legacy DH group unchanged for protocol compatibility with older peers.

8. **MD5 key derivation** — Replace MD5 with HKDF-SHA256 for RC4 key derivation when designing the next protocol version.

9. **RC4 cipher** — Replace with ChaCha20 or AES-CTR in any future protocol revision.

10. **SHA-1 (AICH)** — Plan upgrade path to SHA-256 for AICH in a future protocol version.

### Priority 4 — Informational

11. Document the security model of the obfuscation protocol clearly (already partially done in code comments — expand to developer docs).

12. Address FIXME/TODO items related to Kad encryption negotiation (`ClientList.cpp:607`, `BaseClient.cpp:1458`).

13. ~~Add `Content-Security-Policy` and `X-Content-Type-Options` headers to the embedded web server.~~ **[STALE — WebServer removed]**

---

## Appendix: Cryptographic Library Inventory

| Library | Version | Usage | Status |
|---------|---------|-------|--------|
| Crypto++ (`../../eMule-cryptopp/`) | — | DH, MD4, MD5, SHA, AutoSeededRandomPool, RC4 | Active |
| mbedTLS | 3.x (PSA API) | TLS for web server and SMTP | Active, Modern |
| Windows CryptoAPI | System | S/MIME / PKCS-7 email signing | Active, Legacy |

---

*End of report.*
