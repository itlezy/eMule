# eMule REST API Server — Named Pipe + Node.js Sidecar

**Status:** Implemented / Canonical Contract
**Related:** `WEB_APIs.md` (in-process approach analysis)

---

## Table of Contents

- [1. Overview](#1-overview)
- [2. Why a Sidecar Instead of In-Process](#2-why-a-sidecar-instead-of-in-process)
- [3. Architecture](#3-architecture)
- [4. Named Pipe Protocol](#4-named-pipe-protocol)
  - [4.1 Transport](#41-transport)
  - [4.2 Message Types](#42-message-types)
  - [4.3 Command Reference](#43-command-reference)
  - [4.4 Event Reference](#44-event-reference)
- [5. Data Types](#5-data-types)
- [6. REST API Endpoints](#6-rest-api-endpoints)
  - [6.1 Auth](#61-auth) — [6.2 Application](#62-application) — [6.3 Stats](#63-stats) — [6.4 Transfers](#64-transfers) — [6.5 Uploads, Servers, Kad, Shared](#65-uploads-servers-kad-shared) — [6.6 Log](#66-log) — [6.7 Search](#67-search) — [6.8 Events (SSE)](#68-events-sse) — [6.9 eMule Extensions](#69-emule-extensions)
- [7. TypeScript Project Structure](#7-typescript-project-structure)
- [8. C++ Side — CPipeApiServer](#8-c-side--cpipeapiserver)
  - [8.1 Class Design](#81-class-design) — [8.2 Pipe Lifecycle](#82-pipe-lifecycle) — [8.3 Event Hook Points](#83-event-hook-points-in-emule) — [8.4 Thread Safety](#84-thread-safety-model) — [8.5 JSON Library](#85-json-library)
- [9. HTTP Error Convention](#9-http-error-convention)
- [10. Implementation Order](#10-implementation-order)
- [11. Out of Scope](#11-out-of-scope)
- [Feature Identifier](#feature-identifier) (PLAN_004)

---

## 1. Overview

Rather than extending the built-in HTML web server (see `WEB_APIs.md`), this document describes an alternative architecture: a **named pipe IPC channel** inside eMule paired with a standalone **Node.js/TypeScript sidecar process** that owns the HTTP surface.

The sidecar exposes a grouped `/api/v2/...` REST API backed by the local pipe contract. Push notifications (download complete, speed stats, etc.) are delivered to HTTP clients via **Server-Sent Events (SSE)**. This document is the canonical published contract for both the in-process pipe server and the sibling `eMule-remote` sidecar.

---

## 2. Why a Sidecar Instead of In-Process

| Concern | In-process REST | Named Pipe + Sidecar |
|---|---|---|
| C++ HTTP/TLS complexity | Stays in C++ | Moves to Node — far better ecosystem |
| eMule crash kills API | Yes | No — sidecar survives |
| API changes require eMule rebuild | Yes | No — sidecar is independent |
| Thread safety burden | High — every handler needs locks | Contained — pipe I/O is one thread |
| Real-time push (SSE/WS) | Painful in C++ | Trivial in Node |
| OpenAPI / tooling | Manual | Auto-generated |
| Deployment | Single binary | Two processes, no install needed (tsx/node) |

The named pipe is **local-only** (same machine), so there is no network attack surface on the IPC channel itself. All authentication lives in the sidecar's HTTP layer.

---

## 3. Architecture

```
┌─────────────────────────────────────────────────────┐
│                    eMule.exe                        │
│                                                     │
│  CPipeApiServer                                     │
│  ├── CreateNamedPipe  \\.\pipe\emule-api            │
│  ├── Read thread  →  JSON command dispatch          │
│  ├── Write  →  JSON responses + event pushes        │
│  └── Event hooks:                                   │
│      ├── EmuleDlg::OnFileCompleted                  │
│      ├── CServerConnect::OnConnected                │
│      ├── periodic stats timer (~1 s)                │
│      └── search result callbacks                   │
└──────────────────┬──────────────────────────────────┘
                   │  \\.\pipe\emule-api
                   │  newline-delimited JSON (JSON-lines)
                   │  full-duplex, single connection
┌──────────────────▼──────────────────────────────────┐
│              emule-sidecar  (Node.js / TypeScript)  │
│                                                     │
│  PipeClient                                         │
│  ├── Connects as named pipe client                  │
│  ├── Framing: split on \n, parse JSON               │
│  ├── Requests matched to responses by UUID id       │
│  ├── Events dispatched to EventBus                  │
│  └── Auto-reconnect with backoff                    │
│                                                     │
│  EventBus  ──→  SseManager  ──→  SSE HTTP clients   │
│                                                     │
│  Fastify HTTP server                                │
│  ├── /api/v2/app/*                                  │
│  ├── /api/v2/stats/*                                │
│  ├── /api/v2/transfers*                             │
│  ├── /api/v2/uploads/*                              │
│  ├── /api/v2/servers/*                              │
│  ├── /api/v2/kad/*                                  │
│  ├── /api/v2/shared/*                               │
│  ├── /api/v2/log*                                   │
│  ├── /api/v2/search/*                               │
│  └── /api/v2/events    (SSE stream)                 │
└─────────────────────────────────────────────────────┘
                   │  HTTP / HTTPS
          REST clients, web UIs, scripts
```

**Key design decisions:**

- **eMule is the named pipe server.** It has the longer lifetime; the sidecar can crash and reconnect freely without eMule noticing beyond a momentary pipe disconnect.
- **One connection, max_instances = 1.** Only one sidecar instance at a time.
- **Auth boundary at HTTP only.** The pipe is local — no credentials needed on the IPC channel.
- **Hashes as hex strings.** eMule MD4 hashes are 16 bytes, exposed as 32-char lowercase hex. Same shape as qBittorrent's SHA1 hashes (40 chars) — clients treat them as opaque strings either way.

---

## 4. Named Pipe Protocol

### 4.1 Transport

- Pipe name: `\\.\pipe\emule-api`
- Mode: `PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED`, byte stream
- Framing: each message is a single line of JSON terminated by `\n` (JSON-lines / NDJSON)
- Encoding: UTF-8

### 4.2 Message Types

Three distinct message shapes share the same wire format:

```
// 1. Request  (sidecar → eMule)
{ "id": "<uuid>", "cmd": "<command>", "params": { ... } }

// 2. Response  (eMule → sidecar)
{ "id": "<uuid>", "result": { ... } }
{ "id": "<uuid>", "error": { "code": "<code>", "message": "<message>" } }

// 3. Event  (eMule → sidecar, unsolicited, no id)
{ "event": "<event-name>", "data": { ... } }
```

**Discrimination rule:** presence of `"event"` key = event; presence of `"id"` without `"cmd"` = response; presence of both `"id"` and `"cmd"` = request.

### 4.3 Command Reference

All commands are sent by the sidecar. eMule replies with a matching `id`.

#### Transfers

| Command | Params | Result |
|---|---|---|
| `transfers/list` | `{ filter?: string, category?: number }` | `Transfer[]` |
| `transfers/get` | `{ hash: string }` | `Transfer` |
| `transfers/pause` | `{ hashes: string[] }` | `{ results: MutationResult[] }` |
| `transfers/resume` | `{ hashes: string[] }` | `{ results: MutationResult[] }` |
| `transfers/stop` | `{ hashes: string[] }` | `{ results: MutationResult[] }` |
| `transfers/delete` | `{ hashes: string[], delete_files?: bool }` | `{ results: MutationResult[] }` |
| `transfers/add` | `{ link: string }` | `{ hash: string, name: string }` |
| `transfers/set_priority` | `{ hash: string, priority: string }` | `{ ok: true }` |
| `transfers/set_category` | `{ hash: string, category: number }` | `{ ok: true }` |
| `transfers/recheck` | `{ hash: string }` | `{ ok: true }` |
| `transfers/sources` | `{ hash: string }` | `Source[]` |

#### Uploads

| Command | Params | Result |
|---|---|---|
| `uploads/list` | — | `Upload[]` (active slots) |
| `uploads/queue` | — | `QueueEntry[]` (waiting) |
| `uploads/remove` | `{ userHash?: string, ip?: string, port?: number }` | `{ ok: true, removed: 'queue'\|'slot' }` |
| `uploads/release_slot` | `{ userHash?: string, ip?: string, port?: number }` | `{ ok: true }` |

#### Shared Files

| Command | Params | Result |
|---|---|---|
| `shared/list` | — | `SharedFile[]` |
| `shared/get` | `{ hash: string }` | `SharedFile` |
| `shared/add` | `{ path: string }` | `{ ok: true, path: string, alreadyShared: boolean, queued: boolean, file: SharedFile \| null }` |
| `shared/remove` | `{ hash?: string, path?: string }` | `{ ok: true, path: string, hash: string \| null }` |

#### Servers (ED2K)

| Command | Params | Result |
|---|---|---|
| `servers/list` | — | `Server[]` |
| `servers/status` | — | `ServerStatus` |
| `servers/connect` | `{ addr?: string, port?: number }` | `ServerStatus` |
| `servers/disconnect` | — | `ServerStatus` |
| `servers/add` | `{ addr: string, port: number, name?: string }` | `Server` |
| `servers/remove` | `{ addr: string, port: number }` | `Server` |

#### Kademlia

| Command | Params | Result |
|---|---|---|
| `kad/status` | — | `KadStatus` |
| `kad/connect` | — | `KadStatus` |
| `kad/disconnect` | — | `KadStatus` |
| `kad/recheck_firewall` | — | `KadStatus` |

#### Search

| Command | Params | Result |
|---|---|---|
| `search/start` | `{ query: string, type?: string, method?: string, min_size?: number, max_size?: number, ext?: string }` | `{ search_id: string }` |
| `search/results` | `{ search_id: string }` | `{ status: 'running'\|'complete', results: SearchResult[] }` |
| `search/stop` | `{ search_id: string }` | `{ ok: true }` |

#### Application / Stats

| Command | Params | Result |
|---|---|---|
| `stats/global` | — | `GlobalStats` |
| `app/version` | — | `{ version: string, build: string }` |
| `app/preferences/get` | — | `Preferences` |
| `app/preferences/set` | `{ prefs: Partial<Preferences> }` | `{ ok: true }` |
| `app/shutdown` | — | `{ ok: true }` |
| `log/get` | `{ limit?: number }` | `LogEntry[]` |

### 4.4 Event Reference

Events are pushed by eMule at any time. The sidecar forwards them to SSE clients.

| Event | Data | Trigger |
|---|---|---|
| `transfer_completed` | `Transfer` | `TM_FILECOMPLETED` (success) |
| `transfer_added` | `Transfer` | File added to queue |
| `transfer_error` | `Transfer` | `TM_FILECOMPLETED` (failure) |
| `transfer_removed` | `{ hash, name }` | File removed from queue |
| `transfer_updated` | `Transfer` | Transfer state/priority/category changes |
| `stats_updated` | `GlobalStats` | Periodic ~1 s |
| `server_connected` | `{ name, addr, port, users, files }` | `CServerConnect::OnConnected` |
| `server_disconnected` | `{}` | Server disconnect |
| `kad_status_changed` | `{ running, connected, firewalled }` | Kad state change |
| `search_results` | `{ search_id, results: SearchResult[] }` | New search results batch |

---

## 5. Data Types

### Transfer (maps from `CPartFile`)

```typescript
interface Transfer {
  hash:           string;      // 32-char hex MD4
  name:           string;
  size:           number;      // bytes
  size_done:      number;      // bytes downloaded
  progress:       number;      // 0.0 – 1.0
  state:          TransferState;
  priority:       Priority;
  dl_speed:       number;      // bytes/s
  ul_speed:       number;      // bytes/s
  sources:        number;      // GetSourceCount()
  sources_xfer:   number;      // GetTransferringSrcCount()
  category:       number;
  added_on:       number;      // unix timestamp
  completion_on:  number | null;
  eta:            number | null; // seconds
  parts_total:    number;      // GetPartCount()
  parts_avail:    number;      // GetAvailablePartCount()
}

type TransferState =
  | 'downloading'   // PS_READY + transferring sources
  | 'stalledDL'     // PS_READY + no transferring sources
  | 'pausedDL'      // PS_PAUSED or m_stopped
  | 'checkingDL'    // PS_HASHING | PS_WAITINGFORHASH
  | 'checkingUP'    // PS_COMPLETING
  | 'uploading'     // PS_COMPLETE + in upload queue
  | 'pausedUP'      // PS_COMPLETE + idle
  | 'error'         // PS_ERROR
  | 'missingFiles'; // PS_INSUFFICIENT

type Priority = 'verylow' | 'low' | 'normal' | 'high' | 'veryhigh' | 'auto';
```

**`EPartFileStatus` → `TransferState` mapping:**

| `EPartFileStatus` | Condition | `TransferState` |
|---|---|---|
| `PS_READY` (0) | `GetTransferringSrcCount() > 0` | `downloading` |
| `PS_READY` (0) | no transferring sources | `stalledDL` |
| `PS_EMPTY` (1) | — | `stalledDL` |
| `PS_WAITINGFORHASH` (2) | — | `checkingDL` |
| `PS_HASHING` (3) | — | `checkingDL` |
| `PS_ERROR` (4) | — | `error` |
| `PS_INSUFFICIENT` (5) | — | `missingFiles` |
| `PS_PAUSED` (7) / `m_stopped` | — | `pausedDL` |
| `PS_COMPLETING` (8) | — | `checkingUP` |
| `PS_COMPLETE` (9) | queued for upload | `uploading` |
| `PS_COMPLETE` (9) | not in upload queue | `pausedUP` |

**`PR_*` → `Priority` mapping:**

| C++ constant | Value | Priority string |
|---|---|---|
| `PR_VERYLOW` | 4 | `verylow` |
| `PR_LOW` | 0 | `low` |
| `PR_NORMAL` | 1 | `normal` |
| `PR_HIGH` | 2 | `high` |
| `PR_VERYHIGH` | 3 | `veryhigh` |
| `PR_AUTO` | 5 | `auto` |

### Source (maps from `CUpDownClient`)

```typescript
interface Source {
  ip:           string;
  port:         number;
  client_name:  string;         // GetUserName()
  software:     string;         // GetClientSoftVer()
  dl_state:     DownloadState;  // EDownloadState
  ul_state:     UploadState;    // EUploadState
  transferred:  number;         // bytes from this source
  low_id:       boolean;        // HasLowID()
}
```

### Server (maps from `CServer`)

```typescript
interface Server {
  addr:        string;
  port:        number;
  name:        string;
  description: string;
  users:       number;
  files:       number;
  max_users:   number;
  ping:        number;   // ms
  failed:      number;   // fail count
  version:     string;
  static:      boolean;
  connected:   boolean;
  features: {
    compression:     boolean;
    unicode:         boolean;
    large_files:     boolean;
    obfuscation_tcp: boolean;
    obfuscation_udp: boolean;
  };
}
```

### KadStatus (maps from `CKademlia` statics)

```typescript
interface KadStatus {
  running:        boolean;
  connected:      boolean;
  firewalled:     boolean;
  lan_mode:       boolean;
  users:          number;
  files:          number;
  indexed_sources: number;
  indexed_keys:   number;
  indexed_notes:  number;
  external_ip:    string;
}
```

### GlobalStats (maps from `CStatistics`)

```typescript
interface GlobalStats {
  dl_speed:        number;   // CStatistics::rateDown  bytes/s
  ul_speed:        number;   // CStatistics::rateUp
  dl_speed_avg:    number;   // GetAvgDownloadRate(0)
  ul_speed_avg:    number;   // GetAvgUploadRate(0)
  dl_session:      number;   // sessionReceivedBytes
  ul_session:      number;   // sessionSentBytes
  dl_speed_peak:   number;   // maxDown
  ul_speed_peak:   number;   // maxUp
  connected_clients: number;
  upload_slots:    number;   // GetActiveUploadsCount()
  queue_size:      number;   // GetWaitingUserCount()
  session_start:   number;   // unix timestamp from starttime
}
```

### Preferences

`app/preferences/get` and `app/preferences/set` expose a curated runtime subset only. Unsupported keys must be rejected rather than ignored.

```typescript
interface Preferences {
  maxUploadKiB: number;
  maxDownloadKiB: number;
  maxConnections: number;
  maxConPerFive: number;
  maxSourcesPerFile: number;
  uploadClientDataRate: number;
  maxUploadSlots: number;
  queueSize: number;
  autoConnect: boolean;
  newAutoUp: boolean;
  newAutoDown: boolean;
  creditSystem: boolean;
  safeServerConnect: boolean;
  networkKademlia: boolean;
  networkEd2k: boolean;
}
```

---

## 6. REST API Endpoints

Base URL: `http://localhost:<port>/api/v2`

Authentication: `Authorization: Bearer <token>` header on all endpoints except `/auth/login`.

### 6.1 Auth

```
POST /api/v2/auth/login
  body: { username: string, password: string }
  200:  { token: string }
  403:  { error: "INVALID_CREDENTIALS" }

POST /api/v2/auth/logout
  200:  { ok: true }
```

Token is a random 256-bit hex string. Stored in an in-memory map in the sidecar. Invalidated on logout or sidecar restart.

### 6.2 Application

```
GET  /api/v2/app/version
  200: { appName, version, build, platform }

GET  /api/v2/app/preferences
  200: Preferences

POST /api/v2/app/preferences
  body: { prefs: Partial<Preferences> }
  200: { ok: true }

POST /api/v2/app/shutdown
  200: { ok: true }
```

### 6.3 Stats

```
GET /api/v2/stats/global
  200: GlobalStats
```

### 6.4 Transfers

```
GET  /api/v2/transfers
  query: filter=<string>
         category=<id>
  200: Transfer[]

GET  /api/v2/transfers/:hash
  200: Transfer

GET  /api/v2/transfers/:hash/sources
  200: Source[]

POST /api/v2/transfers/add
  body: { links: string[] }
  200: { results: MutationResult[] }

POST /api/v2/transfers/pause
  body: { hashes: string[] }
  200: { results: MutationResult[] }

POST /api/v2/transfers/resume
  body: { hashes: string[] }
  200: { results: MutationResult[] }

POST /api/v2/transfers/stop
  body: { hashes: string[] }
  200: { results: MutationResult[] }

POST /api/v2/transfers/delete
  body: { hashes: string[], deleteFiles?: bool }
  200: { results: MutationResult[] }

POST /api/v2/transfers/:hash/priority
  body: { priority: string }
  200: { ok: true }

POST /api/v2/transfers/:hash/category
  body: { category: number }
  200: { ok: true }

POST /api/v2/transfers/:hash/recheck
  200: { ok: true }
```

### 6.5 Uploads, Servers, Kad, Shared

```
GET  /api/v2/uploads/list
  200: Upload[]

GET  /api/v2/uploads/queue
  200: QueueEntry[]

POST /api/v2/uploads/remove
  body: { userHash?: string, ip?: string, port?: number }
  200: { ok: true, removed: 'queue' | 'slot' }

POST /api/v2/uploads/release_slot
  body: { userHash?: string, ip?: string, port?: number }
  200: { ok: true }

GET  /api/v2/servers/list
  200: Server[]

GET  /api/v2/servers/status
  200: ServerStatus

POST /api/v2/servers/connect
  body: { addr?: string, port?: number }
  200: ServerStatus

POST /api/v2/servers/disconnect
  200: ServerStatus

POST /api/v2/servers/add
  body: { addr: string, port: number, name?: string }
  200: Server

POST /api/v2/servers/remove
  body: { addr: string, port: number }
  200: Server

GET  /api/v2/kad/status
  200: KadStatus

POST /api/v2/kad/connect
  200: KadStatus

POST /api/v2/kad/disconnect
  200: KadStatus

POST /api/v2/kad/recheck_firewall
  200: KadStatus

GET  /api/v2/shared/list
  200: SharedFile[]

GET  /api/v2/shared/:hash
  200: SharedFile

POST /api/v2/shared/add
  body: { path: string }
  200: { ok: true, path: string, alreadyShared: boolean, queued: boolean, file: SharedFile | null }

POST /api/v2/shared/remove
  body: { hash?: string, path?: string }
  200: { ok: true, path: string, hash: string | null }
```

### 6.6 Log

```
GET /api/v2/log
  query: limit=<number>
  200: LogEntry[]

interface LogEntry {
  message:   string;
  timestamp: number;
  level:     'info' | 'warning' | 'error' | 'success';
  debug:     boolean;
}
```

### 6.7 Search

```
POST /api/v2/search/start
  body: {
    query: string,
    method?: string,
    type?: string,
    min_size?: number,
    max_size?: number,
    ext?: string
  }
  200: { search_id: string }

GET /api/v2/search/results
  query: search_id=<string>
  200: {
    status: 'running' | 'complete',
    results: SearchResult[]
  }

POST /api/v2/search/stop
  body: { search_id: string }
  200: { ok: true }

interface SearchResult {
  hash:            string;
  name:            string;
  size:            number;
  fileType:        string;
  sources:         number;
  completeSources: number;
  complete:        boolean | null;
  knownType:       string;
}
```

### 6.8 Events (SSE)

```
GET /api/v2/events
  Accept: text/event-stream
  → persistent SSE stream
```

Each SSE message carries a single pipe event as JSON in the `data` field:

```
event: transfer_completed
data: {"hash":"a1b2c3...","name":"example.mkv","size":1073741824}

event: stats_updated
data: {"downloadSpeed":102400,"uploadSpeed":51200,"sessionDownloaded":536870912,...}

event: server_connected
data: {"name":"eMule Security","addr":"1.2.3.4","port":4661,"users":150000,"files":8000000}
```

### 6.9 eMule Extensions

The grouped `/api/v2/uploads/*`, `/api/v2/servers/*`, `/api/v2/kad/*`, and `/api/v2/shared/*` routes above are the eMule-specific HTTP surface. There is no separate `/api/v2/emule/*` namespace in the current implementation.

---

## 7. TypeScript Project Structure

```
emule-sidecar/
├── src/
│   ├── pipe/
│   │   ├── PipeClient.ts       named pipe connection, framing, reconnect loop
│   │   └── protocol.ts         discriminated union types for all pipe messages
│   │
│   ├── events/
│   │   ├── EventBus.ts         typed EventEmitter between PipeClient and SSE
│   │   └── SseManager.ts       fan-out: one EventBus → N SSE response streams
│   │
│   ├── api/
│   │   ├── server.ts           Fastify instance, plugin registration, CORS, error handler
│   │   ├── auth.ts             token map, login/logout, Bearer middleware
│   │   └── routes/
│   │       ├── app.ts          /api/v2/app/*
│   │       ├── stats.ts        /api/v2/stats/*
│   │       ├── transfers.ts    /api/v2/transfers*
│   │       ├── uploads.ts      /api/v2/uploads/*
│   │       ├── servers.ts      /api/v2/servers/*
│   │       ├── kad.ts          /api/v2/kad/*
│   │       ├── shared.ts       /api/v2/shared/*
│   │       ├── log.ts          /api/v2/log*
│   │       ├── search.ts       /api/v2/search/*
│   │       ├── events.ts       /api/v2/events  (SSE)
│   │
│   ├── types/
│   │   └── emule.ts            shared HTTP and pipe-facing types
│   │
│   └── index.ts                wire PipeClient + Fastify + EventBus, read .env, start
│
├── package.json
├── tsconfig.json
└── .env.example
```

### Key dependencies

```json
{
  "dependencies": {
    "fastify": "^5",
    "@fastify/cors": "^10",
    "zod": "^3"
  },
  "devDependencies": {
    "typescript": "^5",
    "tsx": "^4",
    "@types/node": "^22"
  }
}
```

No bundling. Run with `tsx src/index.ts` in development, `node --import tsx/esm src/index.ts` in production (or compile to JS with `tsc`).

### `.env.example`

```
PORT=8080
PIPE_NAME=\\.\pipe\emule-api
AUTH_SECRET=change-me-to-random-256-bit-hex
RECONNECT_INTERVAL_MS=3000
```

### `PipeClient.ts` sketch

```typescript
import { createConnection, Socket } from 'net';
import { randomUUID } from 'crypto';
import { EventEmitter } from 'events';
import type { PipeRequest, PipeResponse, PipeEvent } from './protocol';

export class PipeClient extends EventEmitter {
  private socket: Socket | null = null;
  private pending = new Map<string, { resolve: (r: unknown) => void; reject: (e: Error) => void }>();
  private buffer = '';
  private reconnectTimer: NodeJS.Timeout | null = null;

  constructor(
    private readonly pipeName: string,
    private readonly reconnectMs: number = 3000,
  ) { super(); }

  connect(): void {
    this.socket = createConnection(this.pipeName);
    this.socket.setEncoding('utf8');
    this.socket.on('data', chunk => this.onData(chunk));
    this.socket.on('error', () => { /* swallow, close will fire */ });
    this.socket.on('close', () => this.scheduleReconnect());
    this.socket.on('connect', () => this.emit('connected'));
  }

  private onData(chunk: string): void {
    this.buffer += chunk;
    const lines = this.buffer.split('\n');
    this.buffer = lines.pop()!;
    for (const line of lines) {
      if (!line.trim()) continue;
      try {
        const msg = JSON.parse(line) as PipeResponse | PipeEvent;
        if ('event' in msg) {
          this.emit('emule_event', msg);
        } else if ('id' in msg) {
          const p = this.pending.get(msg.id);
          if (p) {
            this.pending.delete(msg.id);
            'error' in msg ? p.reject(new Error(msg.error)) : p.resolve(msg.result);
          }
        }
      } catch { /* malformed line — ignore */ }
    }
  }

  request<T>(cmd: string, params?: Record<string, unknown>): Promise<T> {
    return new Promise((resolve, reject) => {
      if (!this.socket?.writable) return reject(new Error('pipe not connected'));
      const id = randomUUID();
      this.pending.set(id, { resolve: resolve as (r: unknown) => void, reject });
      const msg: PipeRequest = { id, cmd, params: params ?? {} };
      this.socket.write(JSON.stringify(msg) + '\n');
    });
  }

  private scheduleReconnect(): void {
    this.emit('disconnected');
    if (this.reconnectTimer) return;
    this.reconnectTimer = setTimeout(() => {
      this.reconnectTimer = null;
      this.connect();
    }, this.reconnectMs);
  }

  disconnect(): void {
    if (this.reconnectTimer) clearTimeout(this.reconnectTimer);
    this.socket?.destroy();
  }
}
```

### `SseManager.ts` sketch

```typescript
import type { FastifyReply } from 'fastify';
import type { EventBus } from './EventBus';

export class SseManager {
  private clients = new Set<FastifyReply>();

  constructor(bus: EventBus) {
    bus.on('emule_event', (event: { event: string; data: unknown }) => {
      this.broadcast(event.event, event.data);
    });
  }

  addClient(reply: FastifyReply): void {
    reply.raw.on('close', () => this.clients.delete(reply));
    this.clients.add(reply);
  }

  private broadcast(eventName: string, data: unknown): void {
    const payload = `event: ${eventName}\ndata: ${JSON.stringify(data)}\n\n`;
    for (const client of this.clients) {
      client.raw.write(payload);
    }
  }
}
```

---

## 8. C++ Side — `CPipeApiServer`

### 8.1 Class Design

```cpp
// PipeApiServer.h
class CPipeApiServer {
public:
    void Start();
    void Stop();

    // Called from any eMule thread to push an event to the sidecar
    void PostEvent(const char* eventType, const nlohmann::json& data);

private:
    HANDLE           m_hPipe      = INVALID_HANDLE_VALUE;
    HANDLE           m_hThread    = NULL;
    HANDLE           m_hStopEvent = NULL;
    CRITICAL_SECTION m_csWrite;

    static DWORD WINAPI ThreadProc(LPVOID param);
    void RunLoop();

    // Framing
    void SendLine(const std::string& json);
    void SendResponse(const std::string& id, const nlohmann::json& result);
    void SendError(const std::string& id, const std::string& message);

    // Dispatch
    void HandleRequest(const nlohmann::json& req);

    // Command handlers — one per cmd string
    nlohmann::json OnTransfersList(const nlohmann::json& params);
    nlohmann::json OnTransfersGet(const nlohmann::json& params);
    nlohmann::json OnTransfersPause(const nlohmann::json& params);
    nlohmann::json OnTransfersResume(const nlohmann::json& params);
    nlohmann::json OnTransfersStop(const nlohmann::json& params);
    nlohmann::json OnTransfersDelete(const nlohmann::json& params);
    nlohmann::json OnTransfersAdd(const nlohmann::json& params);
    nlohmann::json OnTransfersSetPriority(const nlohmann::json& params);
    nlohmann::json OnTransfersSetCategory(const nlohmann::json& params);
    nlohmann::json OnTransfersSources(const nlohmann::json& params);
    nlohmann::json OnUploadsList(const nlohmann::json& params);
    nlohmann::json OnUploadsQueue(const nlohmann::json& params);
    nlohmann::json OnSharedList(const nlohmann::json& params);
    nlohmann::json OnServersList(const nlohmann::json& params);
    nlohmann::json OnServersConnect(const nlohmann::json& params);
    nlohmann::json OnServersDisconnect(const nlohmann::json& params);
    nlohmann::json OnServersAdd(const nlohmann::json& params);
    nlohmann::json OnServersRemove(const nlohmann::json& params);
    nlohmann::json OnKadStatus(const nlohmann::json& params);
    nlohmann::json OnKadConnect(const nlohmann::json& params);
    nlohmann::json OnKadDisconnect(const nlohmann::json& params);
    nlohmann::json OnKadRecheckFirewall(const nlohmann::json& params);
    nlohmann::json OnStatsGlobal(const nlohmann::json& params);
    nlohmann::json OnAppVersion(const nlohmann::json& params);
    nlohmann::json OnAppPreferencesGet(const nlohmann::json& params);
    nlohmann::json OnAppPreferencesSet(const nlohmann::json& params);
    nlohmann::json OnLogGet(const nlohmann::json& params);
    nlohmann::json OnSearchStart(const nlohmann::json& params);
    nlohmann::json OnSearchResults(const nlohmann::json& params);
    nlohmann::json OnSearchStop(const nlohmann::json& params);

    // Serialization helpers
    static nlohmann::json SerializePartFile(const CPartFile* pFile);
    static nlohmann::json SerializeClient(const CUpDownClient* pClient);
    static nlohmann::json SerializeServer(const CServer* pServer);
    static std::string HashToHex(const uchar* hash);   // 16 bytes → 32 hex chars
};

extern CPipeApiServer thePipeApiServer;
```

### 8.2 Pipe Lifecycle

```cpp
void CPipeApiServer::Start() {
    InitializeCriticalSection(&m_csWrite);
    m_hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    m_hPipe = CreateNamedPipe(
        L"\\\\.\\pipe\\emule-api",
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1,       // max instances — one sidecar at a time
        65536,   // output buffer
        65536,   // input buffer
        0,       // default timeout
        NULL     // default security (local access only)
    );
    m_hThread = CreateThread(NULL, 0, ThreadProc, this, 0, NULL);
}

void CPipeApiServer::RunLoop() {
    while (true) {
        // Wait for sidecar to connect (with stop check)
        HANDLE events[2] = { m_hStopEvent, /* overlapped event */ };
        ConnectNamedPipe(m_hPipe, &overlapped);
        if (WaitForMultipleObjects(2, events, FALSE, INFINITE) == WAIT_OBJECT_0)
            break; // stop requested

        // Read JSON-lines, dispatch, write responses
        // ... ReadFile loop accumulating until \n ...

        DisconnectNamedPipe(m_hPipe);
    }
    CloseHandle(m_hPipe);
}

void CPipeApiServer::PostEvent(const char* eventType, const nlohmann::json& data) {
    nlohmann::json msg = { {"event", eventType}, {"data", data} };
    SendLine(msg.dump());
}

void CPipeApiServer::SendLine(const std::string& json) {
    EnterCriticalSection(&m_csWrite);
    std::string line = json + "\n";
    DWORD written;
    WriteFile(m_hPipe, line.c_str(), (DWORD)line.size(), &written, NULL);
    LeaveCriticalSection(&m_csWrite);
}
```

### 8.3 Event Hook Points in eMule

```cpp
// EmuleDlg.cpp — OnFileCompleted handler
LRESULT CemuleDlg::OnFileCompleted(WPARAM wParam, LPARAM lParam) {
    CPartFile* pFile = reinterpret_cast<CPartFile*>(lParam);
    if (wParam & FILE_COMPLETION_THREAD_SUCCESS) {
        thePipeApiServer.PostEvent("transfer_completed", {
            {"hash", CPipeApiServer::HashToHex(pFile->GetFileHash())},
            {"name", CStringToUtf8(pFile->GetFileName())},
            {"size", pFile->GetFileSize()}
        });
    } else {
        thePipeApiServer.PostEvent("transfer_error", {
            {"hash", CPipeApiServer::HashToHex(pFile->GetFileHash())},
            {"name", CStringToUtf8(pFile->GetFileName())}
        });
    }
    // ... existing handling ...
}

// CServerConnect.cpp — on successful server connection
thePipeApiServer.PostEvent("server_connected", {
    {"name",  CStringToUtf8(pServer->GetListName())},
    {"addr",  inet_ntoa(...)},
    {"port",  pServer->GetPort()},
    {"users", pServer->GetUsers()},
    {"files", pServer->GetFiles()}
});

// Existing stats timer (e.g. CemuleDlg::OnTimer or CStatistics::RecordRate)
thePipeApiServer.PostEvent("stats_updated", {
    {"downloadSpeed", (int)theStats.rateDown},
    {"uploadSpeed", (int)theStats.rateUp},
    {"sessionDownloaded", theStats.sessionReceivedBytes},
    {"sessionUploaded", theStats.sessionSentBytes},
    {"activeUploads",  theApp.uploadqueue->GetActiveUploadsCount()},
    {"waitingUploads", theApp.uploadqueue->GetWaitingUserCount()}
});
```

### 8.4 Thread Safety Model

Command handlers run on the pipe read thread. They must not call MFC UI functions directly. The same pattern used in the existing `WebServer.cpp` applies:

- **Read-only data access** (`GetFileCount()`, iterating `filelist`, etc.): wrap with `CSingleLock` on the appropriate critical section, same as the web server does.
- **Mutating operations** (pause, resume, add link, connect server): use `PostMessage` / `SendMessage` to the main UI thread, same as `WebServer.cpp`'s `WEB_GUI_INTERACTION` pattern. The handler waits for the message to be processed before sending the response.
- **`PostEvent` calls**: protected by `m_csWrite`, safe from any thread.

### 8.5 JSON Library

Use **nlohmann/json** (single header, `nlohmann/json.hpp`). No build system changes beyond dropping the header into the source tree. Already the de-facto standard for C++ projects of this size.

---

## 9. HTTP Error Convention

All errors return a JSON body with a consistent shape:

```json
{ "error": "ERROR_CODE", "message": "human-readable description" }
```

| Scenario | HTTP Status | `error` code |
|---|---|---|
| Missing/invalid Bearer token | 401 | `UNAUTHORIZED` |
| Wrong credentials at login | 403 | `INVALID_CREDENTIALS` |
| Hash not found | 404 | `HASH_NOT_FOUND` |
| Invalid ed2k link | 400 | `INVALID_LINK` |
| Pipe not connected | 503 | `EMULE_UNAVAILABLE` |
| Pipe command timeout | 504 | `EMULE_TIMEOUT` |
| eMule returned an error | 500 | `EMULE_ERROR` |

Requests that target multiple hashes (pause, resume, delete) proceed best-effort and return `{ ok: true }` — callers should not assume all hashes succeeded.

---

## 10. Implementation Order

### Phase 1 — C++ pipe server scaffold
- `CPipeApiServer` class: `Start()`, `Stop()`, `RunLoop()`, `PostEvent()`
- JSON-lines framing (read + write)
- Reconnect handling on the eMule side (wait for next client after disconnect)
- Integrate `nlohmann/json`
- Wire `Start()`/`Stop()` alongside existing `CWebServer::StartServer()`/`StopServer()`

### Phase 2 — Read-only commands
- `transfers/list`, `transfers/get`, `transfers/sources`
- `uploads/list`, `uploads/queue`
- `shared/list`
- `servers/list`
- `kad/status`
- `stats/global`
- `app/version`
- `log/get`

### Phase 3 — Events
- `transfer_completed` / `transfer_error` from `TM_FILECOMPLETED`
- `server_connected` / `server_disconnected`
- `stats_updated` from stats timer
- `kad_status_changed`

### Phase 4 — Mutating commands
- `transfers/pause`, `transfers/resume`, `transfers/stop`, `transfers/delete`
- `transfers/add` (ed2k link)
- `transfers/set_priority`, `transfers/set_category`
- `servers/connect`, `servers/disconnect`, `servers/add`, `servers/remove`
- `kad/connect`, `kad/disconnect`, `kad/recheck_firewall`
- `app/preferences/set`, `app/shutdown`

### Phase 5 — Search
- `search/start`, `search/results`, `search/stop`
- `search_results` event push

### Phase 6 — Node.js sidecar
- Project scaffold: Fastify + TypeScript + Zod
- `PipeClient.ts` with reconnect
- `EventBus.ts` + `SseManager.ts`
- All grouped `/api/v2/app|stats|transfers|uploads|servers|kad|shared|log|search` routes
- SSE endpoint (`/api/v2/events`)
- Mutating routes

---

## 11. Out of Scope

- Replacing the existing HTML web server — it stays untouched alongside this
- Bundling the sidecar into eMule.exe
- WebSocket (SSE covers the push use case adequately)
- Pagination (download queues rarely exceed a few hundred items)
- Rate limiting (local-only deployment assumption)

---

## Feature Identifier

### PLAN_004: Named Pipe + Node.js Sidecar API

This document describes the plan for a modern HTTP API server architecture using a named pipe IPC channel between the eMule core process and a Node.js sidecar process that exposes a REST/WebSocket API.

**Status:** Planning phase. No implementation has been started.
