# eMule Threading Model — Analysis & Improvement Paths

**Platform: Windows 10+ x64 ONLY**
**Build: MSVC v143 (VS 2022), MFC static**
**Codebase: srchybrid/**

---

## Executive Summary

eMule uses a hybrid threading model: a handful of background worker threads for file I/O, combined
with a **single-threaded event loop on the UI thread** that drives all network I/O, all protocol
processing, Kademlia, download/upload scheduling, and everything else. The socket layer
(`CAsyncSocketEx`) is built on `WSAAsyncSelect`, which converts network events into Windows
messages dispatched to a message-only "helper window" — also on the UI thread.

The result is that the UI thread is the network thread, the protocol thread, the scheduler, and
the rendering thread simultaneously. This is architecturally sound for a 2002-era Win32 app, but
produces serious bottlenecks at modern connection counts, blocks the UI whenever any one operation
stalls, and makes the code hard to reason about under load.

---

## 1. Thread Inventory

### 1.1 Worker Thread Classes (CWinThread subclasses)

| Class | File | Priority | Purpose | Communication |
|-------|------|----------|---------|---------------|
| `CAddFileThread` | SharedFileList.h:145 | BELOW_NORMAL | Hash file (BLAKE2B+AICH), add to shared list | PostMessage TM_FINISHEDHASHING, TM_HASHFAILED, TM_FILEOPPROGRESS, TM_IMPORTPART |
| `CPartFileWriteThread` | PartFileWriteThread.h:35 | BELOW_NORMAL | Async overlapped file writes via IOCP | `volatile` flags + CEvent m_eventThreadEnded |
| `CUploadDiskIOThread` | UploadDiskIOThread.h:36 | NORMAL | Async overlapped file reads via IOCP | `volatile` flags + CEvent m_eventThreadEnded |
| `UploadBandwidthThrottler` | UploadBandwidthThrottler.h:22 | NORMAL | Upload slot bandwidth pacing | CCriticalSection queues + CEvent signals |
| `CAICHSyncThread` | AICHSyncThread.h:23 | IDLE | Read/write KNOWN2.MET hash file | CMutex m_mutKnown2File |
| `CFrameGrabThread` | FrameGrabThread.h:30 | BELOW_NORMAL | Decode video frames for preview | PostMessage TM_FRAMEGRABFINISHED |
| `CGDIThread` | GDIThread.h:9 | NORMAL | GDI off-screen rendering | CEvent m_hEventKill/m_hEventDead + CCriticalSection |
| `CPreviewThread` | Preview.h:24 | BELOW_NORMAL | Launch external preview process | Fires and forgets |
| `CGetMediaInfoThread` | FileInfoDialog.cpp:232 | LOWEST | Read media file metadata | PostMessage UM_MEDIA_INFO_RESULT |
| `CNotifierMailThread` | SendMail.cpp:223 | LOWEST | Send SMTP email notifications | CCriticalSection sm_critSect |
| `CLoadDataThread` | Indexed.h:64 | BELOW_NORMAL | Load Kademlia index data from disk | CMutex m_mutSync + volatile flags |
| `CStartDiscoveryThread` | UPnPImplMiniLib.h:36 | NORMAL | miniupnpc UPnP device discovery | CMutex m_mutBusy |

Additionally:
- `WebSocket.cpp:472` — raw `CreateThread` for WebSocket accept loop
- `WebSocket.cpp:583` — raw `CreateThread` for TLS/socket session handling

### 1.2 Everything Else: The UI Thread

**All** of the following runs on the UI thread, driven by a 100ms `SetTimer` and `WSAAsyncSelect`
message dispatch:

- TCP socket accept, receive, send (`ListenSocket`, `CEMSocket`, `CClientReqSocket`)
- UDP packet receive and dispatch (`CClientUDPSocket`)
- All eMule protocol parsing and processing
- Kademlia routing, search, bootstrap, firewall check (`CKademlia::Process`)
- Download queue scheduling (`CDownloadQueue::Process`)
- Upload queue scheduling (`CUploadQueue::Process`)
- Server connection management (`CServerConnect`)
- Known file list, client list, credit processing
- DNS resolution via `WSAAsyncGetHostByName` → WM_HOSTNAMERESOLVED

---

## 2. The Central Scheduler

### 2.1 UploadQueue Timer (the heartbeat)

```cpp
// UploadQueue.cpp:119
::SetTimer(NULL, 0, SEC2MS(1)/10, UploadTimer);  // 100ms
```

`UploadQueue::UploadTimer` is a system timer callback (`TIMERPROC`) that fires every 100 ms on
the UI thread. It is the closest thing eMule has to a game-loop tick. The full call tree per tick:

```
UploadTimer (100ms)
├─ uploadqueue->Process()             — upload slot management, client cycling
├─ downloadqueue->Process()           — chunk requests, source finding, timeout checks
│
├─ [every 1s — i1sec >= 10]
│   ├─ clientcredits->Process()
│   ├─ serverlist->Process()
│   ├─ knownfiles->Process()
│   ├─ friendlist->Process()
│   ├─ clientlist->Process()          — purges dead clients, processes queued packets
│   ├─ sharedfiles->Process()
│   ├─ Kademlia::CKademlia::Process() — full Kad routing tick
│   │   ├─ RecheckFirewalled()
│   │   ├─ RefreshUPnP()
│   │   ├─ Self-lookup / FindBuddy()
│   │   ├─ SearchManager::JumpStart()
│   │   ├─ RoutingZone::OnBigTimer()  (hourly)
│   │   └─ RoutingZone::OnSmallTimer() (minutely)
│   ├─ serverconnect->TryAnotherConnectionRequest()
│   ├─ listensocket->UpdateConnectionsStatus()
│   └─ serverconnect->CheckForTimeout()
│
├─ [every 2s]
│   ├─ UpdateConnectionStats()
│   └─ taskbar progress update
│
└─ [every stats interval]
    └─ statisticswnd->ShowStatistics()
```

Every one of these runs synchronously on the UI thread, one after another, before the message
pump can process any socket events.

### 2.2 Socket Events Interleaved with Timer Ticks

Between timer ticks the message pump dispatches `WM_SOCKETEX_NOTIFY+N` messages from the
`CAsyncSocketExHelperWindow`. Each message triggers one call into a socket's `OnReceive`,
`OnSend`, `OnAccept`, or `OnClose` handler — again on the UI thread.

A single `CEMSocket::OnReceive` call reads up to **2 MB** from the socket into a static global
buffer:

```cpp
// EMSocket.cpp:258
static char GlobalReadBuffer[2000000];
```

All protocol parsing, packet queuing, and state transitions happen inline in that call before
returning to the message pump. Under heavy load (hundreds of connections) this means the message
pump is essentially always occupied with socket handlers.

---

## 3. The AsyncSocketEx Helper Window in Detail

`CAsyncSocketEx` uses **`WSAAsyncSelect`** — the oldest Windows async socket API, dating to
Winsock 1.1. It works by posting a window message for each socket event to a registered `HWND`.

```
WSAAsyncSelect(socket, hHelperWnd, WM_SOCKETEX_NOTIFY + nSocketIndex, FD_READ|FD_WRITE|...)
                                  ↓
Windows kernel posts message to helper window's queue
                                  ↓
Message pump dispatches to CAsyncSocketExHelperWindow::WindowProc
                                  ↓
Calls OnReceive() / OnSend() / OnAccept() / OnClose() on the socket object
                                  ↓
All protocol work happens here, on the UI thread
```

Each thread that creates a `CAsyncSocketEx` gets its own helper window via thread-local storage:

```cpp
// AsyncSocketEx.h:279
static THREADLOCAL t_AsyncSocketExThreadData *thread_local_data;
```

The socket-to-message-ID mapping uses a flat array:

```cpp
// AsyncSocketEx.h:85-86
#define WM_SOCKETEX_NOTIFY   (WM_USER + 0x101 + 3)  // 0x0504
#define MAX_SOCKETS          (0xBFFF - WM_SOCKETEX_NOTIFY + 1)  // max ~47 869 sockets
```

### 3.1 Why This Is the Core Problem

`WSAAsyncSelect` forces all socket notifications through the message pump of the thread that
registered the socket. In eMule that thread is the UI thread. There is no way to split socket
processing across threads without abandoning `WSAAsyncSelect` entirely, because the `HWND`
used to register the socket is bound to the thread that created it.

Every millisecond the UI is rendering a list control, updating a progress bar, or running through
the 1-second Process tick, no socket event can be processed. Every millisecond a socket's
`OnReceive` is parsing packets, the UI cannot respond to user input.

---

## 4. Synchronization Primitives in Use

### 4.1 CCriticalSection (MFC wrapper over CRITICAL_SECTION)

| Variable | File | Protects |
|----------|------|---------|
| `sendLocker` | EMSocket.h:134 | Control/standard TCP send queues |
| `sendLocker` | ClientUDPSocket.h:63 | UDP packet send queue |
| `sendLocker` | UDPSocket.h:80 | Raw UDP send queue |
| `m_csGDILock` | GDIThread.h:61 | GDI off-screen drawing |
| `m_lockWriteList` | PartFileWriteThread.h:43 | File write queue |
| `m_lockFlushList` | PartFileWriteThread.h:51 | File flush queue |
| `sm_critSect` | SendMail.cpp:238 | SMTP send serialization |
| `m_mutWriteList` | SharedFileList.h:102 | Shared file map writes |
| `queueLocker` | UploadBandwidthThrottler.h:69 | Upload socket list |
| `tempQueueLocker` | UploadBandwidthThrottler.h:70 | Temp socket queue |
| `pcsUploadListRead` | UploadDiskIOThread.cpp:95 | Upload read list |
| `m_csBlockListsLock` | UploadQueue.h:36 | Block lists |
| `m_csUploadListMainThrdWriteOtherThrdsRead` | UploadQueue.h:140 | Main writes, others read |

### 4.2 CMutex (kernel-mode named mutex)

| Variable | File | Protects |
|----------|------|---------|
| `hashing_mut` | Emule.h:119 | File hashing serialization |
| `m_mutSync` | Indexed.h:103 | Kademlia index data |
| `m_FileCompleteMutex` | PartFile.h:335 | File completion state |
| `m_mutKnown2File` | SHAHashSet.h:254 | KNOWN2.MET access |
| `m_mutBusy` | UPnPImplMiniLib.h:61 | UPnP discovery state |

### 4.3 CEvent (auto/manual reset)

| Variable | File | Reset | Initial | Purpose |
|----------|------|-------|---------|---------|
| `m_eventThreadEnded` | PartFileWriteThread.h:64 | Manual | Signaled | Write thread done |
| `m_eventThreadEnded` | UploadBandwidthThrottler.h:72 | Manual | Signaled | Throttler done |
| `m_eventPaused` | UploadBandwidthThrottler.h:73 | Manual | Signaled | Throttler paused |
| `m_eventDataAvailable` | UploadBandwidthThrottler.h:74 | Manual | Unsignaled | Upload data ready |
| `m_eventSocketAvailable` | UploadBandwidthThrottler.h:75 | Manual | Unsignaled | Socket ready |
| `m_eventThreadEnded` | UploadDiskIOThread.h:59 | Manual | Unsignaled | Read thread done |

### 4.4 Atomic / Interlocked

Used extensively for flag variables crossing the UI thread ↔ worker thread boundary:

- `InterlockedExchange8` — single-byte flags (`m_bNewData`, `m_Run`) in `PartFileWriteThread`,
  `UploadDiskIOThread`
- `InterlockedExchange` / `InterlockedExchange64` — byte counters in `EMSocket`,
  `UploadBandwidthThrottler`
- `InterlockedIncrement` / `InterlockedDecrement` — reference counting in `CustomAutoComplete`,
  `MediaInfo`, `UPnPImplWinServ`

### 4.5 Volatile Flags (not atomically safe by themselves)

```
ArchiveRecovery.h:404   volatile bool m_bIsValid
HttpDownloadDlg.h:96    volatile bool m_bAbort
Indexed.h:104           volatile bool m_bAbortLoading
Indexed.h:105           volatile bool m_bDataLoaded
Kademlia.h:102          volatile bool m_bRunning
PartFile.h:343          volatile bool m_bPreviewing         ← NO LOCK — race condition
PartFile.h:344          volatile bool m_bRecoveringArchive  ← NO LOCK — race condition
PartFile.h:386          volatile WPARAM m_uFileOpProgress
PartFile.h:405          volatile EPartFileOp m_eFileOp
PartFileWriteThread.h   volatile char m_Run, m_bNewData
UploadBandwidthThrottler volatile bool m_bRun
UploadBandwidthThrottler volatile LONG m_needsMoreBandwidthSlots
UploadDiskIOThread.h    volatile char m_Run, m_bNewData
UPnPImpl.h:69           volatile TRISTATE m_bUPnPPortsForwarded
UPnPImplMiniLib.h:70    volatile bool m_bAbortDiscovery
```

On x64, `volatile` alone does not guarantee atomicity for types wider than a machine word, nor
does it provide a memory fence. The correct modern replacement is `std::atomic<T>` with explicit
`memory_order` annotations. The `InterlockedExchange8` calls in `PartFileWriteThread` and
`UploadDiskIOThread` are correct; the bare `volatile bool` flags elsewhere are technically
data races under the C++11 memory model.

---

## 5. I/O Completion Port Usage (Good Patterns to Extend)

Two threads already use IOCP correctly — these are the best-engineered parts of the threading model:

### 5.1 CPartFileWriteThread (PartFileWriteThread.cpp)

```cpp
m_hPort = ::CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, 0, 1);   // line 67
// per-file association:
::CreateIoCompletionPort(pFile->m_hWrite, m_hPort, completionKey, 0); // line 155
// main loop:
::GetQueuedCompletionStatus(m_hPort, &dwWrite, &completionKey,
                             (LPOVERLAPPED*)&pCurIO, INFINITE);        // line 76
// issue:
::WriteFile(pFile->m_hWrite, ..., NULL, (LPOVERLAPPED)pOvWrite);      // line 140
```

- Dedicated thread with concurrency limit 1
- Zero-copy: overlapped write issued, completion dequeued in the same thread
- Termination via `PostQueuedCompletionStatus(m_hPort, 0, 0, NULL)` (line 61)

### 5.2 CUploadDiskIOThread (UploadDiskIOThread.cpp)

Identical IOCP pattern for file reads supplying upload data.

These two threads are the template for how network I/O should also be handled.

---

## 6. Inter-Thread Message Types

### 6.1 Worker → UI Thread (TM_* messages, EmuleDlg.h:319)

```cpp
TM_FINISHEDHASHING    = WM_APP + 10   // file hashing complete → OnFileHashed
TM_HASHFAILED                         // hashing error → OnHashFailed
TM_IMPORTPART                         // partial import data → OnImportPart
TM_FRAMEGRABFINISHED                  // video frame ready → OnFrameGrabFinished
TM_FILEALLOCEXC                       // alloc exception → OnFileAllocExc
TM_FILECOMPLETED                      // file done → OnFileCompleted
TM_FILEOPPROGRESS                     // progress update → OnFileOpProgress
TM_CONSOLETHREADEVENT                 // terminal services event
```

### 6.2 UI subsystem messages (UM_* messages, UserMsgs.h:4)

30+ UM_ message codes used for web interface ↔ UI, UPnP results, media info, tab
control events, archive scan completion, etc.

### 6.3 Async Socket Messages (AsyncSocketEx.h:82)

```cpp
WM_SOCKETEX_TRIGGER  = WM_USER + 0x101      // layer notification
WM_SOCKETEX_GETHOST  = WM_USER + 0x102      // WSAAsyncGetHostByName reply
WM_SOCKETEX_CALLBACK = WM_USER + 0x103      // pending callback dispatch
WM_SOCKETEX_NOTIFY   = WM_USER + 0x104      // FD_READ/WRITE/ACCEPT/CLOSE per socket
// + WM_SOCKETEX_NOTIFY + nSocketIndex for each of up to 47869 sockets
```

---

## 7. Known Bugs in Current Threading

### 7.1 Unsynchronized volatile Flags — Data Races (PartFile.h)

`m_bPreviewing` (line 343) and `m_bRecoveringArchive` (line 344) are `volatile bool` fields
checked and set from the UI thread and from worker threads without any lock or atomic operation.
Under the C++11/14/17 memory model this is undefined behaviour. On x64 the generated code happens
to be correct today (single-byte store/load), but a compiler with aggressive optimization can
legally reorder or eliminate these accesses.

**Fix:** Replace with `std::atomic<bool>` and remove `volatile`.

### 7.2 CEMSocket Static Global Read Buffer

```cpp
// EMSocket.cpp:258
static char GlobalReadBuffer[2000000];
```

This 2 MB static buffer is shared across all socket receive calls. Because all sockets run on the
UI thread this is currently safe (no two `OnReceive` calls can overlap). If socket processing is
ever moved to multiple threads this will become an immediately-exploitable buffer aliasing bug.

**Fix:** Per-socket receive buffer, or thread-local allocation.

### 7.3 Mixed Use of CMutex and CCriticalSection for Same Resources

Some paths use kernel-mode `CMutex` (slow, supports named/cross-process access) for resources
that never leave the process. E.g., `hashing_mut` in `Emule.h:119` uses `CMutex`. This is 10–50×
slower than `CCriticalSection` / `CRITICAL_SECTION` for uncontended lock acquisition.

**Fix:** Replace intra-process `CMutex` with `CRITICAL_SECTION` or `std::mutex`.

### 7.4 `WaitForSingleObject(INFINITE)` on UI Thread

Several paths block the UI thread indefinitely:

```cpp
// CreditsDlg.cpp:107
WaitForSingleObject(m_pThread->m_hThread, INFINITE);
// HttpDownloadDlg.cpp:728
WaitForSingleObject(m_pThread->m_hThread, INFINITE);
```

These freeze the entire application until the thread finishes. On a slow/stalled network or slow
disk this can hang the UI for seconds.

---

## 8. Path to a Real Threading Model

The changes fall into two independent tracks:

- **Track A** — move network I/O off the UI thread (replaces `WSAAsyncSelect`)
- **Track B** — clean up worker thread hygiene (safer, easier, independent of Track A)

---

### Track A: Replace WSAAsyncSelect with I/O Completion Ports

#### A1. The Root Change

Replace the `CAsyncSocketEx` helper window dispatch mechanism with a dedicated network thread
running an IOCP loop. All socket I/O moves off the UI thread permanently.

**New architecture:**

```
[Network Thread (IOCP)]
  CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1)
  for each TCP/UDP socket:
      WSARecv/WSASend with OVERLAPPED
      associate socket with IOCP
  loop: GetQueuedCompletionStatus(...)
      → dispatch to per-socket OnReceive / OnSend / OnClose handlers
      → protocol parsing happens HERE, off UI thread
      → results queued to UI thread via PostMessage or lock-free queue

[UI Thread]
  No socket calls at all
  Receives parsed events via message queue or concurrent queue
  Updates UI state only
```

This mirrors exactly what `CPartFileWriteThread` and `CUploadDiskIOThread` already do for file I/O.
The pattern is proven and already in the codebase — it just needs to be applied to sockets.

#### A2. Required Changes to CAsyncSocketEx

`CAsyncSocketEx` needs a complete socket backend swap:

| Current (WSAAsyncSelect) | Replacement (IOCP) |
|--------------------------|-------------------|
| `WSAAsyncSelect(s, hWnd, WM_SOCKETEX_NOTIFY+n, FD_READ\|...)` | `WSARecv(s, &wsaBuf, 1, NULL, &flags, &ovl, NULL)` |
| Message-only window `HWND_MESSAGE` | Dedicated network thread |
| `WindowProc` dispatch | `GetQueuedCompletionStatus` loop |
| Per-thread helper window (TLS) | Single IOCP handle shared across all sockets |
| Socket index → message mapping | Completion key = socket pointer |

The external interface of `CAsyncSocketEx` (virtual `OnReceive`, `OnSend`, `OnAccept`, `OnClose`)
stays unchanged so callers don't need to change. Only the backend changes.

#### A3. Thread Safety for Protocol Objects

Once `OnReceive` runs on the network thread instead of the UI thread, any shared state it touches
needs locking. The scope of changes required:

**Immediately affected:**

- `CDownloadQueue` — `AddSource`, `RemoveSource` called from `OnReceive` → needs lock
- `CUploadQueue` — `GetUploadingClientByIP` called from multiple handlers → needs lock
- `CClientList` — `FindClientByIP` etc. → needs lock or read/write separation
- `CPartFile` state updates (block reception, file data written) → needs per-file lock

**Less affected (already thread-safe or isolated):**

- `CPartFileWriteThread` / `CUploadDiskIOThread` — already off UI thread, unaffected
- `UploadBandwidthThrottler` — already off UI thread with its own locks
- File hashing threads — post messages, no shared state issue

#### A4. Kademlia Thread

Once the IOCP network thread exists, Kademlia processing should move to it (or to its own
dedicated thread). The current `CKademlia::Process()` is called from the 1-second timer on the
UI thread and mixes routing table maintenance, zone timers, UPnP refresh, and firewall checking.

Moving Kademlia to its own thread:

```cpp
// New: KademliaThread.cpp
class CKademliaThread : public CWinThread {
    // or: std::jthread + std::stop_token (C++20, supported by MSVC v143)
    void Run() {
        while (!m_stop.stop_requested()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            Kademlia::CKademlia::Process();  // needs internal locking
        }
    }
    std::stop_source m_stop;
};
```

The main interactions Kademlia has with the rest of eMule:

- Reads/writes routing table → internal lock (already partially locked via `CMutex m_mutSync`)
- Posts source finds back to download queue → safe via PostMessage
- Calls `theApp.emuledlg->RefreshUPnP()` → must become PostMessage to UI thread

#### A5. DNS Resolution

Currently uses `WSAAsyncGetHostByName` → `WM_HOSTNAMERESOLVED` message on UI thread:

```cpp
// DownloadQueue.cpp:1496
WSAAsyncGetHostByName(m_hWnd, WM_HOSTNAMERESOLVED, hostname, buf, len);
```

`WSAAsyncGetHostByName` is deprecated. Replacement on Windows 10+:

```cpp
// Option 1: GetAddrInfoExW with completion callback (non-blocking, Win8+)
GetAddrInfoExW(hostname, NULL, NS_DNS, NULL, &hints, &result, NULL,
               &overlapped, CompletionCallback, &cancelHandle);

// Option 2: std::async + getaddrinfo (blocking in thread pool)
auto fut = std::async(std::launch::async, [hostname]() {
    addrinfo *res = nullptr;
    getaddrinfo(hostname, nullptr, nullptr, &res);
    return res;
});
```

---

### Track B: Worker Thread Hygiene

Independent of Track A — these can be done now without touching the socket architecture.

#### B1. Replace volatile bool with std::atomic\<bool\>

All `volatile bool` fields used as cross-thread flags:

```cpp
// Before (PartFile.h:343-344)
volatile bool m_bPreviewing;
volatile bool m_bRecoveringArchive;

// After
std::atomic<bool> m_bPreviewing{false};
std::atomic<bool> m_bRecoveringArchive{false};
```

Repeat for all 20 volatile flag variables listed in section 4.5. The `volatile` keyword is not a
synchronization primitive in C++11+. `std::atomic<bool>` with default `memory_order_seq_cst`
is a safe direct replacement.

#### B2. Replace intra-process CMutex with std::mutex

`CMutex` uses a kernel object and requires a syscall even for uncontended acquisition (~200 ns
vs ~5 ns for an uncontended `CRITICAL_SECTION`). None of the in-process mutexes need cross-process
or named mutex capabilities.

```cpp
// Before (Emule.h:119)
CMutex hashing_mut;

// After
std::mutex hashing_mut;
// Usage: std::lock_guard<std::mutex> lock(hashing_mut);
// or:    std::unique_lock<std::mutex> lock(hashing_mut);
```

Affected: `hashing_mut`, `m_mutSync` (Indexed), `m_FileCompleteMutex` (PartFile),
`m_mutWriteList` (SharedFileList). Keep `m_mutKnown2File` as a named `CMutex` only if
cross-process access to KNOWN2.MET from external tools is required; otherwise replace it too.

#### B3. Replace CWinThread with std::jthread (C++20)

MSVC v143 fully supports `std::jthread` and `std::stop_token`. These replace `CWinThread`
subclasses with a cleaner, exception-safe, automatically-joining model:

```cpp
// Before (AICHSyncThread.h:23 pattern)
class CAICHSyncThread : public CWinThread {
    DECLARE_DYNCREATE(CAICHSyncThread)
    virtual BOOL InitInstance();
    virtual int Run();
    // manually managed lifecycle
};
// AfxBeginThread(RUNTIME_CLASS(CAICHSyncThread), ...);

// After
std::jthread m_aichSyncThread([](std::stop_token st) {
    SetThreadDescription(GetCurrentThread(), L"AICHSyncThread");
    while (!st.stop_requested()) {
        // do sync work
        // wait on stop token or condition
    }
});
// m_aichSyncThread.request_stop(); // cooperative stop
// destructor auto-joins
```

Short-lived fire-and-forget threads (file hashing, frame grabbing, UPnP discovery) can use
`std::async(std::launch::async, ...)` or a thread pool instead of creating a new OS thread each
time:

```cpp
// Before: AfxBeginThread(RUNTIME_CLASS(CAddFileThread), ...) per file
// After:
auto future = std::async(std::launch::async, [file]() {
    HashFile(file);
    // PostMessage result back to UI
});
```

#### B4. Fix WaitForSingleObject(INFINITE) on UI Thread

```cpp
// CreditsDlg.cpp:107 — BLOCKS UI THREAD
WaitForSingleObject(m_pThread->m_hThread, INFINITE);
```

Replace with message-pump-safe wait:

```cpp
// Pump messages while waiting
while (MsgWaitForMultipleObjects(1, &m_pThread->m_hThread, FALSE,
                                  INFINITE, QS_ALLINPUT) == WAIT_OBJECT_0 + 1) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        DispatchMessage(&msg);
}
```

Or, better: convert to async model — thread posts a message when done, UI thread handles it in
the normal message loop instead of blocking.

#### B5. Eliminate the 2 MB Static Receive Buffer

```cpp
// EMSocket.cpp:258 — shared, not thread-safe across multiple threads
static char GlobalReadBuffer[2000000];
```

Replace with a per-socket dynamically sized buffer or thread-local allocation:

```cpp
// Thread-local (safe if one thread per socket)
thread_local std::array<char, 65536> tl_recvBuf;

// Or per-socket (required if multiple sockets share a thread)
std::vector<uint8_t> m_recvBuf;  // in CEMSocket, sized in ctor
```

#### B6. Replace InterlockedExchange8 with std::atomic\<char\>

`InterlockedExchange8` is correct but non-portable and verbose:

```cpp
// Before (PartFileWriteThread.h:67)
volatile char m_bNewData;
// ...
InterlockedExchange8(&m_bNewData, 0);

// After
std::atomic<char> m_bNewData{0};
// ...
m_bNewData.store(0, std::memory_order_relaxed);  // or acquire/release as needed
```

---

## 9. Migration Sequence

The safest order to make these changes without destabilizing the app:

```
Step 1 — Track B (no architecture change, pure cleanup)
    1a. volatile bool → std::atomic<bool>  (PartFile.h first, then all others)
    1b. CMutex → std::mutex  (hashing_mut, m_mutSync, m_FileCompleteMutex)
    1c. InterlockedExchange8 → std::atomic<char>  (PartFileWriteThread, UploadDiskIOThread)
    1d. Static GlobalReadBuffer → thread_local / per-socket buffer
    1e. Remove WaitForSingleObject(INFINITE) on UI thread (CreditsDlg, HttpDownloadDlg)

Step 2 — Kademlia thread isolation
    2a. Add internal locking to CKademlia data (routing table, search list)
    2b. Move CKademlia::Process() call from UploadTimer to a dedicated std::jthread
    2c. Make UPnP refresh call thread-safe (PostMessage instead of direct call)
    2d. Validate: all Kademlia-sourced calls to downloadqueue/clientlist are now marshalled

Step 3 — DNS modernization (independent of Step 2)
    3a. Replace WSAAsyncGetHostByName with GetAddrInfoExW + completion callback
    3b. Remove WM_HOSTNAMERESOLVED handler and OnHostnameResolved message map entry

Step 4 — Network thread (largest change, requires Step 1 complete)
    4a. Build IOCP-backed network thread class alongside existing CAsyncSocketEx (don't delete yet)
    4b. Port ListenSocket, CEMSocket, ClientUDPSocket to post work items to network thread
    4c. Add locking to all shared objects touched from OnReceive (CDownloadQueue, CClientList, etc.)
    4d. Validate on a test build: UI thread must make zero socket calls
    4e. Delete old CAsyncSocketExHelperWindow, remove all WM_SOCKETEX_* messages

Step 5 — CWinThread → std::jthread (optional, cosmetic, low risk)
    5a. Replace fire-and-forget threads with std::async
    5b. Replace persistent worker threads with std::jthread
```

---

## 10. What Stays the Same

Some parts of the current design are correct and should not be touched:

- **CPartFileWriteThread / CUploadDiskIOThread** — IOCP-based, already the right model
- **UploadBandwidthThrottler** — properly isolated, correct synchronization, leave as-is
- **PostMessage pattern** for worker→UI communication — correct, safe, keep it
- **TM_* / UM_* message dispatch** — correct architecture, keep message types
- **Per-socket TLS helper window pattern** — replaced in Track A but the TLS mechanism itself
  is correct and the pattern can be reused for the network thread's per-thread state

---

## 11. Estimated Impact

| Change | UI Responsiveness | Throughput | Risk |
|--------|------------------|-----------|------|
| volatile → std::atomic | None | None | Very Low |
| CMutex → std::mutex | ~200ns/lock improvement | Minor | Very Low |
| Remove WFSO(INFINITE) on UI | Eliminates UI freeze during waits | None | Low |
| Static recv buffer → per-socket | None (single thread today) | None | Low |
| Kademlia → own thread | Removes ~2ms/sec from UI | Minor | Medium |
| DNS → GetAddrInfoExW | Eliminates legacy API warning | None | Low |
| Network IOCP thread | Eliminates all socket jank from UI | Major at high peer counts | High |
| CWinThread → std::jthread | None | None | Low |
