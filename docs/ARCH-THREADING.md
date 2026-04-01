# eMule Threading Model — Analysis & Improvement Paths

**Platform: Windows 10+ x64 ONLY**
**Build: MSVC v143 (VS 2022), MFC static**
**Codebase: srchybrid/**

---

## Table of Contents

- [Executive Summary](#executive-summary)
- [1. Thread Inventory](#1-thread-inventory)
- [2. The Central Timer Tick](#2-the-central-timer-tick)
- [3. Historical: The AsyncSocketEx Helper Window Model](#3-historical-the-asyncsocketex-helper-window-model)
- [4. Synchronization Primitives in Use](#4-synchronization-primitives-in-use)
- [5. I/O Completion Port Usage](#5-io-completion-port-usage-good-patterns-to-extend)
- [6. Inter-Thread Message Types](#6-inter-thread-message-types)
- [7. Known Bugs in Current Threading](#7-known-bugs-in-current-threading)
- [8. Path to a Real Threading Model](#8-path-to-a-real-threading-model)
- [9. Migration Sequence](#9-migration-sequence)
- [10. What Stays the Same](#10-what-stays-the-same)
- [11. Estimated Impact](#11-estimated-impact)
- [12. Feature Identifiers](#12-feature-identifiers) (FEAT_029, FEAT_030)

---

## Executive Summary

eMule now uses a hybrid threading model with a dedicated `WSAPoll` network backend for live socket
readiness, while the UI thread still drives protocol scheduling, rendering, and most high-level app
state. TCP and UDP socket ownership no longer lives on the UI thread; socket readiness is owned by
the shared poll backend, and UDP dispatch is marshalled back to the app thread where needed.

The result is materially better than the original helper-window model, but the UI thread still owns
download/upload scheduling, Kademlia processing, and a large amount of shared mutable state. That
means responsiveness and reasoning under load are improved on the transport side, but the app is
not yet at the final IOCP-style architecture described later in this document.

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
| `CLoadDataThread` | Indexed.h:64 | BELOW_NORMAL | Load Kademlia index data from disk | CMutex m_mutSync + volatile flags |
| `CStartDiscoveryThread` | UPnPImplMiniLib.h:36 | NORMAL | miniupnpc UPnP device discovery | CMutex m_mutBusy |

### 1.2 Everything Else: The UI Thread

The UI thread still owns the 100ms `SetTimer` loop and the bulk of protocol scheduling work:

- All eMule protocol parsing and processing
- Kademlia routing, search, bootstrap, firewall check (`CKademlia::Process`)
- Download queue scheduling (`CDownloadQueue::Process`)
- Upload queue scheduling (`CUploadQueue::Process`)
- Server connection management (`CServerConnect`)
- Known file list, client list, credit processing

The live socket and resolver backends are now off the UI thread:

- TCP accept, connect, receive, send readiness (`ListenSocket`, `CEMSocket`, `CClientReqSocket`) via `WSAPoll`
- UDP receive/send readiness (`CClientUDPSocket`, `CUDPSocket`) via `WSAPoll`
- Source hostname DNS resolution via the `CDownloadQueue` resolver worker
- Server UDP hostname DNS resolution via the `CUDPSocket` resolver worker

---

## 2. The Central Timer Tick

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

Every one of these runs synchronously on the UI thread, one after another, before the app can
return to ordinary message processing.

### 2.2 Live Socket Dispatch Relative to Timer Ticks

Between timer ticks, live TCP readiness is handled by the dedicated `WSAPoll` network thread and
live UDP readiness is posted back to the app thread through `UM_WSAPOLL_UDP_SOCKET`.

A single `CEMSocket::OnReceive` call reads up to **2 MB** from the socket into a static global
buffer:

```cpp
// EMSocket.cpp:258
static char GlobalReadBuffer[2000000];
```

Once control reaches the existing protocol handlers, packet parsing and state transitions still
happen inline on the owning thread. Under heavy load this means the UI thread can still spend long
stretches inside protocol work even though transport readiness is no longer helper-window driven.

---

## 3. Historical: The AsyncSocketEx Helper Window Model

This section describes the pre-`WSAPoll` helper-window design that earlier audits and migration
notes refer to. It is historical branch context, not the current live transport path.

Before the transport migration, `CAsyncSocketEx` used **`WSAAsyncSelect`** — the oldest Windows
async socket API, dating to Winsock 1.1. It worked by posting a window message for each socket
event to a registered `HWND`.

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

### 3.1 Why This Was the Core Problem

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

30+ UM_ message codes used for UI coordination, UPnP results, media info, tab
control events, archive scan completion, etc.

### 6.3 Async Socket Messages (AsyncSocketEx.h:82)

```cpp
WM_SOCKETEX_TRIGGER  = WM_USER + 0x101      // layer notification
WM_SOCKETEX_GETHOST  = WM_USER + 0x102      // historical WSAAsyncGetHostByName reply
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

## 8. Remaining Path to a Real Threading Model

The changes fall into two independent tracks:

- **Track A** — continue from the current `WSAPoll` bridge toward a final IOCP model
- **Track B** — clean up worker thread hygiene (safer, easier, independent of Track A)

---

### Track A0: Historical Bridge Step Completed

`WSAPoll` is the smallest Windows-native step away from helper-window socket dispatch. It keeps the
code in the classic non-overlapped `socket`/`connect`/`send`/`recv`/`accept` style, but removes the
hard dependency on an `HWND`, `WM_SOCKETEX_NOTIFY`, and the UI thread's message pump.

This is a valid **transitional backend** if the immediate goal is:

- stop running network readiness detection on the UI thread
- remove the hidden helper window and `WSAAsyncSelect` plumbing
- preserve most of the current `CAsyncSocketEx` external contract while deferring a full IOCP rewrite

It is **not** the final high-scale Windows design. `WSAPoll` is still an `O(n)` readiness scan and
still requires manual interest-mask management for every socket.

This track is complete in the current branch: live TCP and UDP transport now sit on the shared
`WSAPoll` backend, and the old helper-window resolver/message routes are gone from the runtime path.

#### A0.1 What Changes and What Stays the Same

**What changes:**

- `CAsyncSocketEx` no longer calls `WSAAsyncSelect(...)`
- there is no helper window and no `WM_SOCKETEX_NOTIFY + index` dispatch path
- a dedicated network thread owns the poll loop and calls socket handlers explicitly
- socket readiness becomes "pull from a `WSAPOLLFD[]` set" instead of "receive a Win32 message"

**What can stay the same:**

- the public `CAsyncSocketEx` API (`Create`, `Connect`, `AsyncSelect`, `Send`, `Receive`, `Close`)
- the virtual callback shape (`OnReceive`, `OnSend`, `OnConnect`, `OnAccept`, `OnClose`)
- the higher-level protocol classes (`CEMSocket`, `CClientReqSocket`, `CServerSocket`) if the backend
  still invokes those callbacks in the same semantic order

#### A0.2 Why This Is Not a Drop-In API Swap

The current backend relies on more than "readable / writable / closed":

- `AsyncSocketEx.cpp` keeps per-socket state (`connecting`, `connected`, `listening`, `attached`,
  `aborted`, `closed`) and transitions it inside the helper-window dispatcher.
- `m_nPendingEvents` stores `FD_READ` / `FD_WRITE` that arrived while a nonblocking connect is still
  pending, then replays them after `FD_CONNECT`.
- `FD_CLOSE` is special-cased: if bytes are still available, the code resends a close notification
  and invokes `OnReceive(WSAESHUTDOWN)` before `OnClose`.
- older layer-chain code assumed the backend could deliver synthetic `FD_*` events, not just raw
  socket readiness bits. That compatibility burden is gone now that the proxy/layer path has been removed.

`WSAPoll` gives only `revents` flags. It does **not** reproduce any of that behavior automatically.
The backend must rebuild it explicitly.

#### A0.3 Required Backend Shape in This Codebase

The minimum viable `WSAPoll` backend looks like this:

```text
[Network Thread]
  owns vector<WSAPOLLFD>
  owns vector<CAsyncSocketEx*>
  owns command queue (add socket / remove socket / change interest mask / stop)
  loop:
      apply queued commands
      build pollfd.events from each socket's current desired state
      rc = WSAPoll(pollfds.data(), pollfds.size(), timeout_ms)
      for each ready socket:
          translate revents -> FD_* semantic events
          call backend dispatcher
          possibly queue UI work / protocol work

[UI Thread]
  no WSAAsyncSelect
  ideally no direct socket calls
  receives marshalled UI updates only
```

Key implementation point: `AsyncSelect(long lEvent)` should survive as the **public interest-mask
API**, but internally it should only update desired state on the socket object and enqueue a command
to the poll thread. It must no longer touch WinSock notification APIs directly.

#### A0.4 Event Mapping from Current FD_* Semantics to WSAPoll

| Current semantic event | Current source | `WSAPoll` interest | Backend action |
|------------------------|----------------|--------------------|----------------|
| `FD_ACCEPT` | Helper-window `FD_ACCEPT` | `POLLIN` on listening socket | Loop `accept()` until `WSAEWOULDBLOCK`; call `OnAccept(0)` or synthetic accept dispatcher |
| `FD_CONNECT` | Helper-window `FD_CONNECT` | `POLLOUT` while connect is pending | Use `getsockopt(SO_ERROR)` to determine connect result; call `OnConnect(err)` |
| `FD_READ` | Helper-window `FD_READ` | `POLLIN` | Call `OnReceive(0)` or layer `CallEvent(FD_READ, 0)` |
| `FD_WRITE` | Helper-window `FD_WRITE` | `POLLOUT` only when send queue is non-empty or connect is pending | Call `OnSend(0)` when writable work exists |
| `FD_CLOSE` | Helper-window `FD_CLOSE` | `POLLHUP`, `POLLERR`, failed `recv`, or `recv == 0` | Preserve current "drain readable bytes before close" behavior, then call `OnClose(err)` |

Important detail: unlike `FD_WRITE` message delivery, `POLLOUT` will often remain continuously true
for a healthy connected TCP socket. If the backend leaves `POLLOUT` armed all the time, the poll loop
will wake almost constantly and spin. Therefore:

- only arm write interest while a nonblocking connect is pending
- or while a socket actually has queued outbound data that could not be fully sent earlier

That single rule determines whether a `WSAPoll` backend is efficient or noisy.

#### A0.5 Callback Translation Rules

To preserve current behavior, the poll backend must apply the following rules:

1. **Connect completion**
   - When `connect()` returns `WSAEWOULDBLOCK`, mark `connectPending = true` and arm `POLLOUT`.
   - On `POLLOUT`, call `getsockopt(SOL_SOCKET, SO_ERROR, ...)`.
   - If `SO_ERROR == 0`, transition to `connected`, invoke `OnConnect(0)`, then replay any deferred
     read/write events exactly like the current `m_nPendingEvents` logic.
   - If `SO_ERROR != 0`, invoke `OnConnect(error)` and close or retry according to existing rules.

2. **Readable data**
   - `POLLIN` means "some read-side progress is possible", not "exactly one packet is ready".
   - Keep sockets nonblocking and let `OnReceive` / lower layers pull until they hit `WSAEWOULDBLOCK`,
     just like today.

3. **Close ordering**
   - If hangup/error arrives but `FIONREAD` still reports unread bytes, preserve the current close
     contract: deliver read-side work first, then a close notification.
   - This matters because `CEMSocket` still expects to finish packet reassembly before final close.

4. **Listening sockets**
   - `POLLIN` on a listening socket means "one or more accepts are possible".
   - The backend should loop `accept()` until `WSAEWOULDBLOCK` instead of issuing one accept per poll
     cycle, otherwise connection bursts will backlog unnecessarily.

5. **Layer chain**
   - If `m_pFirstLayer` exists, the poll backend should continue dispatching synthetic `FD_*` semantic
     events into `m_pLastLayer->CallEvent(...)`.
   - The layer chain is a semantic consumer of event types; it should not be forced to interpret raw
     `WSAPOLLFD::revents` bits.

#### A0.6 UDP Scope

A full migration could not stop at `CAsyncSocketEx` alone:

- historically `CUDPSocket` derived from plain `CAsyncSocket`
- historically `CClientUDPSocket` also derived from plain `CAsyncSocket`

So there are two realistic options:

- **TCP-only interim migration**: move `CAsyncSocketEx` users to `WSAPoll`, leave UDP on MFC
  `CAsyncSocket` temporarily. This reduces TCP/UI coupling but keeps mixed socket models in the app.
- **Full poll-thread migration**: wrap both UDP classes into the poll backend too, so one network
  thread owns both TCP and UDP readiness dispatch.

The second option is cleaner. The first option is lower-risk but only partially solves the "UI thread
is the network thread" problem.

#### A0.7 Poll Loop Sketch

```cpp
for (;;) {
    ApplyQueuedCommands(); // add/remove sockets, update interest masks, stop flag

    BuildPollArrayFromSocketState();

    const int rc = WSAPoll(m_pollfds.data(), static_cast<ULONG>(m_pollfds.size()), m_timeoutMs);
    if (m_stopRequested)
        break;
    if (rc <= 0)
        continue; // timeout or transient error handling

    for (size_t i = 0; i < m_pollfds.size(); ++i) {
        WSAPOLLFD &pfd = m_pollfds[i];
        if (!pfd.revents)
            continue;

        CAsyncSocketEx *sock = m_socketByIndex[i];
        if (!sock)
            continue;

        if (sock->IsConnectPending() && (pfd.revents & (POLLOUT | POLLERR | POLLHUP))) {
            HandleConnectCompletion(*sock);
            continue;
        }

        if (sock->IsListening() && (pfd.revents & POLLIN))
            HandleAcceptBurst(*sock);

        if (pfd.revents & POLLIN)
            DispatchSemanticEvent(*sock, FD_READ, 0);

        if ((pfd.revents & POLLOUT) && sock->WantsWrite())
            DispatchSemanticEvent(*sock, FD_WRITE, 0);

        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            HandleReadableThenClose(*sock);
    }
}
```

The exact function boundaries can differ, but the control flow above is the essential replacement for
the current `CAsyncSocketExHelperWindow::WindowProc`.

#### A0.8 State That the Backend Must Own Explicitly

`WSAAsyncSelect` currently hides some of the "which events are armed?" bookkeeping inside WinSock.
A poll backend must surface that state in the socket object or backend record:

- `desiredEvents` or reuse of `m_lEvent`
- `connectPending`
- `wantWrite` or "send queue non-empty"
- `closeNotified`
- `isListening`
- `isUdp`
- deferred read/write bits currently represented by `m_nPendingEvents`

This means the backend rewrite is not just a new thread class. It also requires refactoring
`CAsyncSocketEx` from "WinSock owns readiness subscriptions" to "our backend owns readiness intent".

#### A0.9 Threading Consequences

Moving readiness detection to a poll thread only helps if protocol processing moves with it.

There are two possible designs:

- **Poll thread only, but marshal every callback back to UI**:
  preserves more legacy assumptions, but the UI thread still parses packets and remains the real
  bottleneck. This removes `WSAAsyncSelect` but does not remove the architectural problem.
- **Poll thread owns readiness and protocol callbacks directly**:
  this actually decouples networking from the UI, but it requires the same shared-state locking work
  that the IOCP track requires.

For this codebase, the second model is the only version worth implementing.

#### A0.10 Strengths and Weaknesses Versus IOCP

**Why `WSAPoll` is attractive:**

- much smaller conceptual jump from current synchronous-style socket code
- no overlapped I/O structures
- easier to debug initially
- easier to stage behind the existing `CAsyncSocketEx` callback interface

**Why `WSAPoll` is still second-best:**

- readiness scanning is still linear in the number of sockets
- no kernel completion queue
- manual write-interest management is mandatory
- burst handling is less efficient than overlapped accept / recv / send
- once locking is added for off-UI-thread protocol work, much of the hard concurrency work has
  already been paid, so the remaining jump to IOCP is smaller than it first appears

#### A0.11 Recommendation

`WSAPoll` is a reasonable **bridge**, not the final target.

Choose it only if the immediate requirement is:

- remove hidden-window / message-pump socket dispatch quickly
- keep the current `send` / `recv` / `accept` style code for one migration stage
- accept that a second backend migration to IOCP may still follow later

Skip it and go straight to IOCP if the real goal is:

- highest Windows scalability
- minimum per-socket CPU overhead
- a backend that will not need replacing again

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

The live tree no longer uses `WSAAsyncGetHostByName`. Named endpoints now resolve off-thread and
hand completed IPv4 results back to the owning component:

- `CUDPSocket` resolves dynIP/server hostnames on its resolver worker before queued UDP sends.
- `CDownloadQueue` resolves unresolved source hostnames on a dedicated worker and drains the
  completions during `CDownloadQueue::Process()`.

That removes the last hidden resolver windows from the live networking path while preserving
main-thread mutation of `CPartFile` and download-queue state.

The remaining modernization choice is about the API shape, not the architecture baseline:

```cpp
// Option 1: keep worker-thread getaddrinfo for simple serialized queues
addrinfo *res = nullptr;
getaddrinfo(hostname, NULL, &hints, &res);

// Option 2: adopt GetAddrInfoExW cancellation/completion if finer-grained resolver control is needed
GetAddrInfoExW(hostname, NULL, NS_DNS, NULL, &hints, &result, NULL,
               &overlapped, CompletionCallback, &cancelHandle);
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

## 9. Historical Migration Sequence and Remaining Steps

The sequence below started as the safest way to land the transport migration. Several early steps
are now complete and are kept here as historical context for the branch:

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

Step 3 — DNS modernization (completed in current branch)
    3a. Replace WSAAsyncGetHostByName with worker-thread getaddrinfo-based resolution
    3b. Remove WM_HOSTNAMERESOLVED handler and OnHostnameResolved message map entry

Step 4 — Network thread (WSAPoll bridge completed; IOCP follow-up remains)
    4a. WSAPoll-backed network thread landed for TCP and UDP
    4b. Port ListenSocket, CEMSocket, ClientUDPSocket to post work items to the network thread
    4c. Add locking to all shared objects touched from OnReceive (CDownloadQueue, CClientList, etc.)
    4d. Validate on a test build: UI thread must make zero socket calls
    4e. Final backend: replace the poll loop with IOCP overlapped operations if proceeding to the end-state design
    4f. Old CAsyncSocketExHelperWindow and WM_SOCKETEX_* runtime path already removed

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

---

## 12. Feature Identifiers

### FEAT_029: Track B — Worker Thread Hygiene

Covers the Track B improvements described in sections 8.2 (B1-B6) of this document:

- Replace all `volatile bool` cross-thread flags with `std::atomic<bool>` (B1)
- Replace intra-process `CMutex` with `std::mutex` (B2)
- Replace `CWinThread` subclasses with `std::jthread` / `std::async` (B3)
- Fix `WaitForSingleObject(INFINITE)` on the UI thread (B4)
- Eliminate the 2 MB static `GlobalReadBuffer` (B5)
- Replace `InterlockedExchange8` with `std::atomic<char>` (B6)

**Status:** Not yet started. These are independent of the network architecture and can be done incrementally.

### FEAT_030: Track A — Network IOCP Migration

Covers the Track A changes described above. The `WSAPoll` bridge in Track A0 is optional; FEAT_030
targets the final IOCP end state:

- Replace `WSAAsyncSelect` + helper window with a dedicated IOCP network thread (A1)
- Rewrite `CAsyncSocketEx` backend from message-based to overlapped I/O (A2)
- Add thread-safe locking to protocol objects (`CDownloadQueue`, `CClientList`, etc.) (A3)
- Move Kademlia processing to its own `std::jthread` (A4)
- Keep the current worker-thread DNS path or move to `GetAddrInfoExW` only if cancellation/completion semantics become necessary (A5)

**Status:** Transport bridge complete, final IOCP end-state not started. The main remaining socket-side work is operational hardening and any future IOCP decision, not helper-window migration.
