# Kad Improvement Plan

## Table of Contents

- [Summary](#summary)
- [Feature Tracking](#feature-tracking)
- [Primary Goals](#primary-goals)
- [Guiding Principles](#guiding-principles)
- [Phase 1: Bring In CSafeKad2 and CFastKad](#phase-1-bring-in-csafekad2-and-cfastkad-feat_002-feat_003)
- [Phase 2: Refactor the Imported Implementations](#phase-2-refactor-the-imported-implementations-feat_002-feat_003)
- [Phase 3: Integrate into Current Kad Entry Points](#phase-3-integrate-into-current-kad-entry-points-feat_004)
- [Phase 4: Improve Routing Quality Beyond eMuleAI](#phase-4-improve-routing-quality-beyond-emuleai-feat_005)
- [Phase 5: Observability and Debuggability](#phase-5-observability-and-debuggability-feat_006)
- [Phase 6: Optional Advanced Improvements](#phase-6-optional-advanced-improvements-feat_007-feat_008)
- [eMuleAI Reference Implementation](#emuleai-reference-implementation)
- [Implementation Order](#implementation-order)
- [Validation Plan](#validation-plan)

## Summary

This plan modernizes the current Kad implementation in `eMulebb/eMule` with a focus on security, routing quality, search efficiency, and observability. The first execution chunk has already brought in `SafeKad` and `FastKad` in this branch, but the broader refactor, diagnostics, and validation work described below is still only partially complete.

The goal is to improve Kad behavior without introducing protocol incompatibilities. All planned changes are local policy, timing, routing, and diagnostics changes. No Kad wire format changes are required.

## Feature Tracking

| ID | Feature | Status |
|----|---------|--------|
| FEAT_002 | CSafeKad2 import and refactor | **[PARTIAL]** — `SafeKad` is imported, compiled, and live in Kad paths; broader planned cleanup and diagnostics remain |
| FEAT_003 | CFastKad import and refactor | **[PARTIAL]** — `FastKad` is imported, compiled, and live in Kad paths; broader planned tuning/diagnostics remain |
| FEAT_004 | Kad integration into entry points | **[PARTIAL]** — integrated into current UDP/search/routing flow, but not all planned validation/diagnostic work is complete |
| FEAT_005 | Routing quality improvements | Not started |
| FEAT_006 | Kad observability/diagnostics | Not started |
| FEAT_007 | Persisted trust cache (optional) | Not started |
| FEAT_008 | Search-mode adaptation (optional) | Not started |

## Primary Goals

- Harden Kad against identity spoofing, poisoned routing answers, and persistently bad responders.
- Reduce wasted Kad search time by adapting timeout behavior to observed network conditions.
- Improve routing table quality by preferring reliable and diverse contacts over merely alive contacts.
- Increase visibility into Kad health so tuning and regressions are diagnosable.
- Keep the work incremental and validate each phase with targeted functional testing.

## Guiding Principles

- Do not port eMuleAI code blindly. Reuse ideas and logic, but refactor implementations to match this branch.
- Avoid wrappers, compatibility mappings, and dead transitional code.
- Keep all changes local to Kad internals unless a clear cross-module benefit justifies broader refactoring.
- Preserve protocol compatibility with the current Kad network.
- Default to conservative policy where false positives could disconnect valid peers.

## Phase 1: Bring In `CSafeKad2` and `CFastKad` (FEAT_002, FEAT_003)

### `CSafeKad2` scope

- Introduce node identity tracking keyed by `(IP, UDP port)`.
- Track verified Kad identity stability across time.
- Track short-lived problematic nodes that repeatedly fail or return low-value behavior.
- Maintain a bounded banned-IP cache for severe cases.
- Add cleanup and expiry logic so memory stays bounded.

### `CSafeKad2` behavior

- Record a verified node identity after trusted hello/ack verification paths.
- Reject or downgrade nodes whose verified Kad ID changes too quickly.
- Mark nodes as problematic when they timeout, repeatedly fail, or deliver obviously bad search behavior.
- Evict banned contacts from routing maintenance.
- Gate hard banning behind a dedicated preference.
- Keep the default threshold conservative to reduce false positives.

### `CFastKad` scope

- Track recent response times from accepted Kad replies.
- Compute an adaptive timeout estimate from bounded recent samples.
- Use current fixed timeout logic as fallback until enough samples exist.
- Clamp the adaptive estimate to safe minimum and maximum bounds.

### Why import them jointly

- Both touch the Kad search response path.
- `CSafeKad2` decides whether a node is trustworthy.
- `CFastKad` decides how long to wait for trustworthy nodes.
- Importing them together avoids reopening the same Kad search and UDP listener code twice.
- They remain logically independent and must be toggleable independently.

## Phase 2: Refactor the Imported Implementations (FEAT_002, FEAT_003)

### `CSafeKad2` refactor requirements

- Replace raw owning pointers with value storage or `std::unique_ptr`.
- Simplify dual-map lifetime bookkeeping if possible while preserving bounded cleanup.
- Add explicit comments describing the attack model for each rule.
- Keep node-level tracking separate from IP-level hard bans.
- Add counters for tracked, problematic, banned, expired, and promoted-to-ban entries.
- Keep a clear preference gate for hard bans.
- Avoid assuming multi-thread safety unless the code explicitly provides it.

### `CFastKad` refactor requirements

- Replace heap-allocated sample entries with value types.
- Use a monotonic timing source better suited to network latency than `clock()` if available in this branch.
- Require a minimum sample count before adaptive timeout is trusted.
- Ignore bad samples from rejected, suspicious, duplicate, or clearly outlier responses.
- Clamp adaptive timeout to bounded min/max values.
- Expose sample count and current estimate through diagnostics.

## Phase 3: Integrate into Current Kad Entry Points (FEAT_004)

### Required integration points

- Kad UDP hello and ack verification paths.
- Kad search response processing.
- Kad search timeout/failure cleanup.
- Kad routing table maintenance and contact eviction.
- Kad shutdown cleanup.
- Preferences load/save for the hard-ban option.

### Expected current-file touch points

- `srchybrid/Kademlia/net/KademliaUDPListener.cpp`
- `srchybrid/Kademlia/kademlia/Search.cpp`
- `srchybrid/Kademlia/routing/RoutingZone.cpp`
- `srchybrid/Kademlia/kademlia/Kademlia.cpp`
- `srchybrid/Preferences.h`
- `srchybrid/Preferences.cpp`

### Initial integration rules

- Track node identity only from trusted verification paths, not from every incoming packet.
- Feed latency samples into `CFastKad` only from accepted responses.
- Do not let `CFastKad` learn from nodes flagged as bad or problematic.
- Keep the current timeout logic as fallback during rollout.
- Keep UI work optional in the first chunk; preference storage is required, UI exposure is not.

## Phase 4: Improve Routing Quality Beyond eMuleAI (FEAT_005)

### Contact reliability scoring

- Add a per-contact reliability score independent of hard bans.
- Track success ratio, timeout rate, invalid-response rate, and last-good timestamp.
- Use reliability in contact selection and replacement decisions.

### Contact diversity rules

- Prefer routing buckets with more IP-range diversity.
- Avoid concentrating too many contacts from the same `/24` where practical.
- Use diversity as a soft preference, not an absolute blocker.

### Probation model

- Add a probation state for newly learned contacts.
- Promote contacts only after at least one successful verified interaction.
- Prefer established contacts over probation contacts when choosing search candidates.

### Response-quality scoring

- Score nodes by the quality of contacts they return.
- Penalize nodes that repeatedly return dead, filtered, duplicate, or low-diversity contact sets.
- Lower query priority for low-yield nodes without immediately banning them.

## Phase 5: Observability and Debuggability (FEAT_006)

### New Kad diagnostics

- Count verified contacts.
- Count probation contacts.
- Count problematic nodes.
- Count banned IPs.
- Track adaptive timeout estimate and sample count.
- Track invalid-response rate.
- Track useful-results-per-query.

### Logging requirements

- Log when a node is rejected for identity inconsistency.
- Log when a node is promoted from problematic to banned.
- Log when adaptive timeout changes materially.
- Log why a routing answer was considered low quality.
- Keep logs concise and keyed to specific decision reasons.

## Phase 6: Optional Advanced Improvements (FEAT_007, FEAT_008)

### Persisted trust/performance cache (FEAT_007)

- Persist a small cache of recent Kad trust and timing summaries across restarts.
- Do not persist full routing state in this feature.
- Use the cache only as a warm-start hint, not as authoritative trust.

### Search-mode adaptation (FEAT_008)

- Add a conservative mode for low-quality network conditions.
- Add an aggressive mode when response quality is high.
- Choose mode automatically based on observed timeout rate and answer quality.

### Network-transition resilience

- Detect recent IP/port rebinding.
- During a short grace period, reduce ban aggressiveness and avoid overwriting known-good routing state too eagerly.

## eMuleAI Reference Implementation

eMuleAI contains working implementations of both CSafeKad2 and CFastKad that serve as reference material for this plan.

**CSafeKad2** (`SafeKad.h`, `SafeKad.cpp` — 262 lines):
- Uses 3 tracking maps capped at 10K/10K/1K entries (`m_mapTrackedNodes`, `m_mapProblematicNodes`, `m_mapBannedIPs`)
- Minimum ID change interval: 1 hour (`s_tMinimumIDChangeInterval = 3600`)
- Maximum ban duration: 4 hours (`s_tMaximumBanTime = 4*3600`)
- Fully standalone — no `CAddress` dependency needed, uses `uint32` IP internally
- Can be ported without touching any other eMuleAI feature

**CFastKad**:
- Tracks last 100 RTT samples from accepted Kad replies
- Computes adaptive timeout as `mean + k * stddev`
- Falls back to fixed timeout until minimum sample count is reached

Both modules are logically independent and toggleable separately. During import they should be refactored to match eMulebb conventions rather than copied verbatim.

Current branch note:

- `SafeKad` and `FastKad` already exist under `srchybrid/kademlia/utils/`
- both are compiled into the project and referenced from live Kad code in the UDP listener, search flow, routing, and Kad startup/shutdown paths
- shared regression coverage also exists in `eMule-build-tests`
- the remaining gap is not import presence, but completing the broader cleanup, diagnostics, and follow-on routing-quality work promised by this plan

## Implementation Order

1. Import and refactor `CSafeKad2`.
2. Import and refactor `CFastKad`.
3. Wire both into Kad search response handling.
4. Wire `CSafeKad2` into trusted hello/ack verification paths.
5. Wire banned-contact eviction into routing maintenance.
6. Add preference persistence for bad Kad node banning.
7. Add initial Kad counters and debug logging.
8. Validate behavior before any diversity or reliability scoring work.
9. Add reliability scoring and probation model.
10. Add response-quality scoring and diversity preferences.
11. Add optional persisted trust/performance cache only after core behavior is stable.

## Defaults and Decisions

- Bring in `CSafeKad2` and `CFastKad` jointly.
- Do not copy either implementation verbatim.
- Keep hard bans preference-gated.
- Keep adaptive timeout fallback to current logic until enough samples exist.
- Skip first-pass UI work unless required for validation.
- Treat observability as mandatory, not optional.
- Do not require `CAddress` or IPv6 work for this chunk.
- Keep all behavior protocol-compatible.

## Risks

- IP-wide bans may affect multiple users behind shared public IPs.
- Over-aggressive adaptive timeout may reduce search completion if bounds are poor.
- Reliability scoring can accidentally bias against slow but valid peers if thresholds are not conservative.
- Diversity constraints can reduce reachable-node count if implemented too rigidly.
- Without diagnostics, tuning errors will be difficult to distinguish from normal Kad variance.

## Validation Plan

### Functional checks

- Verify normal Kad bootstrap still succeeds from empty state.
- Verify hello/ack verification still promotes valid contacts.
- Verify repeated bad identity changes are caught.
- Verify problematic nodes expire correctly.
- Verify banned nodes are evicted from routing maintenance.
- Verify adaptive timeout stays within expected bounds.
- Verify search completion does not regress under normal healthy networks.

### Adversarial checks

- Simulate a node that changes Kad ID too quickly.
- Simulate nodes that repeatedly timeout.
- Simulate low-quality routing answers with many dead or filtered contacts.
- Simulate noisy latency spikes and confirm timeout clamps hold.

### Regression checks

- Compare search completion time before and after.
- Compare bootstrap time before and after.
- Compare routing-table size and verified-contact count after steady-state runtime.
- Compare invalid-response handling and duplicate-query behavior.
- Check shutdown cleanup for leaks or stale structures in debug builds.

## Expected First Execution Chunk

The first implementation chunk should include:

- modernized `CSafeKad2`
- modernized `CFastKad`
- Kad UDP listener integration
- Kad search integration
- routing eviction integration
- preference persistence
- Kad counters and debug logging

The first chunk should not yet include:

- full UI for every tuning option
- persisted trust cache
- aggressive diversity heuristics
- large routing-table policy rewrites beyond what is needed for safe Kad filtering

## Notes

- Update `RESUME.md` when execution starts so the exact last and next chunk are tracked.
- Build using `..\\23-build-emule-debug-incremental.cmd`.
- Keep code comments explicit around the new Kad policy logic so the purpose is easy to identify during later review.
