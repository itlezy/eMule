# Modern Limits Plan

## Table of Contents

- [Feature Tracking](#feature-tracking)
- [Purpose](#purpose)
- [Design Direction](#design-direction)
- [Scope Boundary](#scope-boundary)
- [Current Problem Areas](#current-problem-areas)
- [Recommended Fixed Defaults](#recommended-fixed-defaults)
- [Settings To Expose In The Advanced Tree](#settings-to-expose-in-the-advanced-tree-feat_019)
- [Implementation Guidance](#implementation-guidance)
- [Detailed Target Changes](#detailed-target-changes)
- [Risk Ranking](#risk-ranking)
- [Validation Plan](#validation-plan)
- [Preferred Execution Order](#preferred-execution-order)

## Feature Tracking

| ID | Feature | Status |
|----|---------|--------|
| FEAT_013 | Connection budget defaults | **[DONE]** — MaxConnections remains 500 by branch choice; MaxHalfConnections=50; MaxConPerFive=50 |
| FEAT_014 | Per-client upload cap | **[DONE]** — default cap is 8 MB/s and now persists through Preferences |
| FEAT_015 | Socket buffer sizes | **[DONE]** — UDP recv buffer=512 KiB; TCP big send buffer=512 KiB |
| FEAT_016 | Disk buffering defaults | **[DONE]** — FileBufferSize=64 MiB, slider max=512 MiB, FileBufferTimeLimit=120s |
| FEAT_017 | Queue/source limits | **[DONE]** — QueueSize=10000; MaxSourcesPerFile=600; soft/UDP caps=1000/100 |
| FEAT_018 | Timeout adjustments | **[DONE]** — ConnectionTimeout=30s, DownloadTimeout=75s, UDP queue expiry=20s, ConnectionLatency=15000 |
| FEAT_019 | Advanced tree UI exposure | **[DONE]** — Tweaks exposes the remaining active timeout and upload-cap knobs without adding a new subtree |

## Purpose

This document defines a fixed-value modernization pass for old eMule-era limits, defaults, and hard-coded resource assumptions in the current `v0.72a-broadband-dev` branch.

The intent is:

- keep protocol compatibility
- avoid adaptive behavior where possible
- prefer explicit fixed defaults and user-configurable limits
- raise limits to match modern bandwidth, RAM, CPU, and disk hardware
- preserve predictability for users who want to understand exactly what the software is doing

This is explicitly **not** an opcode redesign, protocol fork, or Kad wire-format change.

## Design Direction

The preferred direction for this branch is:

- increase stale hard-coded defaults
- expose important fixed limits in the Advanced tree where practical
- keep runtime behavior deterministic
- avoid hidden auto-tuning logic unless there is no practical fixed alternative

When in doubt:

- prefer a larger fixed default over an adaptive policy
- prefer a user-visible advanced preference over a compile-time-only magic number
- prefer compatibility-safe cleanup over deep algorithmic changes

## Scope Boundary

### Safe To Change

- hard-coded default limits
- socket buffer sizes
- queue sizes
- source-count caps
- file-buffer sizes and flush timing defaults
- connection and download timeout defaults
- fixed per-slot / per-client throughput ceilings
- deprecated internal comments and obsolete OS-era assumptions
- preference exposure for fixed limits

### Do Not Change In This Pass

- protocol header values
- opcode numbers
- `PARTSIZE`
- `EMBLOCKSIZE`
- Kad packet format
- eD2k packet format
- `UDP_KAD_MAXFRAGMENT`
- on-wire feature negotiation semantics unless the change is strictly internal and backward-compatible

## Non-Goals

- no adaptive bandwidth controller redesign
- no dynamic timeout estimator
- no automatic hardware benchmarking
- no per-machine automatic scaling tables
- no protocol incompatibility
- no IPv6 redesign in this document

## Current Problem Areas

The code still carries several 20-year-old fixed assumptions which are too conservative for modern systems.

### 1. Connection Budget Defaults Are Too Low (FEAT_013)

Current code:

- recommended max connections stays at `500` by branch choice
- `MaxHalfConnections` now defaults to `50`
- `MaxConnectionsPerFiveSeconds` now defaults to `50`

The remaining design choice here is deliberate conservatism on total connections, not an unfinished legacy fallback.

### 2. Per-Client Upload Ceiling Is Too Low (FEAT_014)

Current code:

- `UPLOAD_CLIENT_MAXDATARATE` is fixed at `1 * 1024 * 1024` bytes/s in [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h#L109)
- this still caps the fallback per-slot target path in [`UploadQueue.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/UploadQueue.cpp#L518)

For a broadband branch, `1 MB/s` per client is an obsolete ceiling.

### 3. Socket Buffers Are Undersized (FEAT_015)

Current code:

- UDP receive buffer defaults to `512 KiB`
- TCP "big send buffer" defaults to `512 KiB`

This feature area is complete for the current fixed-value modernization target.

### 4. Disk Buffering Defaults Are Conservative (FEAT_016)

Current code:

- file buffer size defaults to `64 MiB`
- file buffer time limit defaults to `120s`
- the Tweaks slider now reaches `512 MiB`
- part files still flush on the same size/time triggers in `PartFile.cpp`

This area now favors modern storage throughput while keeping the existing flush policy.

### 5. Queue And Source Limits Are Still Small (FEAT_017)

Current code:

- default `MaxSourcesPerFile` is `600`
- source soft/UDP caps are `1000` and `100`
- queue default is `10000`

This feature area is complete for the current branch target.

### 6. Several Timeouts Are Long And Old (FEAT_018)

Current code in [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h):

- `CONNECTION_TIMEOUT = 40s`
- `DOWNLOADTIMEOUT = 100s`
- `KADEMLIAASKTIME = 1s`
- `UDPMAXQUEUETIME = 30s`
- `CONNECTION_LATENCY = 22050`

These defaults now use the shorter broadband values while staying fixed-value based and non-adaptive.

### 7. Deprecated Capability Baggage Still Exists

Current code:

- deprecated source-exchange and misc capability notes are still visible in [`BaseClient.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/BaseClient.cpp#L972)
- the code still advertises older capability structures for compatibility

That does not mean the wire values should be changed, but internal compatibility baggage should be reviewed and documented before further network modernization.

## Recommended Fixed Defaults

These are proposed **branch defaults**, not hard requirements for every user.

### Connection And Socket Defaults (FEAT_013, FEAT_014, FEAT_015)

| Setting | Current | Proposed |
| --- | --- | --- |
| `MaxConnections` default recommendation | `500`-style cap | `500` |
| `MaxHalfConnections` default | `9` | `50` |
| `MaxConnectionsPerFiveSeconds` default | `20` | `50` |
| per-client upload cap | `1 MB/s` | `8 MB/s` |
| UDP receive socket buffer | `64 KiB` | `512 KiB` |
| TCP big send buffer | `128 KiB` | `512 KiB` |

Notes:

- `500` remains the branch default by explicit choice while the burst and half-open budgets use the broader modern values
- `50` half-open keeps the value modern without turning connection bursts into a free-for-all
- `8 MB/s` per-client cap is large enough not to cripple modern uplinks while still remaining bounded

### File, Queue, And Source Defaults (FEAT_016, FEAT_017)

| Setting | Current | Proposed |
| --- | --- | --- |
| `FileBufferSize` | `512 KiB` | `64 MiB` |
| `FileBufferTimeLimit` | `60s` | `120s` |
| `QueueSize` | ~`5000` effective path | `10000` |
| `MaxSourcesPerFile` | `400` | `600` |
| `MAX_SOURCES_FILE_SOFT` | `750` | `1000` |
| `MAX_SOURCES_FILE_UDP` | `50` | `100` |

Notes:

- `64 MiB` is intentionally SSD/NVMe-friendly for this broadband branch, while the Tweaks slider now tops out at `512 MiB`
- `120s` is long enough to reduce churn but still bounded
- `600` sources/file is a moderate modernization, not an extreme one

### Timeout Defaults (FEAT_018)

| Setting | Current | Proposed |
| --- | --- | --- |
| `CONNECTION_TIMEOUT` | `40s` | `30s` |
| `DOWNLOADTIMEOUT` | `100s` | `75s` |
| `UDPMAXQUEUETIME` | `30s` | `20s` |
| `CONNECTION_LATENCY` | `22050` | `15000` |

Notes:

- these changes should be validated carefully because timeouts are easy to over-tighten
- the preferred style is still fixed values, not RTT adaptation

## Settings To Expose In The Advanced Tree (FEAT_019)

These are now visible in `Preferences > Tweaks` where they were missing, while already-exposed controls stay in their existing pages/groups.

### Definitely Expose

- max connections per five seconds
- max half-open connections
- per-client upload cap
- UDP receive buffer size
- TCP send buffer size
- queue size
- file buffer size
- file buffer time limit
- connection timeout
- download timeout
- max sources per file

### Exposure Style

- keep them in `Preferences > Tweaks`
- reuse existing relevant groups instead of adding a new subtree
- use numeric edits for exact control
- add short comments/tooltips in code and labels where the purpose is not obvious

## Implementation Guidance

### Phase 1: Raise The Defaults Only (FEAT_013, FEAT_014, FEAT_015, FEAT_016, FEAT_017, FEAT_018)

Do first:

- change the fixed defaults
- keep user override semantics intact
- do not change UI flow yet unless a value is already exposed there
- do not add automatic scaling

Files likely involved:

- [`srchybrid/Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h)
- [`srchybrid/Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp)
- [`srchybrid/PPgTweaks.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/PPgTweaks.cpp)

### Phase 2: Replace Compile-Time Magic With Preferences Where Practical (FEAT_019)

Convert selected compile-time constants into persisted advanced preferences where this can be done safely without deep refactoring.

Implemented candidates:

- per-client upload cap
- UDP socket receive buffer size
- TCP big send buffer size
- connection timeout
- download timeout

Keep compatibility-safe boundaries:

- leave wire-format-related constants in place
- only move internal local resource controls to preferences

### Phase 3: Review Deprecated Capability Luggage

Not to change opcodes, but to clean up stale assumptions:

- review old comments and compatibility branches in [`BaseClient.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/BaseClient.cpp)
- document which compatibility flags are still required
- remove wrappers and dead compatibility scaffolding if it no longer materially helps

This phase is optional relative to the pure limits work, but should be considered before any broader network redesign.

## Detailed Target Changes

### 1. `MaxConnections` (FEAT_013)

Current:

- recommended by [`CPreferences::GetRecommendedMaxConnections()`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L1631)

Action:

- keep the recommendation path at `500` as the fixed branch default
- keep existing user-specified values unchanged
- treat the higher half-open and burst budgets as the main connection-budget modernization

Risk:

- low to moderate

### 2. `MaxHalfConnections` (FEAT_013)

Current:

- default `9` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2142)
- old XP SP2 compatibility logic still resets between `9` and `50`

Action:

- change the default to `50`
- remove or simplify the stale OS-era fallback if it no longer serves a supported platform

Risk:

- low

### 3. `MaxConnectionsPerFiveSeconds` (FEAT_013)

Current:

- compile-time fallback `20` in [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h#L106)
- Tweaks fallback also `20` in [`PPgTweaks.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/PPgTweaks.cpp#L41)

Action:

- raise the default/fallback to `50`
- ensure both the compile-time fallback and UI fallback agree

Risk:

- low to moderate

### 4. Per-Client Upload Cap (FEAT_014)

Current:

- `UPLOAD_CLIENT_MAXDATARATE` is `1 MB/s` in [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h#L109)

Action:

- raise to `8 MB/s`
- ideally replace this constant with a preference-backed advanced limit later

Risk:

- moderate

Reason:

- this changes slot throughput distribution and may alter fairness characteristics

### 5. UDP Receive Buffer (FEAT_015)

Current:

- `64 KiB` in [`ClientUDPSocket.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/ClientUDPSocket.cpp#L533)

Action:

- raise to `512 KiB`
- optionally make this configurable later

Risk:

- low

### 6. TCP Big Send Buffer (FEAT_015)

Current:

- `128 KiB` in [`EMSocket.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/EMSocket.cpp#L1092)

Action:

- raise to `512 KiB`
- optionally make this configurable later

Risk:

- low

### 7. File Buffer Size (FEAT_016)

Current:

- `512 KiB` default in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2388)

Action:

- change default to `64 MiB`
- extend the Tweaks slider max to `512 MiB`
- preserve existing user-configured values unless the user reopens Tweaks and accepts the new clamped slider range

Risk:

- low

### 8. File Buffer Time Limit (FEAT_016)

Current:

- `60s` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2393)

Action:

- change default to `120s`

Risk:

- low to moderate

### 9. Queue Size (FEAT_017)

Current:

- old compatibility path still maps to an effective `5000`-style default in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2396)

Action:

- change effective default to `10000`
- keep queue behavior otherwise unchanged

Risk:

- low to moderate

### 10. Source Limits (FEAT_017)

Current:

- default `MaxSourcesPerFile = 600` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2187)
- soft/UDP caps in [`PartFile.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/PartFile.cpp#L5362) and [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h#L96)

Action:

- keep default `MaxSourcesPerFile` at `600`
- raise soft cap to `1000`
- raise UDP cap to `100`

Risk:

- moderate

Reason:

- source tracking affects RAM, CPU, and queue churn

### 11. Timeout Values (FEAT_018)

Current:

- `CONNECTION_TIMEOUT = 40s`
- `DOWNLOADTIMEOUT = 100s`
- `UDPMAXQUEUETIME = 30s`
- `CONNECTION_LATENCY = 22050`

Action:

- reduce to:
  - `30s`
  - `75s`
  - `20s`
  - `12000` or `15000`

Risk:

- moderate to high

Reason:

- timeout regressions are easy to feel immediately

## Risk Ranking

### Low Risk

- UDP receive buffer increase
- TCP send buffer increase
- file buffer size increase
- max half-open default increase

### Moderate Risk

- max connections increase
- burst limit increase
- queue size increase
- source limit increase
- per-client upload cap increase
- file buffer time limit increase

### Higher Risk

- timeout reductions
- cleanup of deprecated capability behavior if it touches actual negotiation paths

## Validation Plan

### Build Verification

- build with `..\\23-build-emule-debug-incremental.cmd`

### Runtime Verification

- verify the app still boots and binds sockets correctly
- verify server connection still succeeds
- verify Kad still starts and performs lookups
- verify high-source downloads do not exhibit obvious churn regressions
- verify upload throughput is not artificially capped at old values
- verify queue behavior remains stable with larger queue/source defaults
- verify no obvious disk thrash regressions after increasing file buffers

### Specific Checks

- monitor open socket count against raised connection defaults
- monitor upload slot throughput after raising the per-client cap
- monitor UDP packet drop/log noise after raising the UDP socket receive buffer
- monitor part-file flush cadence and disk activity after raising file buffer size/time

## Preferred Execution Order

1. Raise connection defaults.
2. Raise socket buffers.
3. Raise file buffer and queue defaults.
4. Raise source limits.
5. Adjust timeout values.
6. Expose fixed modern limits in the Advanced tree.
7. Review stale compatibility comments/branches.

## Explicit Instructions For Later Implementation

When executing this plan later, use the following rules:

- do not change opcode numbers
- do not change protocol headers
- do not change `PARTSIZE`, `EMBLOCKSIZE`, or `UDP_KAD_MAXFRAGMENT`
- prefer fixed values over adaptive behavior
- preserve user overrides from existing configs
- comment any newly introduced limit or default clearly so it is easy to identify in future review
- avoid wrappers and compatibility mapping layers when refactoring
- keep the implementation split into reviewable chunks
- update `RESUME.md` before and after each chunk
- build with `..\\23-build-emule-debug-incremental.cmd`

## Copy-Paste Prompt For Future Execution

Use this exact prompt later when you want the implementation started:

```text
Implement docs\FEATURE-MODERN-LIMITS.md in phased chunks.

Rules:
- do not change opcode numbers, protocol headers, PARTSIZE, EMBLOCKSIZE, or UDP_KAD_MAXFRAGMENT
- prefer fixed values over adaptive behavior
- preserve existing user overrides
- keep comments clear around any new or changed hard-coded limit
- avoid wrappers and compatibility mappings
- update RESUME.md with the exact last and next chunk
- build with ..\23-build-emule-debug-incremental.cmd after each chunk

Execution order:
1. raise connection defaults
2. raise socket buffer sizes
3. raise file buffer and queue defaults
4. raise source limits
5. adjust timeout defaults
6. expose the selected fixed limits in the Advanced tree

For each chunk:
- implement the code changes
- summarize the exact defaults changed
- report build status
- call out any runtime testing still needed
```

## Optional Stronger Copy-Paste Prompt

Use this if the intent is to do the first implementation chunk immediately:

```text
Start Phase 1 of docs\FEATURE-MODERN-LIMITS.md now.

Implement only:
- MaxConnections default recommendation -> 1000
- MaxHalfConnections default -> 50
- MaxConnectionsPerFiveSeconds default/fallback -> 50

Do not change anything else in this chunk.

Requirements:
- preserve existing user overrides
- keep comments clear
- update RESUME.md
- build with ..\23-build-emule-debug-incremental.cmd
- report exact file changes and exact default values changed
```
