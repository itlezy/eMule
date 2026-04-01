# eMule P2P Application — Networking, Sockets, UPnP & Async Processing Analysis

**Date:** 2026-03-24
**Branch:** v0.72a-broadband-dev
**Scope:** Full networking stack — `srchybrid/` (TCP, UDP, encryption, UPnP, throttling, Kademlia)

---

## Table of Contents

- [Executive Summary](#executive-summary)
- [1. Socket Class Hierarchy](#1-socket-class-hierarchy)
- [2. Async I/O Model — WSAPoll Backend](#2-async-io-model--shared-wsapoll-backend)
- [3. Send Queue Architecture](#3-send-queue-architecture)
- [4. Packet Framing and Protocol Parsing](#4-packet-framing-and-protocol-parsing)
- [5. Upload Bandwidth Throttler](#5-upload-bandwidth-throttler)
- [6. UPnP NAT Traversal](#6-upnp-nat-traversal)
- [8. Encrypted Streams and Datagrams](#8-encrypted-streams-and-datagrams)
- [9. DNS Resolution](#9-dns-resolution)
- [10. Known Issues and Recommendations](#10-known-issues-and-recommendations)

---

## Executive Summary

The eMule networking stack is a mature **Windows-native asynchronous I/O architecture** centered on the current `WSAPoll` TCP backend, a dedicated upload bandwidth throttler thread, dual TCP/UDP encrypted transports, and two independent UPnP implementations. This report documents the major subsystems which remain in the current tree, identifies architectural limitations, and calls out specific issues.

---

## 1. Socket Class Hierarchy

### 1.1 Full Inheritance Chain

```
CAsyncSocketEx  (AsyncSocketEx.h — base, wraps the shared WSAPoll backend for TCP)
├── CEncryptedStreamSocket  (EncryptedStreamSocket.h — adds RC4 obfuscation)
│   ├── CEMSocket  (EMSocket.h — adds packet framing + dual send queues)
│   │   ├── CClientReqSocket  (ListenSocket.h — incoming peer TCP connections)
│   │   └── CServerSocket  (ServerSocket.h — outgoing ed2k server connections)
│   └── [implements ThrottledFileSocket interface]
└── CListenSocket  (ListenSocket.h — accepts new peer connections on TCP port)

CAsyncDatagramSocket  (AsyncDatagramSocket.h — wraps the shared WSAPoll backend for UDP)
├── CClientUDPSocket  (ClientUDPSocket.h — peer UDP + Kademlia, throttled)
│   └── [implements CEncryptedDatagramSocket + ThrottledControlSocket]
└── CUDPSocket  (UDPSocket.h — server UDP control/queries)
    └── [implements CEncryptedDatagramSocket + ThrottledControlSocket]
```

### 1.2 Legacy Socket Layering (Removed)

The historic `CAsyncSocketExLayer` and `CAsyncProxySocketLayer` chain has been removed. The current
tree keeps `CAsyncSocketEx` as the direct TCP backend under `CEncryptedStreamSocket` and `CEMSocket`
without middleware layers or proxy negotiation.

**Key files:**

| File | Role |
|------|------|
| `srchybrid/AsyncSocketEx.h/.cpp` | Base TCP socket; shared `WSAPoll` network thread |
| `srchybrid/AsyncDatagramSocket.h/.cpp` | Base UDP socket; shared `WSAPoll` backend with app-thread dispatch |
| `srchybrid/EncryptedStreamSocket.h/.cpp` | RC4 obfuscation layer for TCP |
| `srchybrid/EncryptedDatagramSocket.h/.cpp` | Stateless RC4 encryption for UDP |
| `srchybrid/EMSocket.h/.cpp` | Packet framing, dual send queues, rate control |
| `srchybrid/ListenSocket.h/.cpp` | Accept loop, half-open tracking, timeout logic |
| `srchybrid/ServerConnect.h/.cpp` | Server connection state machine |
| `srchybrid/ClientUDPSocket.h/.cpp` | Peer UDP socket |
| `srchybrid/UDPSocket.h/.cpp` | Server UDP socket |
| `srchybrid/DownloadQueue.h/.cpp` | Hostname source queue; worker-thread DNS + main-thread source insertion |
| `srchybrid/ThrottledSocket.h` | Abstract throttle interface |
| `srchybrid/UploadBandwidthThrottler.h/.cpp` | Dedicated throttler thread |

---

## 2. Async I/O Model — Shared `WSAPoll` Backend

### 2.1 Design Overview

The live tree uses a shared `WSAPoll` backend for both TCP and UDP transports. TCP readiness is
owned by a dedicated network thread under `CAsyncSocketEx`; UDP sockets use the same backend
through `CAsyncDatagramSocket` and marshal receive/send work back to the app thread.

Hostname resolution is no longer tied to WinSock window messages. Server UDP resolves hostnames on
its own resolver worker, and `CDownloadQueue` now resolves source hostnames on a dedicated worker
thread before draining completions during `CDownloadQueue::Process()`.

The remaining architectural limitation is not helper-window coupling anymore; it is that
`WSAPoll` is still an `O(n)` readiness scan and not the final IOCP-shaped design.

### 2.2 Current Dispatch Shape

The live branch no longer assigns sockets to helper-window message slots. `CAsyncSocketEx` keeps
socket interest state in the poll backend, and the dedicated network thread dispatches
`OnReceive` / `OnSend` / `OnConnect` / `OnClose` directly from `WSAPoll` readiness results.

UDP uses the same backend through `CAsyncDatagramSocket`, but marshals actual datagram processing
back to the app thread via `UM_WSAPOLL_UDP_SOCKET` so the existing protocol code can stay on its
current thread-affinity model.

### 2.3 Historical Context: Pre-WSAPoll Helper Window Model

Before the `WSAPoll` migration, `CAsyncSocketEx` used `WSAAsyncSelect` plus a hidden helper window
to translate socket events into `WM_SOCKETEX_NOTIFY + index` messages. That historical model is
kept here only to explain earlier audits and migration notes; it is not the current runtime path.

### 2.4 Current Socket Event Flow

The current TCP path dispatches readiness directly from `CAsyncSocketEx` into the encrypted-stream
and packet-framing layers. There is no longer a proxy or middleware interception stage in the live tree.

The only remaining networking message used in the live tree is the app-thread UDP dispatch post.
The historical `WM_SOCKETEX_*` and `WM_HOSTNAMERESOLVED` message routes have been removed from the
runtime path.

### 2.5 Architectural Limitations of the Current WSAPoll Bridge

- **Not IOCP:** `WSAPoll` is still an `O(n)` readiness scan and not the final Windows-native high-scale design.
- **Mixed thread-affinity above transport:** readiness is off the UI thread, but a large amount of protocol and scheduling logic still lives there.
- **Further hardening is operational, not architectural:** the remaining work is soak/stress validation rather than more transport migration.

---

## 3. TCP Packet Framing and Reassembly — `CEMSocket`

### 3.1 Wire Format

Every eMule TCP packet uses a 5-byte header:

```
Byte 0:    Protocol ID
             0xE3 = OP_EDONKEYPROT  (plain ED2K)
             0xC5 = OP_EMULEPROT    (eMule extended)
             0xD4 = OP_PACKEDPROT   (zlib-compressed payload)
Bytes 1–4: uint32 LE — payload length (EXCLUDING this 5-byte header)
Byte 5:    Opcode (first byte of payload)
Bytes 6…:  Remaining payload
```

The header struct in `srchybrid/Packets.h` is `#pragma pack(push, 1)` ensuring no padding regardless of alignment.

### 3.2 Reassembly State Machine (`EMSocket.cpp:316–398`)

Incoming data is appended to `GlobalReadBuffer` (2 MB static buffer). The reassembler loops until no complete packet can be extracted:

```cpp
static char GlobalReadBuffer[2000000];

while (rend >= rptr + PACKET_HEADER_SIZE || pendingPacket != NULL) {
    if (pendingPacket == NULL) {
        // validate protocol marker (lines 340–348)
        switch (header->eDonkeyID) {
        case OP_EDONKEYPROT: case OP_PACKEDPROT: case OP_EMULEPROT: break;
        default: OnError(ERR_WRONGHEADER); return;
        }
        // guard against > 2 MB (line 351)
        if (header->packetlength - 1 > sizeof GlobalReadBuffer) {
            OnError(ERR_TOOBIG); return;
        }
        pendingPacket = new Packet(rptr);
        rptr += PACKET_HEADER_SIZE;
    }
    uint32 toCopy = min(pendingPacket->size - pendingPacketSize, rend - rptr);
    memcpy(&pendingPacket->pBuffer[pendingPacketSize], rptr, toCopy);
    pendingPacketSize += toCopy;
    rptr += toCopy;
    if (pendingPacketSize == pendingPacket->size)
        PacketReceived(pendingPacket);  // complete — hand off to protocol handler
}
```

**Partial header handling:** Up to 4 bytes of an incomplete header are saved in `pendingHeader[PACKET_HEADER_SIZE]` and merged with the next `OnReceive` call.

### 3.3 Compressed Packets (`OP_PACKEDPROT`)

When protocol byte `0xD4` is detected, the payload is zlib-decompressed before `PacketReceived` is called. The decompressed size is stored in the header's `packetlength` field; a secondary field carries the compressed size. Decompression is transparent to all higher-level protocol handlers.

**Note:** The recent WIP commit `6c6fd3f` removes upload compression. Once that work is finalised, `OP_PACKEDPROT` on the receive path should be audited: the code may still need to accept compressed packets from older peers even if it no longer sends them.

### 3.4 Send Queue Architecture

`CEMSocket` maintains two separate send queues protected by a single `CCriticalSection sendLocker`:

```cpp
// EMSocket.h:132–134
CTypedPtrList<CPtrList, Packet*>          controlpacket_queue;  // high priority
CList<StandardPacketQueueEntry>           standardpacket_queue; // file data
CCriticalSection                          sendLocker;
```

Additionally, a `sendbuffer` / `sendblen` / `sent` triple tracks the packet currently being sent mid-stream.

**Send flow** (`EMSocket.cpp:537–696`):

1. Throttler thread calls `SendEM(maxBytes, minFragSize, controlOnly)`.
2. Control queue drained first; standard queue only if `controlOnly == false`.
3. `send()` called on the raw socket; if `WSAEWOULDBLOCK` is returned:
   - `m_bBusy = true`
   - `sendLocker.Unlock()` — **critical: lock released before return**
   - Returns to throttler; next `OnSend` callback will resume.
4. Sent byte count accumulated in `SocketSentBytes` return struct.

### 3.5 Download Rate Limiting

Per-socket download throttling is implemented directly in the `OnReceive` path (`EMSocket.cpp:273–292`):

```cpp
if (downloadLimitEnable && downloadLimit == 0) {
    pendingOnReceive = true;
    return;                    // block further reads until limit refreshed
}
if (downloadLimitEnable && readMax > downloadLimit)
    readMax = downloadLimit;
// … recv() … subtract from downloadLimit
```

The limit is refreshed by the parent calling `SetDownloadLimit(newBytes)`. This is a simple **credit-based** scheme: once credits are exhausted, `OnReceive` is suppressed until the next credit grant. This causes bursty behaviour at very low limits (< ~10 KB/s) because the granularity is one `OnReceive` event.

---

## 4. Encryption Transport — `CEncryptedStreamSocket`

### 4.1 Two Handshake Modes

#### Mode A — Client-to-Client (RC4 with MD5 key derivation)

```
A → B (plaintext header + RC4-encrypted body):
  <SemiRandomNotProtocolMarker[1]>  — distinguishes obfuscation from raw protocol
  <RandomKeyPart[4]>
  <MagicValue 0x22[4]>             — MAGICVALUE_REQUESTER = 34
  <EncryptionMethods[1]>
  <Preferred[1]>
  <RandomPadding[0–15]>

Key derivation (both sides):
  SendKey = MD5(PeerHash[16] || 0x22 || RandomKeyPart[4])
  RecvKey = MD5(PeerHash[16] || 0xCB || RandomKeyPart[4])
  First 1024 bytes of each RC4 keystream discarded
```

Overhead: ~33 bytes per connection setup.

#### Mode B — Client-to-Server (Diffie-Hellman, 768-bit prime)

```
Client → Server (cleartext):
  <SemiRandomNotProtocolMarker[1]>
  <G^A[96]>                        — 768-bit DH public key
  <RandomBytes[0–15]>

Server → Client:
  <G^B[96]>
  <MagicValue 0xCB[4]>             — MAGICVALUE_SERVER = 203
  <EncMethods[1]>
  <Preferred[1]>
  <Padding>

Shared secret S = G^(A*B) mod p  (768-bit, hardcoded prime at EncryptedStreamSocket.cpp:101–110)
Key derivation via MD5(S || magic)
```

Overhead: ~229 bytes.

### 4.2 Encryption State Machine

**`EStreamCryptState`** (11 states for client-client, 8 for server, `EncryptedStreamSocket.h:36–66`):

```
ECS_NONE → ECS_PENDING / ECS_PENDING_SERVER
  → ECS_NEGOTIATING (handshake bytes exchanged)
  → ECS_ENCRYPTING  (RC4 active, normal traffic flows)
```

State is advanced in `Receive()` (`EncryptedStreamSocket.cpp:229–328`). Once `ECS_ENCRYPTING`, every `recv()` call decrypts the buffer in-place with RC4 before returning bytes to `CEMSocket`.

### 4.3 Known Issues in Encryption Layer

**ASSERT(0) with "must be a bug" comments (`EncryptedStreamSocket.cpp`):**

Lines 150, 168, 181, 195, 300, 325, 432, 444, 471, 642, 655, 688, 700, 742 — 14 instances. In a Release build all ASSERTs compile out, meaning these code paths fall through silently. Several are annotated "this must be a bug" which implies the socket state becomes corrupted. These should be converted to `OnError(ERR_ENCRYPTION)` + disconnect.

**No forward secrecy:** Once the RC4 keys are established they never rotate. Recording a session and later compromising the client hash / DH exchange fully decrypts the session.

**No renegotiation path:** The state machine has no transition from `ECS_ENCRYPTING` back to a negotiation state. Cipher upgrade requires a new connection.

---

## 5. Proxy Support (Removed)

Proxy support and the associated socket-layer chain were removed during the `WSAPoll` TCP migration.
The current branch has no proxy settings, no proxy negotiation layer, and no proxy-specific transport path.

---

## 6. Upload Bandwidth Throttler

### 6.1 Architecture

`UploadBandwidthThrottler` is a dedicated `CWinThread` that runs its own tight loop, calling `Send()` on sockets rather than waiting for `OnSend` callbacks. This decouples upload bandwidth management from the main message pump.

```cpp
// UploadBandwidthThrottler.h:22–83
class UploadBandwidthThrottler : public CWinThread {
    std::list<ThrottledControlSocket*> m_ControlQueue_list;
    std::list<ThrottledControlSocket*> m_ControlQueueFirst_list;
    std::list<ThrottledControlSocket*> m_TempControlQueue_list;
    std::list<ThrottledControlSocket*> m_TempControlQueueFirst_list;
    CArray<ThrottledFileSocket*>        m_StandardOrder_list;   // upload slots
    CCriticalSection queueLocker;
    CCriticalSection tempQueueLocker;
    HANDLE m_eventSocketAvailable;     // wakeup signal when sockets added
    volatile bool m_bRun;
};
```

### 6.2 Throttler Main Loop (`UploadBandwidthThrottler.cpp:294–end`)

Each iteration:

1. **Determine allowed bytes:** Start from the active upload limit and keep the unlimited-path estimator as the upper bound when no finite limit is configured.
2. **Merge temp queues:** Swap `m_TempControl*` lists into active lists (under `tempQueueLocker`).
3. **Send control packets first:**
   - Drain `m_ControlQueueFirst_list` (sockets that already sent this cycle — re-queued for priority).
   - Then drain `m_ControlQueue_list`.
   - Each call: `socket->SendControlData(allowedBytes, minFragSize)`.
4. **Send file data:**
   - Iterate `m_StandardOrder_list` in order.
   - Each call: `socket->SendFileAndControlData(remainingBytes, minFragSize)`.
   - Stop when `remainingBytes == 0` or all sockets return busy (`WSAEWOULDBLOCK`).
5. **Auto-limit adjustment:** If no bandwidth cap is configured, guess an optimal limit based on the ratio of busy sockets.

### 6.3 Interface — `ThrottledSocket.h:12–31`

```cpp
struct SocketSentBytes { uint32 sentBytesControlPackets; uint32 sentBytesStandardPackets; };

class ThrottledControlSocket {
    virtual SocketSentBytes SendControlData(uint32 maxBytes, uint32 minFragment) = 0;
};

class ThrottledFileSocket : public ThrottledControlSocket {
    virtual SocketSentBytes SendFileAndControlData(uint32 maxBytes, uint32 minFragment) = 0;
    virtual bool HasQueues(bool onlyStandard = false) const = 0;
    virtual bool IsBusyExtensiveCheck() = 0;
    virtual bool IsBusyQuickCheck() const = 0;
};
```

`CEMSocket` implements both interfaces by delegating to `SendEM()`.

### 6.4 Locking Order

Three locks are involved; they must always be acquired in this order to avoid deadlock:

```
1. queueLocker      (throttler's slot list)
2. tempQueueLocker  (staging queues)
3. sendLocker       (per-socket send queue)
```

`sendLocker` is released **before** returning from any `SendEM()` call that hits `WSAEWOULDBLOCK` — this is the critical discipline that prevents deadlock between the throttler thread and the main thread.

### 6.5 Busy Detection

- **Quick:** `m_bBusy` flag — set when `send()` returns `WSAEWOULDBLOCK`, cleared on `OnSend`.
- **Extensive:** Checks queue depth, `sendbuffer != NULL`, fragment state, and time since last successful send. Called less frequently to avoid lock overhead.

### 6.6 Limitations

- **No token bucket:** The throttler uses a simple per-loop byte counter. This produces correct average rate but allows short-burst overruns between loop iterations. A sliding-window token bucket would produce smoother output.
- **Tight loop without sleep:** The throttler loop runs as fast as possible with no fixed tick rate. Under light load this wastes CPU. Under very high load it may starve the main thread. A `Sleep(1)` or `WaitForSingleObject(m_eventSocketAvailable, 1)` at the loop bottom helps on some hardware but is not consistently applied.
- **Single thread for all uploads:** All upload slots are serialised through one throttler thread. On multi-core machines this is a bottleneck once aggregate upload exceeds ~50–100 MB/s.

---

## 7. UPnP — Port Mapping Architecture

### 7.1 Design: Strategy Pattern with Fallback

UPnP is implemented using a **strategy pattern** with two concrete implementations and an automatic fallback mechanism.

```
CUPnPImplWrapper  (UPnPImplWrapper.h — selects and manages active impl)
├── m_liAvailable  (unused implementations)
├── m_liUsed       (tried implementations)
└── m_pActiveImpl  → one of:
    ├── CUPnPImplWinServ   (UPNP_IMPL_WINDOWSERVICE = 0)  — Windows UPnP Service / COM
    ├── CUPnPImplMiniLib   (UPNP_IMPL_MINIUPNPLIB = 1)   — miniupnpc library
    └── CUPnPImplNone      (UPNP_IMPL_NONE)               — dummy, no-op
```

**Base interface** (`UPnPImpl.h:51–56`):

```cpp
virtual void StartDiscovery(uint16 nTCPPort, uint16 nUDPPort) = 0;
virtual bool CheckAndRefresh() = 0;
virtual void StopAsyncFind() = 0;
virtual void DeletePorts() = 0;
virtual bool IsReady() = 0;
virtual int  GetImplementationID() = 0;
```

Two ports are mapped: **TCP peer port** and **UDP peer port**.

### 7.2 Implementation Selection (`UPnPImplWrapper.cpp:30–68`)

On startup, the wrapper:

1. Creates both implementations (unless disabled in preferences).
2. Checks `thePrefs.GetLastWorkingUPnPImpl()` — if the last working implementation is in the available list, promotes it to active.
3. Falls back to the list head if no preference match.
4. `SwitchImplementation()` is called on failure to try the next available impl.

```cpp
CUPnPImplWrapper::CUPnPImplWrapper() {
    if (!thePrefs.IsWinServUPnPImplDisabled())
        m_liAvailable.AddTail(new CUPnPImplWinServ());
    if (!thePrefs.IsMinilibUPnPImplDisabled())
        m_liAvailable.AddTail(new CUPnPImplMiniLib());
    if (m_liAvailable.IsEmpty())
        m_liAvailable.AddTail(new CUPnPImplNone());
    Init();
}
```

### 7.3 Windows UPnP Service Implementation — `CUPnPImplWinServ`

**File:** `srchybrid/UPnPImplWinServ.h/.cpp`
**Origin:** Derived/adapted from the Shareaza project (copyright notice retained in header)

Uses the Windows `upnp.h` COM API (`IUPnPDeviceFinder`, `IUPnPDevice`, `IUPnPService`).

**Discovery flow:**

1. `ProcessAsyncFind(L"urn:schemas-upnp-org:device:InternetGatewayDevice:1")` — async search via `IUPnPDeviceFinder::CreateInstanceAsync`.
2. `CDeviceFinderCallback::DeviceAdded()` fires for each discovered device.
3. `GetDeviceServices()` enumerates WANIPConnection and WANPPPConnection services.
4. `StartPortMapping()` → `MapPort()` → `InvokeAction()` — calls `AddPortMapping` SOAP action.
5. Result message posted to application window (`SendResultMessage()`).

**ADSL detection** (`m_bADSL`, `m_ADSLFailed`): The implementation separately tracks whether the gateway is ADSL-type and whether port mapping failed for it. If ADSL mapping fails, it retries with `m_bDisableWANPPPSetup = true` (tries WANIPConnection instead).

**`CheckAndRefresh()` returns false** — acknowledged in a comment:

> *"No Support for Refreshing in this (fallback) implementation yet — in many cases where it would be needed (router reset etc) the windows side of the implementation tends to get bugged until reboot anyway."*

**Timeout handling** (`IsAsyncFindRunning()`, line 100):

```cpp
if (::GetTickCount() >= m_tLastEvent + SEC2MS(10)) {
    m_pDeviceFinder->CancelAsyncFind(m_nAsyncFindHandle);
    m_bAsyncFindRunning = false;
}
// + message pump drain while waiting:
while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
    ::TranslateMessage(&msg); ::DispatchMessage(&msg);
}
```

**Issue:** Calling `PeekMessage` / `DispatchMessage` inside `IsAsyncFindRunning()` introduces a **re-entrant message pump**. If any of those dispatched messages trigger further UPnP code this can cause re-entrancy bugs. This pattern is a known hazard in Windows COM callback code.

**`std::unary_function` usage** (`UPnPImplWinServ.h:116`):

```cpp
struct FindDevice : private std::unary_function<DevicePointer, bool>
```

`std::unary_function` was deprecated in C++11 and removed in C++17. This will fail to compile with `/std:c++17` or later.

### 7.4 miniupnpc Implementation — `CUPnPImplMiniLib`

**File:** `srchybrid/UPnPImplMiniLib.h/.cpp`
**Library:** `miniupnpc\include\miniupnpc.h` + `upnpcommands.h`

Uses the miniupnpc C library, which does its own SSDP discovery and direct UPnP/SOAP HTTP calls — no Windows UPnP service dependency.

**Discovery thread** (`CStartDiscoveryThread : public CWinThread`):

Discovery runs in a separate `CWinThread`, protecting the main thread from blocking SSDP/HTTP calls. The thread sets `m_bAbortDiscovery = true` as its stop flag.

**Abort / cleanup** (`UPnPImplMiniLib.cpp:68–87`):

```cpp
void CUPnPImplMiniLib::StopAsyncFind() {
    if (m_hThreadHandle != NULL) {
        m_bAbortDiscovery = true;
        CSingleLock lockTest(&m_mutBusy);
        if (!lockTest.Lock(SEC2MS(7))) {
            DebugLogError(_T("Waiting for UPnP StartDiscoveryThread to quit failed, trying to terminate..."));
            if (m_hThreadHandle != NULL)
                DebugLogError(::TerminateThread(m_hThreadHandle, 0) ? _T("...OK") : _T("...Failed"));
        }
    }
}
```

**Issue:** `TerminateThread()` is used as a last resort if the thread doesn't exit within 7 seconds. `TerminateThread` does not unwind the stack, does not release locks, and does not close file handles. It is documented by Microsoft as a dangerous API. The comment in the code acknowledges this: *"there isn't a good solution here… terminating is quite bad too."* If the miniupnpc library holds internal state (e.g. open sockets or malloc'd memory) at termination time, the process heap can be left in an inconsistent state.

**`CheckAndRefresh()` is fully implemented** in `CUPnPImplMiniLib` — it re-runs port mapping to refresh leases. This makes it the preferred implementation for long-running sessions where router reboots are possible.

**Port deletion** (`DeletePort()`, `UPnPImplMiniLib.cpp:99–110`):

```cpp
void CUPnPImplMiniLib::DeletePort(uint16 port, LPCTSTR prot) {
    char achPort[8];
    sprintf(achPort, "%hu", port);   // safe: %hu with uint16 always fits in 8 bytes
    UPNP_DeletePortMapping(m_pURLs->controlURL, …, achPort, CStringA(prot), NULL);
}
```

The `sprintf` here is safe (port number is at most 5 digits), but would benefit from `sprintf_s` for consistency with the rest of the codebase.

### 7.5 `TRISTATE` Port Forwarding State

```cpp
enum TRISTATE { TRIS_FALSE, TRIS_UNKNOWN, TRIS_TRUE };
volatile TRISTATE m_bUPnPPortsForwarded;
```

`TRIS_UNKNOWN` is the initial state and the state after a failed/pending discovery. The application uses this to show "unknown" in the status bar rather than a definitive success/failure indication.

### 7.6 UPnP Issues Summary

| Issue | Severity | File |
|-------|----------|------|
| Re-entrant message pump in `IsAsyncFindRunning()` | Medium | `UPnPImplWinServ.h:100–109` |
| `std::unary_function` deprecated/removed in C++17 | High (compile break) | `UPnPImplWinServ.h:116` |
| `TerminateThread()` on stuck miniupnpc thread | Medium | `UPnPImplMiniLib.cpp:79` |
| `WinServ` impl does not support `CheckAndRefresh()` | Low | `UPnPImplWinServ.h:86` |
| `sprintf` instead of `sprintf_s` in `DeletePort()` | Low | `UPnPImplMiniLib.cpp:103` |

---

## 8. UDP Sockets

### 8.1 Peer UDP — `CClientUDPSocket`

**File:** `srchybrid/ClientUDPSocket.h:38`

```cpp
class CClientUDPSocket : public CAsyncDatagramSocket,
                          public CEncryptedDatagramSocket,
                          public ThrottledControlSocket
{
    CTypedPtrList<CPtrList, UDPPack*> controlpacket_queue;
    CCriticalSection sendLocker;
    uint16 m_port;
    bool m_bWouldBlock;
};
```

UDP packet struct (`ClientUDPSocket.h:23–36`):

```cpp
struct UDPPack {
    Packet  *packet;
    uint32   dwIP;
    uint16   nPort;
    DWORD    dwTime;          // enqueue time — aged packets dropped
    bool     bEncrypt;
    bool     bKad;            // route to Kademlia listener
    uint32   nReceiverVerifyKey;
    uchar    pachTargetClientHashORKadID[16];
};
```

Packets are removed from the queue if `dwTime` is too old — prevents stale UDP packets from being sent after a timeout.

### 8.2 Server UDP — `CUDPSocket`

**File:** `srchybrid/UDPSocket.h:51`

Similar to `CClientUDPSocket` but sends `SServerUDPPacket` structs to ed2k servers. The live tree
no longer uses `CUDPSocketWnd`; unresolved server hostnames are resolved by an owned worker and the
socket stays on the shared `WSAPoll` backend.

### 8.3 UDP Encryption — `CEncryptedDatagramSocket`

**File:** `srchybrid/EncryptedDatagramSocket.h:19–32`

Unlike TCP, UDP has **no handshake**. Keys are pre-derived from known material:

**Client-to-client:**

```cpp
static int DecryptReceivedClient(BYTE *in, int len, BYTE **out,
    uint32 ip, uint32 *recvKey, uint32 *sendKey);
static uint32 EncryptSendClient(uchar *buf, uint32 len,
    const uchar *clientHash, bool isKad, uint32 recvKey, uint32 sendKey);
```

Keys derived from the target's client hash — same MD5-based derivation as TCP obfuscation, applied statically.

**Client-to-server:**

```cpp
static int DecryptReceivedServer(BYTE *in, int len, BYTE **out,
    uint32 baseKey, const SOCKADDR_IN &dbgIP);
static uint32 EncryptSendServer(uchar *buf, uint32 len, uint32 baseKey);
```

`baseKey` is derived from the server's public key material.

**No replay protection noted.** UDP packets carry no sequence number or nonce in the obfuscation layer. A captured and replayed UDP packet will decrypt and be processed normally if it arrives within the server's processing window.

### 8.4 Kademlia UDP

Kademlia traffic flows through `CClientUDPSocket` (marked `bKad = true`). The Kademlia listener (`kademlia/net/KademliaUDPListener`) processes DHT operations (bootstrap, ping, search, store). Kademlia UDP has no explicit bandwidth throttling — it is treated as control traffic and bypasses the standard upload throttler's file-data slot mechanism.

---

## 9. Server Connection State Machine — `CServerConnect`

### 9.1 Connection States (`ServerConnect.h:19–29`)

```cpp
CS_NOTCONNECTED         =  0
CS_CONNECTING           =  1   // TCP SYN sent
CS_CONNECTED            =  2   // logged in, fully operational
CS_WAITFORLOGIN         =  3   // connected, awaiting OP_LOGINREPLY
CS_WAITFORPROXYLISTENING=  4   // SOCKS BIND awaiting accept
CS_SERVERFULL           = -1   // server rejected: capacity
CS_ERROR                = -2   // protocol-level error
CS_SERVERDEAD           = -3   // timeout / connection reset
CS_DISCONNECTED         = -4   // intentional disconnect
CS_FATALERROR           = -5   // unrecoverable
```

### 9.2 Lifecycle (`ServerConnect.cpp:115–211`)

```
ConnectToServer(CServer*, multiconnect)
  → Create CServerSocket
  → socket->Create(0, SOCK_STREAM, FD_DEFAULT)
  → socket->ConnectTo(server, bNoCrypt)   — async
  → OnConnect() fires
  → Send OP_LOGINREQUEST  [clienthash, port, tags: name/version/flags]
  → Await OP_LOGINREPLY
  → State = CS_CONNECTED
```

### 9.3 Retry Logic

A `SetTimer` callback fires every `CS_RETRYCONNECTTIME = 30 seconds`. Up to 2 simultaneous connection attempts are allowed (obfuscated port + plain port). If the obfuscated attempt fails, the plain port is tried automatically. `StopConnectionTry()` cleanly destroys all pending attempt sockets.

---

## 10. Connection Acceptance — `CListenSocket`

### 10.1 Half-Open Connection Tracking

`CListenSocket` tracks the number of half-open (TCP SYN received, not yet fully established) connections:

```cpp
// ListenSocket.h
int m_nHalfOpen;    // count of SS_Half sockets
int m_nComp;        // count of SS_Complete sockets
```

This is important on Windows XP SP2+ which throttles half-open connections system-wide. The code uses this count to delay accepting new connections when the half-open count is high.

### 10.2 Timeout Logic (`ListenSocket.cpp:120–154`)

```cpp
bool CClientReqSocket::CheckTimeOut() {
    if (m_nOnConnect == SS_Half) {
        // 4× normal timeout for half-open — accommodates Win XP SP2 connection limits
        if (curTick < timeout_timer + GetTimeOut() * 4)
            return false;
        Disconnect("SS_Half timeout");
        return true;
    }
    // SS_Complete: timeout depends on activity (downloading, chatting, Kad buddy)
}
```

The 4× multiplier for `SS_Half` exists specifically to accommodate Windows XP SP2's connection throttling. This is a form of legacy OS compatibility code (see also `REPOFUNC.md` Section 6).

### 10.3 Safe Deferred Deletion (`ListenSocket.cpp:194–207`)

```cpp
void CClientReqSocket::Safe_Delete() {
    AsyncSelect(FD_CLOSE);     // suppress further event notifications
    Shutdown(SD_BOTH);         // TCP FIN
    deltimer = GetTickCount();
    deletethis = true;         // deferred: actual delete after 10 seconds
}
```

The 10-second deferral allows any in-flight packets to be drained before the socket object is freed. The main window timer sweeps `deletethis` sockets periodically.

---

## 11. Thread Model and Synchronization

### 11.1 Threads Involved in Networking

| Thread | Role | Socket access |
|--------|------|--------------|
| **Main UI thread** | Message pump; all socket callbacks (`OnReceive`, `OnConnect`, `OnClose`) | Direct — owns sockets |
| **Upload throttler** | Calls `SendEM()` on upload slots | Via `sendLocker` per socket |
| **UPnP MiniLib discovery** | SSDP + SOAP HTTP calls | None (own network I/O via miniupnpc) |
| **AICH sync thread** | File hash verification | No socket I/O |
| **Kademlia worker** | DHT routing decisions | Posts to main thread for actual sends |

### 11.2 Critical Section Discipline

All lock acquisitions in networking code follow this strict order:

```
1. UploadBandwidthThrottler::queueLocker       (throttler slot list)
2. UploadBandwidthThrottler::tempQueueLocker   (staging queues)
3. CEMSocket::sendLocker                       (per-socket send buffer)
```

Violating this order (e.g. acquiring `sendLocker` then trying to acquire `queueLocker`) would deadlock with the throttler thread.

### 11.3 WSAEWOULDBLOCK and Lock Release

The critical discipline in `SendEM()` (`EMSocket.cpp:630–640`):

```cpp
if (result == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK) {
    m_bBusy = true;
    sendLocker.Unlock();    // MUST unlock before returning to throttler
    return ret;
}
```

If `sendLocker` were held when returning `WSAEWOULDBLOCK` to the throttler, and the throttler then tried to acquire `queueLocker` while the main thread was trying to add to the queue (acquiring `queueLocker` then `sendLocker`), deadlock would occur. The explicit unlock before return is the correct and necessary pattern here.

### 11.4 Main Thread Stall Risk

Because all socket callbacks run on the main UI thread's message pump, any slow handler stalls every socket in the application. Handlers that do file I/O, decompress large packets, or acquire locks that might be held by the throttler thread are potential stall points. The architecture has no mechanism to shed work to worker threads within a single packet callback.

---

## 12. Network Error Handling

### 12.1 Error Constants (`EMSocket.h` / `EncryptedStreamSocket.h`)

```cpp
ERR_WRONGHEADER         = 0x01  // unknown protocol byte
ERR_TOOBIG              = 0x02  // packet > 2 MB
ERR_ENCRYPTION          = 0x03  // handshake failed
ERR_ENCRYPTION_NOTALLOWED = 0x04 // unencrypted connection when encryption required
```

### 12.2 Disconnect on Error

`OnError(code)` → `CClientReqSocket::Disconnect(reason)` → `Safe_Delete()` (deferred 10s). All error paths should reach `OnError`; the 14 `ASSERT(0)` calls in `EncryptedStreamSocket.cpp` that currently skip `OnError` in Release builds are a gap (see Section 4.3).

### 12.3 Protocol Header Validation

Every packet's protocol byte is validated before the packet object is allocated (`EMSocket.cpp:340–348`). Unknown protocol bytes call `OnError(ERR_WRONGHEADER)` and disconnect immediately. This is correct and prevents allocation of arbitrarily large `Packet` objects from malformed input.

---

## 13. Performance Characteristics

| Metric | Value | Notes |
|--------|-------|-------|
| Max sockets per thread | 47,869 | `AsyncSocketEx.h:82–86` |
| Socket lookup time | O(1) | Direct array index by message ID |
| Packet reassembly buffer | 2 MB static | `GlobalReadBuffer` in `EMSocket.cpp` |
| TCP encryption handshake overhead | ~33 bytes (c2c) / ~229 bytes (c2s) | One-time per connection |
| TCP encryption CPU cost | RC4 — ~300–500 MB/s on modern CPU | Negligible for P2P workloads |
| UDP encryption overhead | Per-packet RC4, no handshake | Stateless, keyed from client hash |
| UPnP discovery time | 1–10 seconds | Async; both impls have timeouts |
| Half-open timeout | 4× `GetTimeOut()` (~6 min) | Accommodates Win XP SP2 |
| Deferred socket delete delay | 10 seconds | Safe_Delete() deferral |

---

## 14. Issues and Recommendations

### Priority 1 — Should Fix

**14.1 Convert `ASSERT(0)` + "must be a bug" to real error paths in `EncryptedStreamSocket.cpp`**

Lines: 150, 168, 181, 195, 300, 325, 432, 444, 471, 642, 655, 688, 700, 742.
In Release builds these compile out. The socket continues in an undefined state. Each should call `OnError(ERR_ENCRYPTION)` and disconnect.

**14.2 Fix `std::unary_function` in `UPnPImplWinServ.h:116`**

`std::unary_function` was deprecated in C++11 and **removed in C++17**. Replace with either a lambda or a plain functor struct:

```cpp
// Before:
struct FindDevice : private std::unary_function<DevicePointer, bool> { … };
// After (C++11+):
struct FindDevice {
    using argument_type = DevicePointer;
    using result_type = bool;
    …
};
// Or just use auto with a lambda at the call site.
```

**14.3 Address re-entrant message pump in `CUPnPImplWinServ::IsAsyncFindRunning()`**

The `PeekMessage` / `DispatchMessage` loop inside `IsAsyncFindRunning()` at line 104–108 can cause re-entrant calls into UPnP code or other message handlers while the function is in use. Move discovery to a dedicated thread (as `CUPnPImplMiniLib` already does) or process only a specific message range rather than all messages.

**14.4 UDP — No Replay Protection**

UDP packets carry no sequence number or per-packet nonce in the obfuscation layer. An attacker can replay captured UDP packets. For Kademlia this could be used to inject stale routing information. Add a timestamp or nonce field to the UDP obfuscation header and reject packets outside a ±30 second window.

### Priority 2 — Should Address

**14.5 `TerminateThread()` in `UPnPImplMiniLib::StopAsyncFind()`**

The 7-second timeout followed by `TerminateThread()` is dangerous. Consider: (a) increasing the timeout, (b) setting `m_bAbortDiscovery` earlier so miniupnpc has more time to notice, (c) structuring the thread's inner loop to check the abort flag more frequently between SSDP/HTTP calls.

**14.6 UDP not proxied — real IP leakage**

Document this limitation clearly in UI preferences. If a user configures a SOCKS5 proxy expecting full anonymity, they will have their real IP exposed on UDP (Kademlia + peer UDP). Either implement SOCKS5 UDP associate or display a warning when proxy is enabled.

**14.7 Download rate-limiting granularity**

The credit-based `downloadLimit` scheme produces bursty downloads at low limits. Replace with a token-bucket refill on a 100ms timer for smoother behaviour.

**14.8 Throttler loop CPU usage**

The upload throttler runs a tight loop with no yield when no sockets are active. Ensure `WaitForSingleObject(m_eventSocketAvailable, timeout)` is used to sleep when the queue is empty, and that the event is signalled whenever sockets are added to the queue.

### Priority 3 — Modernisation

**14.9 IOCP migration**

The current `WSAPoll` bridge is functional and already moves live socket ownership off the UI
thread. The remaining long-term scalability step is `WSAIoctl` + completion ports (IOCP), which
would replace the poll loop rather than the removed helper-window model.

**14.10 Separate socket I/O from UI thread**

Live socket readiness no longer runs on the main thread. The remaining UI-thread bottleneck is the
higher-level protocol and scheduling work that still executes there after transport dispatch.

**14.11 Replace `sprintf` with `sprintf_s` in `UPnPImplMiniLib.cpp:103`**

Minor consistency fix — the rest of the codebase uses `_s` variants.

---

## 15. Component Reference Table

| Component | Class | Files | Technology |
|-----------|-------|-------|------------|
| Base async socket | `CAsyncSocketEx` | `AsyncSocketEx.h/.cpp` | `WSAPoll` network thread |
| TCP obfuscation | `CEncryptedStreamSocket` | `EncryptedStreamSocket.h/.cpp` | RC4 + MD5 / DH-768 handshake |
| P2P TCP | `CEMSocket` | `EMSocket.h/.cpp` | Packet framing, dual queues |
| Peer TCP | `CClientReqSocket` | `ListenSocket.h/.cpp` | Timeout management, deferred delete |
| Server TCP | `CServerSocket` | `ServerSocket.h/.cpp` | Login protocol |
| Server state machine | `CServerConnect` | `ServerConnect.h/.cpp` | 9-state machine, retry timer |
| Accept loop | `CListenSocket` | `ListenSocket.h/.cpp` | Half-open tracking |
| Peer UDP | `CClientUDPSocket` | `ClientUDPSocket.h/.cpp` | Encrypted datagrams, aged queue |
| Server UDP | `CUDPSocket` | `UDPSocket.h/.cpp` | Encrypted datagrams |
| UDP obfuscation | `CEncryptedDatagramSocket` | `EncryptedDatagramSocket.h/.cpp` | Stateless RC4, key from hash |
| Throttle interface | `ThrottledSocket` | `ThrottledSocket.h` | Abstract send interface |
| Upload throttler | `UploadBandwidthThrottler` | `UploadBandwidthThrottler.h/.cpp` | Dedicated thread, 3-tier queues |
| UPnP wrapper | `CUPnPImplWrapper` | `UPnPImplWrapper.h/.cpp` | Strategy + fallback |
| UPnP (Windows) | `CUPnPImplWinServ` | `UPnPImplWinServ.h/.cpp` | COM / `IUPnPDeviceFinder` |
| UPnP (miniupnpc) | `CUPnPImplMiniLib` | `UPnPImplMiniLib.h/.cpp` | miniupnpc library, worker thread |
| UPnP (none) | `CUPnPImplNone` | `UPnPImpl.h` | No-op dummy |
| Kademlia UDP | `CKademliaUDPListener` | `kademlia/net/` | DHT overlay on peer UDP port |

---

*End of report.*

---

## Future: IPv6, uTP, and NAT Traversal (from eMuleAI analysis)

> **Note:** None of this is implemented in eMulebb yet; IPv6/uTP are future work.
> Kad Buddy NAT relay already exists in stock eMule; the items below are extensions.

### FEAT_025: CAddress IPv6 Abstraction Layer

*Origin: eMuleAI, extended from NeoMule*

Full dual-stack IPv4+IPv6 address abstraction. Key design points:

- **CUInt128 Kademlia interop** — seamless conversion between Kad 128-bit node IDs and IPv6 addresses
- **Byte-order control** — explicit host/network byte order throughout the API
- **`IsMappedIPv4()`** — detects IPv4-mapped IPv6 addresses (::ffff:0:0/96)
- **`IsPublicIP()`** — unified public-address check for both address families
- **`GetAF()`** — returns `AF_INET` or `AF_INET6` for socket creation

### FEAT_026: uTP Transport

*Origin: eMuleAI, based on libutp*

Micro Transport Protocol (uTP) as an alternative to raw UDP for congestion-controlled transfers:

- **Multi-context architecture** with `CCriticalSection` thread safety per uTP context
- **IPv6 dual-stack** — single socket handles both address families
- **Expected peer tracking** — associates incoming uTP connections with known peer records
- **Per-peer dynamic MTU** via Windows IP Helper API (see FEAT_028)

### FEAT_027: NAT Traversal — eServer Buddy Relay

*Origin: eMuleAI only (two LowID clients on the same ed2k server)*

Extension of the existing Kad Buddy relay to work via ed2k servers:

- **`EServerRelayRequest` struct** — encapsulates relay request state
- **Slot management** — configurable 3-100 buddy slots per relay
- **Magic proof** — cryptographic proof to prevent relay abuse
- **Buddy pull** — passive relay establishment initiated by the buddy side

### FEAT_028: Per-Peer Dynamic MTU

*Origin: eMuleAI*

Automatic MTU discovery per peer connection:

- **IPv4 default:** 1402 bytes
- **IPv6 default:** 1232 bytes (RFC 2460 minimum path MTU)
- **Discovery method:** `GetBestInterfaceEx()` + `GetIpInterfaceEntry()` via Windows IP Helper API
- Feeds into uTP (FEAT_026) for optimal packet sizing
