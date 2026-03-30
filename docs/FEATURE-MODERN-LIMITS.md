# Modern Limits Plan

## Feature Tracking

| ID | Feature | Status |
|----|---------|--------|
| FEAT_013 | Connection budget defaults | **[PARTIAL]** — MaxHalfConnections=50, MaxConPerFive=50; MaxConnections still 500 (planned 1000) |
| FEAT_014 | Per-client upload cap | **[DONE]** — raised to 8 MB/s |
| FEAT_015 | Socket buffer sizes | **[PARTIAL]** — UDP recv buffer=512 KiB [DONE]; TCP big send buffer status TBD |
| FEAT_016 | Disk buffering defaults | **[DONE]** — FileBufferSize=2 MiB |
| FEAT_017 | Queue/source limits | **[PARTIAL]** — QueueSize still 5000 (planned 10000); MaxSourcesPerFile still 400 (planned 600) |
| FEAT_018 | Timeout adjustments | Not started — CONNECTION_TIMEOUT=40s, DOWNLOADTIMEOUT=100s, UDPMAXQUEUETIME=30s unchanged |
| FEAT_019 | Advanced tree UI exposure | **[PARTIAL]** — connection and web limits UI refitted (commit `6b74300`) |

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

- recommended max connections still effectively lands at `500` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L1631)
- `MaxHalfConnections` still defaults to `9` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2142)
- the fallback burst limit is still `20` in [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h#L106) and [`PPgTweaks.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/PPgTweaks.cpp#L41)

These values were tuned for old Windows TCP stacks and much weaker home hardware.

### 2. Per-Client Upload Ceiling Is Too Low (FEAT_014)

Current code:

- `UPLOAD_CLIENT_MAXDATARATE` is fixed at `1 * 1024 * 1024` bytes/s in [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h#L109)
- this still caps the fallback per-slot target path in [`UploadQueue.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/UploadQueue.cpp#L518)

For a broadband branch, `1 MB/s` per client is an obsolete ceiling.

### 3. Socket Buffers Are Undersized (FEAT_015)

Current code:

- UDP receive buffer is forced to `64 KiB` in [`ClientUDPSocket.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/ClientUDPSocket.cpp#L533)
- TCP "big send buffer" is only `128 KiB` in [`EMSocket.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/EMSocket.cpp#L1092)

Those values are low for modern WAN throughput and modern memory budgets.

### 4. Disk Buffering Defaults Are Conservative (FEAT_016)

Current code:

- file buffer size defaults to `512 KiB` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2388)
- file buffer time limit defaults to `60s` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2393)
- part files flush on size/time in [`PartFile.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/PartFile.cpp#L2189)
- part files also force periodic flushes while active in [`PartFile.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/PartFile.cpp#L2193)

This is still safe, but leaves performance on the table for SSD/NVMe systems.

### 5. Queue And Source Limits Are Still Small (FEAT_017)

Current code:

- default `MaxSourcesPerFile` is `400` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2187)
- source soft/UDP caps are `750` and `50` in [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h#L96)
- queue default path still effectively lands around `5000` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2396)

These values were designed for older swarm sizes, older RAM sizes, and weaker CPUs.

### 6. Several Timeouts Are Long And Old (FEAT_018)

Current code in [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h):

- `CONNECTION_TIMEOUT = 40s`
- `DOWNLOADTIMEOUT = 100s`
- `KADEMLIAASKTIME = 1s`
- `UDPMAXQUEUETIME = 30s`
- `CONNECTION_LATENCY = 22050`

These may still be functional, but they are conservative and leave recovery/reactivity slower than necessary on modern links.

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
| `MaxConnections` default recommendation | `500`-style cap | `1000` |
| `MaxHalfConnections` default | `9` | `50` |
| `MaxConnectionsPerFiveSeconds` default | `20` | `50` |
| per-client upload cap | `1 MB/s` | `8 MB/s` |
| UDP receive socket buffer | `64 KiB` | `512 KiB` |
| TCP big send buffer | `128 KiB` | `512 KiB` |

Notes:

- `1000` is a conservative modern default, not an aggressive "max everything" value
- `50` half-open keeps the value modern without turning connection bursts into a free-for-all
- `8 MB/s` per-client cap is large enough not to cripple modern uplinks while still remaining bounded

### File, Queue, And Source Defaults (FEAT_016, FEAT_017)

| Setting | Current | Proposed |
| --- | --- | --- |
| `FileBufferSize` | `512 KiB` | `2 MiB` |
| `FileBufferTimeLimit` | `60s` | `120s` |
| `QueueSize` | ~`5000` effective path | `10000` |
| `MaxSourcesPerFile` | `400` | `600` |
| `MAX_SOURCES_FILE_SOFT` | `750` | `1000` |
| `MAX_SOURCES_FILE_UDP` | `50` | `100` |

Notes:

- `2 MiB` is still reasonable on modest systems and much less stale than `512 KiB`
- `120s` is long enough to reduce churn but still bounded
- `600` sources/file is a moderate modernization, not an extreme one

### Timeout Defaults (FEAT_018)

| Setting | Current | Proposed |
| --- | --- | --- |
| `CONNECTION_TIMEOUT` | `40s` | `30s` |
| `DOWNLOADTIMEOUT` | `100s` | `75s` |
| `UDPMAXQUEUETIME` | `30s` | `20s` |
| `CONNECTION_LATENCY` | `22050` | `12000` or `15000` |

Notes:

- these changes should be validated carefully because timeouts are easy to over-tighten
- the preferred style is still fixed values, not RTT adaptation

## Settings To Expose In The Advanced Tree (FEAT_019)

These should become visible advanced preferences if they are not already exposed.

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
- group them under a new `Modern connection and disk limits` subtree, or reuse existing relevant groups if the tree would become too large
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

Best candidates:

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

- change the recommendation path to return `1000` as the fixed modern default on current supported systems
- keep existing user-specified values unchanged
- update any UI helper text if it still implies older-era recommendations

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

- change default to `2 MiB`
- preserve existing user-configured values

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

- default `MaxSourcesPerFile = 400` in [`Preferences.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Preferences.cpp#L2187)
- soft/UDP caps in [`PartFile.cpp`](/C:/prj/p2p/eMulebb/eMule/srchybrid/PartFile.cpp#L5362) and [`Opcodes.h`](/C:/prj/p2p/eMulebb/eMule/srchybrid/Opcodes.h#L96)

Action:

- raise default to `600`
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
