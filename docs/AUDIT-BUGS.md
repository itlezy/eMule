# eMule Bug Audit Report

**Date:** 2026-03-30
**Branch:** `v0.72a-broadband-dev`
**Scope:** Full static analysis of `srchybrid/` (~188K lines C++/MFC)
**Categories:** Buffer overflows, use-after-free/lifetime, network protocol parsing, resource/GDI leaks, logic/crash bugs

---

## Table of Contents

- [Executive Summary](#executive-summary)
- [Current Triage Status](#current-triage-status)
- [CRITICAL Findings](#critical-findings) (BBUG_001–007)
- [HIGH Findings](#high-findings)
- [MEDIUM Findings](#medium-findings)
- [LOW Findings](#low-findings)

---

## Executive Summary

| Category | CRITICAL | HIGH | MEDIUM | LOW | Total |
|---|---|---|---|---|---|
| Buffer Overflow / Memory Corruption | 4 | 1 | 3 | 2 | 10 |
| Use-After-Free / Lifetime | 5 | 2 | 3 | 2 | 12 |
| Network Protocol Parsing | 3 | 1 | 2 | 0 | 6 |
| Resource / GDI Leaks | 2 | 0 | 4 | 2 | 8 |
| Logic / Crash Bugs | 5 | 3 | 6 | 0 | 14 |
| **Total** | **19** | **7** | **18** | **6** | **50** |

## Current Triage Status

This report captures the original 2026-03-30 audit snapshot. The current tree has already moved since that scan, so use the buckets below as the closure record for what was fixed and what proved stale in the current tree:

### Resolved in current tree

- `BBUG_001` through `BBUG_007` were fixed in the packet/parser hardening pass on 2026-03-30.
- `BBUG_018`, `BBUG_020`, `BBUG_026`, `BBUG_028`, `BBUG_029`, and `BBUG_035` were fixed in the audit-driven guard/test pass on 2026-03-30.
- `BBUG_014`, `BBUG_015`, `BBUG_016`, and `BBUG_043` were fixed in the connected-server snapshot hardening pass on 2026-03-30 by caching `GetCurrentServer()` once per call site before dereferencing it.
- `BBUG_030` through `BBUG_034` were fixed in the GDI/DC cleanup pass on 2026-03-30 by replacing raw desktop-DC ownership with RAII where possible and by consolidating early-return cleanup in the meter-icon drawing helpers.
- `BBUG_036`, `BBUG_037`, and `BBUG_044` were fixed in the runtime-guard cleanup pass on 2026-03-30 by inlining defensive zero-denominator handling in the import-progress callback and by guarding colour-popup parent notifications against dead owner windows.
- `BBUG_038`, `BBUG_039`, `BBUG_040`, and `BBUG_041` were fixed in the UI dialog crash-hardening pass on 2026-03-30 by guarding audited `GetDlgItem()` call sites and by validating the `CPartFile` downcast once before using archive-preview-only methods.
- `BBUG_042`, `BBUG_045`, and `BBUG_046` were fixed in the remaining small-runtime cleanup pass on 2026-03-30 by rewriting the unsigned countdown heap-sort loops in `DownloadQueue.cpp`, by switching the live `Server.cpp` IPv4 literal parse to `TryParseDottedIPv4Literal`, and by replacing the audited `_tcscpy` sites with bounded `_tcscpy_s` calls.
- `BBUG_047` and `BBUG_048` were fixed in the final low-risk resource-cleanup pass on 2026-03-30 by replacing the toolbar desktop-DC probe with `CWindowDC` and by wrapping the captcha generator's temporary GDI/DC ownership in scoped cleanup helpers.
- `BBUG_049` was fixed on 2026-03-30 by making `UDPReaskFNF()` return client liveness and by moving the only remaining delete decision to its caller in `ClientUDPSocket.cpp`.
- `BBUG_013` was fixed on 2026-03-30 by removing socket self-deletion from `CClientReqSocket` and by making `CListenSocket::Process()` own final destruction after the close grace period expires.
- `BBUG_008` and `BBUG_009` were fixed on 2026-03-30 by removing `delete this` from `TryToConnect()`, by making the surviving caller contract explicit, and by deleting failed re-ask sources at the `PartFile` iteration seam instead of inside `CUpDownClient`.
- `BBUG_010`, `BBUG_011`, and `BBUG_012` were fixed on 2026-03-30 by moving upload-entry removal to a two-phase retire path in `CUploadQueue`, by delaying final struct destruction until all overlapped reads complete, and by making the disk thread treat retired entries as inert once the live client pointer has been detached.
- `BBUG_022` was fixed on 2026-03-30 by replacing the `inet_ntoa()`-based `ipstr` wrappers with local stack-buffer formatting that preserves the existing dotted-IPv4 text without using Winsock's static conversion storage.
- The shared `eMule-build-tests` harness now replays serialized packet headers and tag spans for the live parser seam, so the current tree has direct parity/divergence coverage around the packet-header underflow guard plus the tag/blob truncation checks that backstop `BBUG_001`, `BBUG_005`, `BBUG_006`, and `BBUG_028`.
- The shared `eMule-build-tests` harness now also covers the connected-server snapshot seam, so the current tree has direct parity/divergence coverage for the null-snapshot guard that backstops the remaining `GetCurrentServer()` TOCTOU fixes.

### Stale after review

- `BBUG_017` is stale because `srchybrid/WebServer.cpp` was removed with the embedded web server.
- `BBUG_019` is stale after inspection because `CTaskbarNotifier::CreateRgnFromBitmap()` still has a single-owner `pBitmapBits` cleanup path in the current tree, so the audited double-free concern does not materialize as written.
- `BBUG_021` is stale because `srchybrid/SendMail.cpp` was removed with the SMTP notifier and TLS mail path.
- `BBUG_023` is stale after inspection because the audited `CList` erase-during-iteration pattern in `CDownloadQueue::RemoveSource()` is valid for MFC `CList`, and the intervening `CDownloadListCtrl::RemoveSource()` call only mutates UI list state, not the A4AF lists being iterated.
- `BBUG_024` is stale after inspection because `CSharedFileList::FindSharedFiles()` is using a known-safe MFC `CMap::GetNextAssoc()` plus `RemoveKey(key)` pattern in the current container model.
- `BBUG_025` is stale after inspection because `CColourPopup` is self-owned and only heap-allocated from `CColorButton`, which does not retain a popup pointer that could dangle after `OnNcDestroy()`.
- `BBUG_027` is stale because `srchybrid/WebServer.cpp` was removed with the embedded web server.
- `BBUG_050` is stale after inspection because `CDeletedClient` only stores scalar metadata plus a flat per-IP array of `{port, credits}` snapshots, so tracked-entry destruction order does not participate in any observable ownership relationship.

### Deferred architectural work

- No deferred ownership/thread-safety findings remain after the current-tree review.
- The 2026-03-30 audit report is now fully triaged in the current tree: every finding is either fixed or stale.

---

## CRITICAL Findings

---

### BBUG_001: Integer underflow in Packet constructor (network-reachable)

- **Severity:** CRITICAL
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/Packets.cpp:54`
- **Reachability:** Network (TCP) — any peer or server
- **Status:** FIXED on 2026-03-30 by rejecting zero-length packet headers before size derivation.

**Vulnerable Code:**
```cpp
Packet::Packet(char *header)
    : pBuffer()
    , size(reinterpret_cast<Header_Struct*>(header)->packetlength - 1)
```

**Description:**
The `packetlength` field is read directly from network data (from the packet header) and decremented by 1. If `packetlength` is 0, subtracting 1 causes an unsigned integer underflow, wrapping `size` to `0xFFFFFFFF` on 32-bit systems. This enormous size value is subsequently used to allocate buffers:
```cpp
pendingPacket->pBuffer = new char[pendingPacket->size + 1];
```
The `size + 1` wraps back to 0, allocating a tiny buffer that is then used to receive a full packet — classic heap overflow.

**Impact:** Heap overflow or OOM crash from a single malformed TCP packet. Potentially exploitable for remote code execution.

**Fix:** Add `if (packetlength == 0) return;` guard before the subtraction, or use checked arithmetic.

---

### BBUG_002: Integer underflow bypass in EMSocket bounds check

- **Severity:** CRITICAL
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/EMSocket.cpp:335-350`
- **Reachability:** Network (TCP)
- **Status:** FIXED on 2026-03-30 by rejecting zero-length headers before subtracting from `packetlength`.

**Vulnerable Code:**
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

// Security: Check for buffer overflow (2MB)
if (reinterpret_cast<Header_Struct*>(rptr)->packetlength - 1 > sizeof GlobalReadBuffer) {
    OnError(ERR_TOOBIG);
    return;
}
```

**Description:**
The check `packetlength - 1 > sizeof GlobalReadBuffer` attempts to prevent overflow into the 2MB static `GlobalReadBuffer`. However, if `packetlength` is 0, the subtraction `0 - 1` underflows to `0xFFFFFFFF` for unsigned types. But since both sides are unsigned, `0xFFFFFFFF > sizeof GlobalReadBuffer` should actually trigger the error — **unless** `packetlength` is stored as a signed type or cast incorrectly. Additionally, the `-1` operation itself is an off-by-one: the check should be `>=` not `>`.

**Impact:** Buffer over-read/overwrite into static 2MB `GlobalReadBuffer`. Defense-in-depth issue.

**Fix:** Check `packetlength == 0` explicitly before the subtraction. Use `>=` comparison.

---

### BBUG_003: UDP payload length underflow

- **Severity:** CRITICAL
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/UDPSocket.cpp:178-179`
- **Reachability:** Network (UDP) — any peer
- **Status:** FIXED on 2026-03-30 by requiring both UDP header bytes before dispatch.

**Vulnerable Code:**
```cpp
if (pBuffer[0] == OP_EDONKEYPROT)
    ProcessPacket(pBuffer + 2, nPayLoadLen - 2, pBuffer[1], sockAddr.sin_addr.s_addr, ntohs(sockAddr.sin_port));
```

**Description:**
If `nPayLoadLen < 2`, the operation `nPayLoadLen - 2` underflows (UINT wraps to a huge value), causing `ProcessPacket` to attempt processing a massive buffer over-read. The `pBuffer[0]` and `pBuffer[1]` accesses are also unchecked — if the payload is 0 or 1 bytes, these are out-of-bounds reads.

**Impact:** Massive buffer over-read from a single UDP packet. Potential information disclosure or crash.

**Fix:** Add `if (nPayLoadLen < 2) return;` before the switch statement.

---

### BBUG_004: UDP buddy callback size underflow

- **Severity:** CRITICAL
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/ClientUDPSocket.cpp:206-214`
- **Reachability:** Network (UDP)
- **Status:** FIXED on 2026-03-30 for the audited underflow path by deriving the forwarded payload size only after the minimum-size guard.

**Vulnerable Code:**
```cpp
if (size < 17 || buddy->socket == NULL)
    break;
if (md4equ(packet, buddy->GetBuddyID())) {
    PokeUInt32(const_cast<BYTE*>(packet) + 10, ip);
    PokeUInt16(const_cast<BYTE*>(packet) + 14, port);
    Packet *response = new Packet(OP_EMULEPROT);
    response->opcode = OP_REASKCALLBACKTCP;
    response->pBuffer = new char[size];
    memcpy(response->pBuffer, packet + 10, size - 10);
    response->size = size - 10;
```

**Description:**
While there is a `size < 17` check, the `size` variable is UINT. The `memcpy` uses `size - 10` as the copy length. If the check is somehow bypassed (e.g., through a different code path or if the check order changes during refactoring), `size - 10` underflows to a huge value causing a heap buffer overflow. Additionally, `PokeUInt32` at `packet + 10` and `PokeUInt16` at `packet + 14` modify the packet buffer without verifying it is writable.

**Impact:** Heap buffer overflow via crafted buddy callback packet.

**Fix:** Use checked subtraction. Consider `if (size < 17) break;` immediately before the memcpy, not separated by other logic.

---

### BBUG_005: Unchecked tag count DoS (client hello)

- **Severity:** CRITICAL
- **Category:** Network Protocol Parsing
- **File:** `srchybrid/BaseClient.cpp:388-392, 779-783`
- **Reachability:** Network (TCP) — any connecting peer
- **Status:** FIXED on 2026-03-30 by capping hostile tag counts before the per-tag parse loops.

**Vulnerable Code:**
```cpp
uint32 tagcount = data.ReadUInt32();
if (bDbgInfo)
    m_strHelloInfo.AppendFormat(_T("  Tags=%u"), tagcount);
for (uint32 i = 0; i < tagcount; ++i) {
    CTag temptag(data, true);
```

**Description:**
An attacker sends a crafted hello packet with `tagcount = 0xFFFFFFFF`. The loop iterates up to 4 billion times. Each iteration constructs a `CTag` object from the `CSafeMemFile`, which will eventually throw a `CFileException` when data runs out — but not before consuming significant CPU time and allocating memory for partial tag parsing. This enables a Denial-of-Service attack.

The same pattern exists in the MuleInfo handler at lines 779-783.

**Impact:** Single malicious packet freezes the application via CPU/memory exhaustion.

**Fix:** Cap `tagcount` to a sane maximum (e.g., 512 or `data.GetLength() / MIN_TAG_SIZE`).

---

### BBUG_006: Unchecked tag count DoS (server ident)

- **Severity:** CRITICAL
- **Category:** Network Protocol Parsing
- **File:** `srchybrid/ServerSocket.cpp:429-438`
- **Reachability:** Network (TCP) — malicious server
- **Status:** FIXED on 2026-03-30 by capping hostile tag counts before the per-tag parse loop.

**Vulnerable Code:**
```cpp
uint32 nTags = data.ReadUInt32();
for (; nTags; --nTags) {
    CTag tag(data, bUTF8);
```

**Description:**
Same pattern as BBUG_005 but in server packet processing. A malicious server can send an `OP_SERVERIDENT` packet with an extremely large tag count to exhaust client resources.

**Impact:** DoS attack via server packet.

**Fix:** Cap `nTags` to a reasonable maximum.

---

### BBUG_007: Integer overflow in server list bounds check

- **Severity:** CRITICAL
- **Category:** Network Protocol Parsing
- **File:** `srchybrid/ServerSocket.cpp:484`
- **Reachability:** Network (TCP) — malicious server
- **Status:** FIXED on 2026-03-30 by switching the bounds check to division-based arithmetic.

**Vulnerable Code:**
```cpp
CSafeMemFile servers(packet, size);
UINT count = servers.ReadUInt8();
// check if packet is valid
if (1 + count * (4 + 2) <= size) {
```

**Description:**
The multiplication `count * 6` can overflow UINT. If `count = 0x2AAAAAAA` (715,827,882), then `count * 6 = 0x100000000`, which wraps to 0 on 32-bit. The check `1 + 0 <= size` passes for any non-empty packet, allowing the subsequent loop to read far beyond the packet buffer. While `count` is read from a `ReadUInt8()` (max 255), limiting the practical risk, the pattern is still unsafe if the read type ever changes.

**Impact:** Bounds check bypass if count source changes to wider type. Currently mitigated by uint8 read.

**Fix:** Use checked arithmetic: `if (count > (size - 1) / 6)` instead.

---

### BBUG_008: `delete this` in TryToConnect error paths

- **Severity:** CRITICAL
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/BaseClient.cpp:1306,1315,1332,1343,1357,1367,1380`
- **Reachability:** Internal — triggered by connection failures
- **Status:** FIXED on 2026-03-30 by removing `delete this` from the immediate-failure branches and by making the caller-owned liveness contract explicit at the affected seams.

**Vulnerable Code:**
```cpp
if (Disconnected(_T("Too many connections"))) {
    delete this;
    return false;
}
```

**Description:**
`TryToConnect()` has 7 different error paths that call `delete this` followed by `return false`. The method returns `bool` to signal callers that deletion occurred. However:
1. Callers must always check the return value — any oversight causes use-after-free
2. If the object is referenced from multiple containers (download queue, client list, source lists), deletion here creates dangling pointers in all other containers
3. Stack variables and temporary references to `this` become dangling after deletion

**Impact:** Use-after-free if any caller doesn't check the return value or if the object is referenced elsewhere.

**Fix:** Keep the existing `bool` return, but stop deleting from inside `TryToConnect()` and let the call site decide whether to delete immediately.

---

### BBUG_009: Use-after-free in PartFile source iteration

- **Severity:** CRITICAL
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/PartFile.cpp:2351-2352`
- **Reachability:** Internal — triggered during download processing
- **Status:** FIXED on 2026-03-30 by making `AskForDownload()` report caller-owned liveness and by deleting the failed source explicitly at the `PartFile` loop site before breaking.

**Vulnerable Code:**
```cpp
} else if (curTick >= cur_src->GetLastTriedToConnectTime() + MIN2MS(20)) {
    if (!cur_src->AskForDownload()) // NOTE: This may *delete* the client!!
        break;
}
```

**Description:**
`AskForDownload()` can trigger `TryToConnect()` which can `delete this` (see BBUG_008). The code breaks out of the loop when this happens, but:
1. The comment explicitly acknowledges the danger
2. If execution continues past the break (restructured code), the iterator references a deleted object
3. Other threads may access `cur_src` concurrently before the break executes

**Impact:** Crash during download source processing. The existing `break` is a fragile safeguard.

**Fix:** Move the delete to the `PartFile` loop so the iteration site owns the lifetime transition explicitly.

---

### BBUG_010: Destructor dereferences potentially-deleted client

- **Severity:** CRITICAL
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/UploadQueue.cpp:1132-1143`
- **Reachability:** Internal — triggered during upload queue cleanup
- **Status:** FIXED on 2026-03-30 by making upload-entry retirement clear and detach the live client before delayed reclamation, and by guarding the destructor against a missing client pointer.

**Vulnerable Code:**
```cpp
UploadingToClient_Struct::~UploadingToClient_Struct()
{
    m_pClient->FlushSendBlocks();

    m_csBlockListsLock.Lock();
    while (!m_BlockRequests_queue.IsEmpty())
        delete m_BlockRequests_queue.RemoveHead();
    while (!m_DoneBlocks_list.IsEmpty())
        delete m_DoneBlocks_list.RemoveHead();
    m_csBlockListsLock.Unlock();
}
```

**Description:**
When `UploadingToClient_Struct` is destroyed, it unconditionally calls `m_pClient->FlushSendBlocks()`. If the `CUpDownClient` pointed to by `m_pClient` has already been deleted (e.g., via `delete this` in TryToConnect), this dereferences freed memory. The destructor also accesses `m_csBlockListsLock` without verifying object validity.

**Impact:** Use-after-free in upload queue teardown. Could corrupt heap or crash.

**Fix:** Null-check `m_pClient` before calling methods. Consider weak references.

---

### BBUG_011: Upload struct deleted outside lock

- **Severity:** CRITICAL
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/UploadQueue.cpp:773-776`
- **Reachability:** Internal — race between main thread and I/O thread
- **Status:** FIXED on 2026-03-30 by replacing eager deletion with a two-phase retire list that only reclaims upload structs after pending overlapped reads have drained.

**Vulnerable Code:**
```cpp
m_csUploadListMainThrdWriteOtherThrdsRead.Lock();
uploadinglist.RemoveAt(curPos);
m_csUploadListMainThrdWriteOtherThrdsRead.Unlock();
delete curClientStruct;
```

**Description:**
The struct is removed from the list under lock, but `delete curClientStruct` happens after `Unlock()`. The `UploadDiskIOThread` may still hold a reference to this struct obtained before the lock was acquired. When the I/O thread accesses the deleted struct, use-after-free occurs.

**Impact:** Race condition between main thread deletion and I/O thread access — heap corruption or crash.

**Fix:** Either delete under the lock, or use a two-phase deletion with epoch-based reclamation.

---

### BBUG_012: Unprotected m_pClient in DiskIO thread

- **Severity:** CRITICAL
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/UploadDiskIOThread.cpp:96-101`
- **Reachability:** Internal — race condition
- **Status:** FIXED on 2026-03-30 by making the disk thread bail out on retired upload entries, by nulling `m_pClient` during retirement, and by letting completion callbacks discard blocks for entries no longer present in the live upload list.

**Vulnerable Code:**
```cpp
const CUploadingPtrList &rUploadList = theApp.uploadqueue->GetUploadListTS(&pcsUploadListRead);
pcsUploadListRead->Lock();
for (POSITION pos = rUploadList.GetHeadPosition(); pos != NULL;)
    StartCreateNextBlockPackage(rUploadList.GetNext(pos));
InterlockedExchange8(&m_bNewData, 0);
pcsUploadListRead->Unlock();
```

**Description:**
`StartCreateNextBlockPackage()` accesses `UploadingToClient_Struct::m_pClient` from the I/O thread. The critical section protects list iteration but **not** the validity of `m_pClient`. The main thread can delete the client (via `TryToConnect` → `delete this`) while the I/O thread is accessing it through `m_pClient`.

**Impact:** Use-after-free race condition. Particularly dangerous because it involves cross-thread access to a raw pointer.

**Fix:** Add per-client locking or use `shared_ptr`/`weak_ptr` for `m_pClient`.

---

### BBUG_013: Socket `delete this` with delayed timer

- **Severity:** CRITICAL
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/ListenSocket.cpp:189-191`
- **Reachability:** Internal — triggered on socket close
- **Status:** FIXED on 2026-03-30 by moving final socket destruction into `CListenSocket::Process()` after the existing grace delay instead of deleting the socket from `CClientReqSocket`.

**Vulnerable Code:**
```cpp
void CClientReqSocket::OnClose(int nErrorCode)
{
    if (::GetTickCount() >= deltimer + SEC2MS(10))
        delete this;
}
```

**Description:**
`OnClose()` uses a timer-based delayed deletion. Within the 10-second window, other socket messages (`OnReceive`, `OnSend`) can still be dispatched to this socket object. After `delete this`, any queued Windows message targeting this socket's HWND/handle will access freed memory.

**Impact:** Use-after-free on socket objects. Windows message dispatch to deleted objects causes crashes.

**Fix:** Keep the grace delay, but make the listener own final destruction after the socket handle is already closed.

---

### BBUG_014: GetCurrentServer() NULL deref (BaseClient)

- **Severity:** CRITICAL
- **Category:** Logic / Crash
- **File:** `srchybrid/BaseClient.cpp:1048-1049`
- **Reachability:** Internal — disconnect race condition
- **Status:** FIXED on 2026-03-30 by caching `GetCurrentServer()` once before reading the server endpoint.

**Vulnerable Code:**
```cpp
if (theApp.serverconnect->IsConnected()) {
    dwIP = theApp.serverconnect->GetCurrentServer()->GetIP();
    nPort = theApp.serverconnect->GetCurrentServer()->GetPort();
```

**Description:**
`GetCurrentServer()` can return NULL between the `IsConnected()` check and the dereference. In a multi-threaded app, another thread can disconnect the server after `IsConnected()` returns true but before `GetCurrentServer()` is called — classic TOCTOU (time-of-check-time-of-use) race.

**Impact:** NULL pointer dereference crash.

**Fix:** Cache `GetCurrentServer()` in a local variable: `if (CServer *pSrv = GetCurrentServer()) { dwIP = pSrv->GetIP(); ... }`

---

### BBUG_015: GetCurrentServer() NULL deref (Emule.cpp)

- **Severity:** CRITICAL
- **Category:** Logic / Crash
- **File:** `srchybrid/Emule.cpp:893,900,907`
- **Reachability:** Internal — disconnect race condition
- **Status:** FIXED on 2026-03-30 by caching `GetCurrentServer()` once before reading the online-signature server fields.

**Vulnerable Code:**
```cpp
strBuff = serverconnect->GetCurrentServer()->GetListName();
strBuff = serverconnect->GetCurrentServer()->GetAddress();
_itoa(serverconnect->GetCurrentServer()->GetPort(), buffer, 10);
```

**Description:**
Three consecutive unguarded `GetCurrentServer()` dereferences. Even if the first succeeds, the server can disconnect before the second or third call.

**Impact:** NULL deref crash during server info retrieval.

**Fix:** Cache the server pointer once and null-check it.

---

### BBUG_016: GetCurrentServer() NULL deref (PartFile)

- **Severity:** CRITICAL
- **Category:** Logic / Crash
- **File:** `srchybrid/PartFile.cpp:2423`
- **Reachability:** Internal — disconnect race condition
- **Status:** FIXED on 2026-03-30 by caching `GetCurrentServer()` once before comparing the low-ID server identity.

**Vulnerable Code:**
```cpp
if (theApp.serverconnect->GetClientID() == userid
    && theApp.serverconnect->GetCurrentServer()->GetIP() == serverip
    && theApp.serverconnect->GetCurrentServer()->GetPort() == serverport)
```

**Description:**
Two separate `GetCurrentServer()` calls in a chained `&&` expression. Short-circuit evaluation doesn't help — if the first `GetCurrentServer()` returns non-NULL but the server disconnects before the second call, the second returns NULL and is dereferenced.

**Impact:** NULL deref crash in source evaluation.

**Fix:** Cache the pointer.

---

### BBUG_017: GetCurrentServer() NULL deref (WebServer)

- **Severity:** CRITICAL
- **Category:** Logic / Crash
- **File:** `srchybrid/WebServer.cpp:776-777, 1215`
- **Reachability:** Network (HTTP) — web interface handler
- **Status:** STALE on 2026-03-30 because the embedded web server was removed from the current tree.

**Vulnerable Code:**
```cpp
theApp.serverconnect->GetCurrentServer()->GetAddress(),
theApp.serverconnect->GetCurrentServer()->GetPort());
```

**Description:**
Direct dereference without NULL check in web server handler code. This runs on the web server thread, making the TOCTOU race even more likely since the main thread manages server connections.

**Impact:** Crash in web server handler, potentially taking down the HTTP interface.

**Fix:** Cache the pointer.

---

### BBUG_018: Division by zero in KnownFile progress (ASSERT-only guard)

- **Severity:** CRITICAL
- **Category:** Logic / Crash
- **File:** `srchybrid/KnownFile.cpp:454`
- **Reachability:** Internal — hashing zero-size files
- **Status:** FIXED on 2026-03-30 by switching the progress update to a bounded helper that treats zero-size files as 0% instead of dividing in Release builds.

**Vulnerable Code:**
```cpp
ASSERT(reinterpret_cast<CKnownFile*>(pvProgressParam)->GetFileSize() == GetFileSize());
WPARAM uProgress = (WPARAM)(100 - (togo * 100) / (uint64)GetFileSize());
```

**Description:**
The only protection against `GetFileSize() == 0` is the ASSERT on the previous line, which is compiled out in Release builds. If a zero-size file is hashed, this causes integer division by zero — a crash.

**Impact:** Crash on zero-size file. ASSERT-only validation means Release builds are unprotected.

**Fix:** Add `if (GetFileSize() == 0) return;` guard.

---

### BBUG_019: Double-free in TaskbarNotifier bitmap handling

- **Severity:** CRITICAL
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/TaskbarNotifier.cpp:504-623`
- **Reachability:** Internal — UI notification code
- **Status:** STALE on 2026-03-30 after inspection because the current-tree `pBitmapBits` cleanup is still single-owner and does not actually double-free the buffer.

**Vulnerable Code:**
```cpp
// Line ~537: inside conditional
if (!m_bBitmapAlpha) {
    free((void*)pBitmapBits);
    pBitmapBits = NULL;
}

// ... ~80 lines of code ...

// Line 623: unconditional
free((void*)pBitmapBits);
```

**Description:**
When `m_bBitmapAlpha` is **false**, `pBitmapBits` is freed and set to NULL at line ~537 (safe — free(NULL) is a no-op at line 623). But when `m_bBitmapAlpha` is **true**, the early free is skipped, and the pointer remains valid until line 623 where it is freed — this path is correct. **However**, if there is an error between line 537 and line 623 that causes `pBitmapBits` to be freed via a different path, double-free occurs. The code structure makes this fragile.

**Actually reviewing more carefully:** When `m_bBitmapAlpha` is false, `pBitmapBits` is freed and set to NULL. The free at line 623 then calls `free(NULL)` which is safe. When `m_bBitmapAlpha` is true, `pBitmapBits` is NOT freed early, so line 623 frees it once. This appears correct but the code structure is extremely fragile — any future modification between these two points could introduce a double-free.

**Impact:** Currently safe but extremely fragile pattern prone to regression. Code review flagged as high-risk maintenance hazard.

**Fix:** Use a single cleanup point with RAII or goto-cleanup pattern.

---

## HIGH Findings

---

### BBUG_020: UDP decompression size edge case

- **Severity:** HIGH
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/ClientUDPSocket.cpp:88-91`
- **Reachability:** Network (UDP)
- **Status:** FIXED on 2026-03-30 by requiring at least one compressed payload byte beyond the UDP header before dispatching the inflater.

**Vulnerable Code:**
```cpp
case OP_EMULEPROT:
    if (nPacketLen < 2)
        strError = _T("eMule packet too short");
    else
        ProcessPacket(pBuffer + 2, nPacketLen - 2, pBuffer[1], ...);
    break;
case OP_KADEMLIAPACKEDPROT:
    if (nPacketLen < 2)
        strError = _T("Kad packet (compressed) too short");
    else {
        iZLibResult = uncompress(unpack + 2, &unpackedsize, pBuffer + 2, nPacketLen - 2);
```

**Description:**
When `nPacketLen == 2`, `nPacketLen - 2 == 0` which is valid but degenerate — passes 0 bytes to `uncompress()`. If `nPacketLen < 2` check is bypassed via a different entry point or if the variable type changes, unsigned underflow occurs. The check exists but is fragile.

**Impact:** Potential buffer over-read if size check is bypassed.

**Fix:** Consider `nPacketLen <= 2` to reject degenerate cases.

---

### BBUG_021: sprintf into fixed 1024-byte buffer (TLS ciphersuite)

- **Severity:** HIGH
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/SendMail.cpp:377`
- **Reachability:** Network (TLS indirect)
- **Status:** STALE on 2026-03-30 because the SMTP notifier and `SendMail.cpp` were removed from the current tree.

**Vulnerable Code:**
```cpp
unsigned char base[1024];
n = sprintf((char*)base, "%s\n", mbedtls_ssl_get_ciphersuite(&ssl));
```

**Description:**
`mbedtls_ssl_get_ciphersuite()` returns a string from the TLS library. While cipher suite names are typically short (~30-60 chars), there is no length restriction enforced. If the TLS library returns an unexpectedly long string (future library version, corrupted state), it overflows the 1024-byte stack buffer.

**Impact:** Stack buffer overflow. Potentially exploitable via TLS downgrade/negotiation attacks.

**Fix:** Use `snprintf(base, sizeof(base), ...)`.

---

### BBUG_022: inet_ntoa thread-unsafe static buffer

- **Severity:** HIGH
- **Category:** Network Protocol Parsing
- **File:** `srchybrid/OtherFunctions.cpp:2520,2525`
- **Reachability:** All threads — pervasive use
- **Status:** FIXED on 2026-03-30 by replacing the `inet_ntoa()` wrappers with local buffer formatting that preserves the existing dotted-IPv4 output without using Winsock's shared conversion storage.

**Vulnerable Code:**
```cpp
CString ipstr(uint32 nIP)
{
    return CString(inet_ntoa(*(in_addr*)&nIP));
}

CStringA ipstrA(uint32 nIP)
{
    return CStringA(inet_ntoa(*(in_addr*)&nIP));
}
```

**Description:**
`inet_ntoa()` returns a pointer to a thread-local static buffer on Windows (actually per-thread on modern Windows, but per-process on older versions). In a heavily multithreaded application like eMule, concurrent calls from different threads can overwrite each other's results, leading to:
- Wrong IP addresses in logs
- Wrong IPs used for banning/filtering decisions
- Corrupted server/peer addresses

**Impact:** Thread-safety violation causing incorrect IP handling across the application.

**Fix:** Replace with `inet_ntop()` or `InetNtop()` with a caller-provided buffer.

---

### BBUG_023: List erase during iteration (DownloadQueue)

- **Severity:** HIGH
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/DownloadQueue.cpp:639-648`
- **Reachability:** Internal — A4AF source processing
- **Status:** STALE on 2026-03-30 after inspection because the current `CList` iteration/removal pattern is valid for MFC `CList`, and the intervening UI removal call does not mutate the iterated source lists.

**Vulnerable Code:**
```cpp
for (POSITION pos = toremove->m_OtherRequests_list.GetHeadPosition(); pos != NULL;) {
    const POSITION pos1 = pos;
    CPartFile *pfile = toremove->m_OtherRequests_list.GetNext(pos);
    POSITION pos2 = pfile->A4AFsrclist.Find(toremove);
    if (pos2) {
        pfile->A4AFsrclist.RemoveAt(pos2);
        theApp.emuledlg->transferwnd->GetDownloadList()->RemoveSource(toremove, pfile);
        toremove->m_OtherRequests_list.RemoveAt(pos1);
    }
}
```

**Description:**
While iterating `m_OtherRequests_list`, the code calls `RemoveAt(pos1)` on the same list. `pos` was obtained via `GetNext()` before the removal, so it should still be valid for `CList` (which uses linked list nodes). However, if `GetNext()` returns the last element, `pos` becomes NULL and the loop exits correctly. **The real risk** is that `RemoveSource` could trigger side effects that further modify `m_OtherRequests_list`, invalidating the saved `pos`.

**Impact:** Potential iterator invalidation if `RemoveSource` has side effects on the list.

**Fix:** Collect elements to remove in a separate list, then remove after iteration.

---

### BBUG_024: Iterator invalidation in SharedFileList map

- **Severity:** HIGH
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/SharedFileList.cpp:800-814`
- **Reachability:** Internal — file list cleanup
- **Status:** STALE on 2026-03-30 after inspection because the current-tree MFC `CMap` iteration uses a known-safe `GetNextAssoc()` plus `RemoveKey(key)` pattern.

**Vulnerable Code:**
```cpp
for (POSITION pos = m_Files_map.GetStartPosition(); pos != NULL;) {
    CKnownFile *cur_file;
    m_Files_map.GetNextAssoc(pos, key, cur_file);
    // ... condition check ...
    m_Files_map.RemoveKey(key);
}
```

**Description:**
`CMap::RemoveKey()` during `GetNextAssoc()` iteration. For MFC's `CMap` (hash table), `GetNextAssoc` retrieves the next position before `RemoveKey` is called, so the saved `pos` should remain valid. This is actually a known-safe pattern for MFC `CMap` (unlike `std::unordered_map`). **However**, it's fragile and non-obvious — future migration to STL containers would break this.

**Impact:** Safe for MFC `CMap` but a maintenance hazard. Would crash if migrated to STL.

**Fix:** Add a comment documenting why this is safe, or collect keys and remove after iteration.

---

### BBUG_025: CColourPopup `delete this` in OnNcDestroy

- **Severity:** HIGH
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/ColourPopup.cpp:392-396`
- **Reachability:** Internal — UI popup destruction
- **Status:** STALE on 2026-03-30 after inspection because `CColourPopup` is still self-owned and only spawned from `CColorButton`, which keeps no popup pointer that could dangle after `OnNcDestroy()`.

**Vulnerable Code:**
```cpp
void CColourPopup::OnNcDestroy()
{
    CWnd::OnNcDestroy();
    delete this;
}
```

**Description:**
Auto-deletion pattern in `WM_NCDESTROY` handler. The popup can be destroyed via `DestroyWindow()` (line 711), parent destruction, or system cleanup. After `delete this`, any code that still holds a pointer to this `CColourPopup` object (e.g., the parent window's member variable) becomes a dangling pointer.

**Impact:** Dangling pointer if parent still references the popup after destruction.

**Fix:** Null out the parent's reference before `delete this`, or use the `PostNcDestroy` idiom properly.

---

### BBUG_026: Division by zero in SharedFileList import progress

- **Severity:** HIGH
- **Category:** Logic / Crash
- **File:** `srchybrid/SharedFileList.cpp:427`
- **Reachability:** Internal — part file import
- **Status:** FIXED on 2026-03-30 by routing the import progress update through the same bounded percentage helper used by other guard-safe progress paths.

**Vulnerable Code:**
```cpp
WPARAM uProgress = (WPARAM)(i * 100 / m_PartsToImport.GetSize());
```

**Description:**
No validation that `m_PartsToImport.GetSize() > 0` before division. If the import list is empty (e.g., all imports failed or were removed), this causes integer division by zero.

**Impact:** Crash during import progress calculation.

**Fix:** Guard with `if (m_PartsToImport.GetSize() > 0)`.

---

## MEDIUM Findings

---

### BBUG_027: _stprintf with fixed buffer in WebServer

- **Severity:** MEDIUM
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/WebServer.cpp:2349-2353, 2387-2391, 2426-2430`
- **Reachability:** Internal — web interface rendering
- **Status:** STALE on 2026-03-30 because the embedded web server was removed from the current tree.

**Description:**
Multiple instances of `TCHAR HTTPTempC[20]` with `_stprintf(HTTPTempC, _T("%i"), ...)`. While `%i` with a 32-bit int fits in 20 chars, the pattern is unsafe — no bounds checking. Other instances at lines 2982-3001 use 100-char buffers with string formatting that could overflow with very long CastItoXBytes results.

**Fix:** Replace with `_stprintf_s` or `StringCchPrintf`.

---

### BBUG_028: Blob allocation without overflow check

- **Severity:** MEDIUM
- **Category:** Network Protocol Parsing
- **File:** `srchybrid/Packets.cpp:505-508`
- **Reachability:** Network (TCP)
- **Status:** FIXED on 2026-03-30 by guarding the blob-length read with an explicit `position <= length` check before subtracting the remaining bytes.

**Vulnerable Code:**
```cpp
m_nBlobSize = data.ReadUInt32();
if (m_nBlobSize <= data.GetLength() - data.GetPosition()) {
    m_pData = new BYTE[m_nBlobSize];
```

**Description:**
The bounds check `data.GetLength() - data.GetPosition()` could underflow if `GetPosition() > GetLength()` due to prior bugs. Also, no check for `new` allocation failure with very large `m_nBlobSize` values (throws `bad_alloc`).

**Fix:** Check `GetPosition() <= GetLength()` first. Consider `nothrow new` with explicit check.

---

### BBUG_029: inet_addr ambiguity with 255.255.255.255

- **Severity:** MEDIUM
- **Category:** Network Protocol Parsing
- **File:** `srchybrid/ED2KLink.cpp:293-294`
- **Reachability:** User input — ed2k link parsing
- **Status:** FIXED on 2026-03-30 by replacing the ambiguous `inet_addr()` literal parse with an explicit dotted-IPv4 parser that accepts the broadcast address.

**Vulnerable Code:**
```cpp
unsigned long dwID = inet_addr(sIPa);
if (dwID == INADDR_NONE) {  // host name?
```

**Description:**
`inet_addr()` returns `INADDR_NONE` (0xFFFFFFFF) for both invalid input AND the valid IP "255.255.255.255". The code treats `INADDR_NONE` as "not an IP, try DNS" — so a source address of 255.255.255.255 in an ed2k link would trigger unnecessary DNS resolution instead of being used as an IP.

**Fix:** Replace with `inet_pton()` which returns a distinct error code.

---

### BBUG_030: MeterIcon GDI leak on error paths

- **Severity:** MEDIUM
- **Category:** Resource Leak
- **File:** `srchybrid/MeterIcon.cpp:57-95`
- **Reachability:** Internal — called during icon refresh
- **Status:** FIXED on 2026-03-30 by routing the meter icon and bar-drawing failure paths through a single cleanup block that restores selected objects and frees every created DC/bitmap/brush/pen exactly once.

**Vulnerable Code:**
```cpp
HDC hScreenDC = ::GetDC(HWND_DESKTOP);
HDC hIconDC = ::CreateCompatibleDC(hScreenDC);
HDC hMaskDC = ::CreateCompatibleDC(hScreenDC);

if (hScreenDC == NULL || hIconDC == NULL || hMaskDC == NULL)
    return NULL;  // LEAK: partially created DCs not released

iiNewIcon.hbmColor = ::CreateCompatibleBitmap(hScreenDC, ...);
if (iiNewIcon.hbmColor == NULL)
    return NULL;  // LEAK: all three DCs leak

// ... 5+ more early return points with the same problem
```

**Description:**
At least 5 error paths return without releasing created DCs and bitmaps. The function is called whenever system tray meter icons need updating, potentially thousands of times.

**Impact:** Progressive GDI handle exhaustion, eventually causing UI rendering failures or system instability.

**Fix:** Use RAII wrappers or a single cleanup label (goto/structured).

---

### BBUG_031: TreeOptionsCtrlEx screen DC leak

- **Severity:** MEDIUM
- **Category:** Resource Leak
- **File:** `srchybrid/TreeOptionsCtrlEx.cpp:214-346`
- **Reachability:** Internal — control initialization
- **Status:** FIXED on 2026-03-30 by switching the temporary desktop DC to `CWindowDC`, which releases the screen DC automatically on every exit path.

**Description:**
`CDC::FromHandle(::GetDC(HWND_DESKTOP))` wraps the screen DC in an MFC CDC object. The `ReleaseDC` at line 346 attempts release via CDC pointer dereference, but the pattern is fragile. If any exception occurs in the 130-line block between GetDC and ReleaseDC, the DC leaks.

**Fix:** Use `CClientDC(CWnd::GetDesktopWindow())` which auto-releases in destructor.

---

### BBUG_032: HTRichEditCtrl DC leak in exception paths

- **Severity:** MEDIUM
- **Category:** Resource Leak
- **File:** `srchybrid/HTRichEditCtrl.cpp:1166-1170`
- **Reachability:** Internal — rich edit rendering
- **Status:** FIXED on 2026-03-30 by replacing the raw desktop-DC calls in the smiley/icon conversion helpers with `CWindowDC`.

**Description:**
Inside a `USE_METAFILE` block, `GetDC(HWND_DESKTOP)` is called with complex control flow before `ReleaseDC`. An exception after GetDC but before ReleaseDC leaks the handle.

**Fix:** RAII wrapper for the DC.

---

### BBUG_033: TitledMenu Attach/Detach DC leak on exception

- **Severity:** MEDIUM
- **Category:** Resource Leak
- **File:** `srchybrid/TitledMenu.cpp:113-119`
- **Reachability:** Internal — menu rendering
- **Status:** FIXED on 2026-03-30 by replacing the manual `Attach`/`Detach` desktop DC management with `CWindowDC`.

**Description:**
```cpp
CDC dc;
dc.Attach(::GetDC(HWND_DESKTOP));
// ... operations that could throw ...
::ReleaseDC(NULL, dc.Detach());
```
If an exception occurs between Attach and Detach, the DC is never released. CDC's destructor calls `DeleteDC` (wrong for a GetDC handle), not `ReleaseDC`.

**Fix:** Use `CWindowDC` or explicit try/catch.

---

### BBUG_034: EnBitmap early return bypasses ReleaseDC

- **Severity:** MEDIUM
- **Category:** Resource Leak
- **File:** `srchybrid/EnBitmap.cpp:176-204`
- **Reachability:** Internal — bitmap loading
- **Status:** FIXED on 2026-03-30 by switching the desktop DC used for `Attach(IPicture*)` to `CWindowDC`, so early exits no longer bypass `ReleaseDC`.

**Description:**
If `CreateCompatibleBitmap` at line 188 fails or picture rendering fails, early return paths bypass the `ReleaseDC` at line 202.

**Fix:** RAII wrapper or restructure error handling.

---

### BBUG_035: Division by zero in EmuleDlg progress bar

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/EmuleDlg.cpp:3523`
- **Reachability:** Internal — UI timer handler
- **Status:** FIXED on 2026-03-30 by clamping the taskbar progress ratio to `0.0f` when the global-size denominator is empty.

**Vulnerable Code:**
```cpp
float overallProgress = globalDone / globalSize;
```

**Description:**
If `globalSize` is 0 (no active downloads, or all downloads are zero-size), floating-point division by zero produces `inf` or `NaN`. While not a crash on x86 (IEEE 754), using `NaN`/`inf` in subsequent integer conversions or UI operations may produce unexpected results.

**Fix:** Guard with `if (globalSize > 0)`.

---

### BBUG_036: Division by zero in PartFile progress

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/PartFile.cpp:2791`
- **Reachability:** Internal — import progress
- **Status:** FIXED on 2026-03-30 by keeping the zero-size guard inline with the progress percentage calculation.

**Vulnerable Code:**
```cpp
WPARAM uProgress = (WPARAM)(TotalBytesTransferred.QuadPart * 100 / TotalFileSize.QuadPart);
```

**Description:**
Guarded by `TotalFileSize.QuadPart != 0` check on line 2790, but if the check is refactored away or if the condition is reordered, zero division occurs.

**Fix:** Consider defensive check inline: `TotalFileSize.QuadPart ? ... : 0`.

---

### BBUG_037: ColourPopup division by zero

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/ColourPopup.cpp:490,501,585`
- **Reachability:** Internal — color picker UI
- **Status:** FIXED on 2026-03-30 by adding runtime guards for `m_nNumColumns <= 0` plus an explicit `ASSERT(m_nNumColumns > 0)` at layout time.

**Vulnerable Code:**
```cpp
return nIndex / m_nNumColumns;    // line 490
return nIndex % m_nNumColumns;    // line 501
m_nNumRows = m_nNumColours / m_nNumColumns;  // line 585
```

**Description:**
`m_nNumColumns` is set to 8 on line 584, but if the constructor or initialization is changed, division by zero occurs. No defensive check.

**Fix:** Add `ASSERT(m_nNumColumns > 0)` and a runtime guard.

---

### BBUG_038: GetDlgItem NULL deref in ArchivePreviewDlg

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/ArchivePreviewDlg.cpp:1070,183,185`
- **Reachability:** Internal — archive preview dialog
- **Status:** FIXED on 2026-03-30 by guarding the reduced-layout and progress-bar `GetDlgItem()` call sites before using the returned controls.

**Vulnerable Code:**
```cpp
tp->progressHwnd = GetDlgItem(IDC_ARCHPROGRESS)->m_hWnd;  // line 1070
GetDlgItem(IDC_APV_FILEINFO)->GetWindowRect(rc);            // line 183
GetDlgItem(IDC_RESTOREARCH)->GetWindowRect(rc);             // line 185
```

**Description:**
`GetDlgItem()` returns NULL if the control doesn't exist. Direct member access without NULL check crashes if the dialog template is modified or if controls are dynamically created.

**Fix:** NULL-check or use `VERIFY(GetDlgItem(...))`.

---

### BBUG_039: GetDlgItem NULL deref in PPgDirectories

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/PPgDirectories.cpp:70,72,75`
- **Reachability:** Internal — preferences dialog
- **Status:** FIXED on 2026-03-30 by guarding the audited edit, buddy-button, and balloon-tip control lookups before dereferencing them.

**Vulnerable Code:**
```cpp
static_cast<CEdit*>(GetDlgItem(IDC_INCFILES))->SetLimitText(MAX_PATH);
AddBuddyButton(GetDlgItem(IDC_INCFILES)->m_hWnd, ...);
AddBuddyButton(GetDlgItem(IDC_TEMPFILES)->m_hWnd, ...);
```

**Description:**
Multiple unchecked `GetDlgItem()` calls with direct member access and unsafe `static_cast`.

**Fix:** NULL-check each call.

---

### BBUG_040: GetDlgItem NULL deref in AddFriend

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/AddFriend.cpp:86`
- **Reachability:** Internal — friend dialog
- **Status:** FIXED on 2026-03-30 by guarding the username edit lookup before applying the nickname length limit.

**Vulnerable Code:**
```cpp
static_cast<CEdit*>(GetDlgItem(IDC_USERNAME))->SetLimitText(thePrefs.GetMaxUserNickLength());
```

**Description:**
Unsafe `static_cast` of potentially NULL pointer from `GetDlgItem()`.

**Fix:** NULL-check.

---

### BBUG_041: Unsafe static_cast without type validation

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/ArchivePreviewDlg.cpp:1002-1004`
- **Reachability:** Internal — archive preview
- **Status:** FIXED on 2026-03-30 by validating the `CPartFile` runtime type once with `IsKindOf(RUNTIME_CLASS(CPartFile))` before enabling restore/archive-preview-only paths.

**Vulnerable Code:**
```cpp
GetDlgItem(IDC_RESTOREARCH)->EnableWindow(pFile->IsPartFile()
    && static_cast<CPartFile*>(pFile)->IsArchive(true)
    && static_cast<CPartFile*>(pFile)->IsReadyForPreview());
```

**Description:**
`static_cast<CPartFile*>` is used after a behavioral check `IsPartFile()`, not a type-safe check. If `IsPartFile()` returns true but the object is not actually `CPartFile`, undefined behavior occurs. Should use `dynamic_cast` or `RUNTIME_CLASS`.

**Fix:** Use `dynamic_cast<CPartFile*>` with NULL check.

---

### BBUG_042: UINT underflow in download queue loop

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/DownloadQueue.cpp:951`
- **Reachability:** Internal — download queue processing
- **Status:** FIXED on 2026-03-30 by replacing the post-decrement countdown loops with explicit non-underflow index ranges.

**Vulnerable Code:**
```cpp
for (UINT i = n / 2; i--;)
```

**Description:**
When `n == 0` or `n == 1`, `i = 0`. The post-decrement `i--` evaluates to 0 (falsy, loop doesn't execute), then `i` wraps to `UINT_MAX`. This is actually correct for the loop condition (post-decrement returns old value), but the pattern is confusing and error-prone.

**Fix:** Use clearer loop: `for (UINT i = n / 2; i > 0; --i)`.

---

### BBUG_043: TOCTOU race in SearchResultsWnd

- **Severity:** MEDIUM
- **Category:** Logic / Crash
- **File:** `srchybrid/SearchResultsWnd.cpp:1211-1212`
- **Reachability:** Internal — search UI
- **Status:** FIXED on 2026-03-30 by caching `GetCurrentServer()` once before reading the large-file and related-search capability flags.

**Vulnerable Code:**
```cpp
bool bServerSupports64Bit = theApp.serverconnect->GetCurrentServer() != NULL
    && (theApp.serverconnect->GetCurrentServer()->GetTCPFlags() & SRV_TCPFLG_LARGEFILES);
```

**Description:**
Two separate `GetCurrentServer()` calls. Server could disconnect between the NULL check (first call) and the flag read (second call).

**Fix:** Cache in a local variable.

---

### BBUG_044: SendMessage to potentially destroyed parent window

- **Severity:** MEDIUM
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/ColourPopup.cpp:686,710`
- **Reachability:** Internal — color picker UI
- **Status:** FIXED on 2026-03-30 by sending popup notifications only when the cached parent pointer still owns a live window handle.

**Vulnerable Code:**
```cpp
m_pParent->SendMessage(UM_CPN_SELCHANGE, (WPARAM)Colour, 0);
m_pParent->SendMessage(nMessage, (WPARAM)m_crColour, 0);
```

**Description:**
If the parent window has been destroyed, `SendMessage` to its HWND causes undefined behavior. The `m_pParent` pointer may be valid as a C++ object but with an invalid `m_hWnd`.

**Fix:** Check `IsWindow(m_pParent->m_hWnd)` before SendMessage.

---

## LOW Findings

---

### BBUG_045: _tcscpy with hardcoded font name literals

- **Severity:** LOW
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/CreditsThread.cpp:301,316,332,346`
- **Reachability:** Internal — credits dialog
- **Status:** FIXED on 2026-03-30 by switching the font face-name copies to bounded `_tcscpy_s` calls.

**Description:**
`_tcscpy(lf.lfFaceName, _T("Arial"))` — safe because "Arial" (5 chars + null) fits in `lfFaceName` (32 chars), but the pattern is dangerous. Should use `_tcscpy_s` or `_tcsncpy`.

---

### BBUG_046: _tcscpy for IP address buffer

- **Severity:** LOW
- **Category:** Buffer Overflow / Memory Corruption
- **File:** `srchybrid/Server.cpp:32,79,257`
- **Reachability:** Internal
- **Status:** FIXED on 2026-03-30 by switching the cached IP-string copies to bounded `_tcscpy_s` calls while also routing the live literal parse through `TryParseDottedIPv4Literal`.

**Description:**
`_tcscpy(ipfull, ipstr(ip))` — IPv4 addresses fit, but `_tcscpy` offers no bounds protection if the source changes.

**Fix:** Use `_tcscpy_s`.

---

### BBUG_047: GetDC/ReleaseDC HWND parameter mismatch

- **Severity:** LOW
- **Category:** Resource Leak
- **File:** `srchybrid/ToolBarCtrlX.cpp:140-142`
- **Reachability:** Internal — toolbar rendering
- **Status:** FIXED on 2026-03-30 by switching the desktop metrics probe to `CWindowDC`, which removes the mismatched raw `GetDC`/`ReleaseDC` pair entirely.

**Description:**
`GetDC(HWND_DESKTOP)` paired with `ReleaseDC(NULL, hDC)`. While both resolve to the desktop DC on Windows, the HWND parameter mismatch is technically incorrect per MSDN.

**Fix:** Use consistent HWND parameter.

---

### BBUG_048: CaptchaGenerator cleanup on exception path

- **Severity:** LOW
- **Category:** Resource Leak
- **File:** `srchybrid/CaptchaGenerator.cpp:71-114`
- **Reachability:** Internal — captcha generation
- **Status:** FIXED on 2026-03-30 by wrapping the temporary captcha bitmaps, font, memory DCs, and selected-object restoration in scoped cleanup helpers.

**Description:**
DCs and GDI objects created between lines 71-76 are cleaned up at lines 107-114. If an exception occurs in the drawing code (lines 78-106), cleanup is skipped.

**Fix:** RAII wrappers for GDI objects.

---

### BBUG_049: `delete this` return value ignored by caller

- **Severity:** LOW
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/DownloadClient.cpp:1325-1327`
- **Reachability:** Internal
- **Status:** FIXED on 2026-03-30 by making `UDPReaskFNF()` report whether the client survived and by letting `ClientUDPSocket.cpp` perform the only remaining delete after the call returns.

**Vulnerable Code:**
```cpp
theApp.downloadqueue->RemoveSource(this);
if (!socket && Disconnected(_T("UDPReaskFNF socket=NULL")))
    delete this;
```

**Description:**
The function that contains this code doesn't reliably signal to callers that `this` was deleted. Any caller that accesses the object after this function returns risks use-after-free.

---

### BBUG_050: CDeletedClient map cleanup ordering

- **Severity:** LOW
- **Category:** Use-After-Free / Lifetime
- **File:** `srchybrid/ClientList.h:156-157`
- **Reachability:** Internal — client tracking
- **Status:** STALE on 2026-03-30 after inspection: tracked entries are independent per-IP snapshots and do not own cross-linked clients, so destruction order is not semantically relevant here.

**Description:**
`CDeletedClientMap m_trackedClientsMap` stores pointers to `CDeletedClient` objects. During bulk cleanup in `RemoveAllTrackedClients()`, the order of deletion may matter if objects have cross-references.

**Fix:** Document cleanup ordering requirements.
