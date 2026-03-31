# Kad Audit

## Table of Contents

- [Metadata](#metadata)
- [Executive Summary](#executive-summary)
- [Audit Item Register](#audit-item-register) (AUD_KAD_001–022)
- [Scope and Method](#scope-and-method)
- [Architectural Summary by Tree](#architectural-summary-by-tree)

## Metadata

- Date: 2026-03-31
- Target tree: `C:\prj\p2p\eMule\eMulebb\eMule-build\eMule`
- Comparison trees:
  - `C:\prj\p2p\eMule\analysis\eMule-0.60d`
  - `C:\prj\p2p\eMule\analysis\eMule-0.72a`
  - `C:\prj\p2p\eMule\analysis\eMuleAI`
- External reference material:
  - libtorrent DHT security extension: `https://libtorrent.org/dht_sec.html`
  - libtorrent DHT reference: `https://libtorrent.org/reference-DHT.html`
  - libtorrent settings reference: `https://libtorrent.org/reference-Settings.html`
  - libtorrent security audit notes: `https://www.libtorrent.org/security-audit.html`

---

## Executive Summary

The current Kad implementation in the target tree is closest to `eMule-0.72a` plus a set of branch-local hardening and routing-quality extensions. It is materially stronger than stock `eMule-0.60d` and `eMule-0.72a` in several important areas:

- stricter routing-contact admission
- adaptive timeout estimation
- better bootstrap reuse and ranking
- stronger `PUBLISH_SOURCE` validation and throttling
- safer `nodes.dat` replacement flow

The target also made a mostly correct selective import from `eMuleAI`: it kept the lower-risk Kad trust and latency ideas while dropping more invasive NAT traversal, buddy, callback, and IPv6-adjacent experiments.

The most important remaining weaknesses are:

- the default `nodes.dat` bootstrap URL still uses plain HTTP
- the current same-IP Kad anti-abuse policy is too blunt for modern CGNAT-heavy networks
- `eMuleAI` had a useful network-change grace period that the target dropped
- Kad abuse throttling is still narrow and should be generalized beyond source publishing
- Kad remains IPv4-oriented end-to-end even though some adjacent work points toward broader transport goals

The right next step is not a Kad protocol redesign. The right next step is to improve local policy only:

- trust scoring
- bootstrap trust
- routing diversity
- resource budgets
- observability
- test coverage

Those changes can be made without changing a single Kad opcode, packet shape, tag requirement, or timing contract that existing public-network peers depend on.

---

## Audit Item Register

This register assigns stable IDs to the concrete Kad audit items in this document. The IDs are intended to be used for follow-up implementation, review, and discussion.

| ID | Category | Summary |
|----|----------|---------|
| `AUD_KAD_001` | Security | **[REJECTED]** Default `nodes.dat` bootstrap still uses plain HTTP |
| `AUD_KAD_002` | Security | **[REJECTED]** Add authenticated bootstrap sources such as HTTPS mirrors and optional signatures |
| `AUD_KAD_003` | Hardening | **[REJECTED]** Treat imported bootstrap contacts as probationary and merge them with local trusted candidates |
| `AUD_KAD_004` | Routing | Keep and extend `FastKad` as a protocol-safe local optimization |
| `AUD_KAD_005` | Routing | Make `FastKad` bootstrap ranking diversity-aware and age stale sidecar trust more aggressively |
| `AUD_KAD_006` | Security/Scalability | Same-IP Kad rejection is too blunt for CGNAT-heavy modern networks |
| `AUD_KAD_007` | Routing | Evolve `SafeKad` toward layered trust, probation, and diversity scoring |
| `AUD_KAD_008` | Search | Add response-usefulness scoring instead of relying mostly on liveness and closeness |
| `AUD_KAD_009` | Search | Add subnet-diversity controls and adaptive fanout for search progression |
| `AUD_KAD_010` | Security | Keep `KadPublishGuard` and the stronger `PUBLISH_SOURCE` validation model |
| `AUD_KAD_011` | Security/Scalability | Generalize expensive-op throttling beyond `PUBLISH_SOURCE` |
| `AUD_KAD_012` | Resilience | Restore recent-network-change grace handling around routing persistence and probing |
| `AUD_KAD_013` | Observability | Add explicit Kad trust, budget, and bootstrap counters |
| `AUD_KAD_014` | Testing | Expand integration and fuzz coverage beyond helper-only Kad tests |
| `AUD_KAD_015` | Porting | Do not port `eMuleAI` multi-served-buddy logic as-is |
| `AUD_KAD_016` | Porting | Do not port partial Kad IPv6 tag plumbing as-is |
| `AUD_KAD_017` | Architecture | If IPv6 Kad is pursued, use a full dual-stack design with separate persisted state |
| `AUD_KAD_018` | Libtorrent | Borrow the principle that storage trust should be stricter than service trust |
| `AUD_KAD_019` | Libtorrent | Borrow IP/ID consistency as a local scoring signal only, not as a hard compatibility rule |
| `AUD_KAD_020` | Libtorrent | Borrow explicit DHT resource budgets and visible ceilings |
| `AUD_KAD_021` | Compatibility | Do not introduce new mandatory Kad tags, token semantics, or wire-level validation rules in a hardening pass |
| `AUD_KAD_022` | Roadmap | Prioritize bootstrap trust, network-change resilience, and generalized abuse budgets first |

---

## Scope and Method

This audit focused on the Kad-specific code paths and on adjacent bootstrap logic that directly affects Kad correctness, survivability, and abuse resistance:

- `srchybrid/kademlia/kademlia`
- `srchybrid/kademlia/net`
- `srchybrid/kademlia/routing`
- `srchybrid/kademlia/utils`
- `srchybrid/KademliaWnd.cpp`
- `srchybrid/ClientList.*` for buddy interaction
- `eMule-build-tests` for Kad helper coverage

The review compared:

1. old stock behavior in `eMule-0.60d`
2. newer stock behavior in `eMule-0.72a`
3. experimental/custom behavior in `eMuleAI`
4. the current target tree

The review also used `eMule-mods-archive` at a coarse level to check whether the target's `SafeKad`-style ideas are historically unusual. They are not. Older eMule mods also carried local anti-abuse layers, so the current target is not conceptually out of family.

---

## Architectural Summary by Tree

## 1. `eMule-0.60d`

`eMule-0.60d` is the older stock baseline. It has the familiar classic eMule Kad structure:

- `nodes.dat` persistence
- classic routing bucket logic
- stock search progression and jump-start logic
- stock UDP listener behavior
- no branch-local trust cache
- no adaptive bootstrap ranking
- no dedicated source-publish abuse throttle

It is useful mainly as the legacy reference point. It does not contain the hardening that matters for current hostile-network conditions.

## 2. `eMule-0.72a`

`eMule-0.72a` is the best stock comparison point for the current target. Relative to `0.60d`, it is mostly a newer stock implementation with cleanup and incremental evolution, but not a different Kad philosophy.

`0.72a` still remains fundamentally:

- a classic eMule Kad node
- with fixed local policy
- and without branch-local trust/performance scoring

That is why the current target reads as `0.72a` plus targeted local Kad policy enhancements.

## 3. `eMuleAI`

`eMuleAI` is the experimental branch in this comparison set. It added more intrusive Kad work:

- `SafeKad`
- `FastKad`
- more involved buddy/callback handling
- network-change grace handling
- ICMP unreachable plumbing
- partial Kad IPv6 tag handling

Some of those ideas are strong and worth preserving. Some are only useful if the surrounding architecture also changes.

## 4. Current Target

The current target adds the following important local Kad layers beyond stock `0.72a`:

- `kademlia/utils/SafeKad.*`
- `kademlia/utils/FastKad.*`
- `kademlia/utils/KadPublishGuard.*`
- `kademlia/utils/NodesDatSupport.*`
- bootstrap progress state in `kademlia/kademlia/Kademlia.*`
- `nodes.fastkad.dat` sidecar persistence

This is the main practical shift in the target tree:

- it no longer treats all reachable contacts as equally useful
- it no longer trusts source publishes as loosely as stock Kad
- it no longer accepts downloaded `nodes.dat` content without structural preflight

---

## Main Differences in Behavior

## 1. Bootstrap and `nodes.dat`

### Stock Trees

The stock trees treat `nodes.dat` as a persisted contact snapshot and bootstrap source, but the acceptance and replacement path is comparatively simple.

### Target Improvements

The target introduced:

- `InspectNodesDatFile()` to validate candidate snapshots
- `ReplaceNodesDatFile()` to atomically replace the persisted file
- bootstrap-only file recognition
- structurally usable contact counting
- rejection of malformed or empty bootstrap downloads
- a UI status line for the local `nodes.dat`

This is a good improvement and is exactly the kind of safety seam that should exist around externally fetched bootstrap material.

### Remaining Weakness

Related items:

- `AUD_KAD_001`
- `AUD_KAD_002`
- `AUD_KAD_003`

The default bootstrap URL is still:

- `http://upd.emule-security.org/nodes.dat`

This is the most important remaining Kad bootstrap weakness.

`NodesDatSupport` protects against malformed snapshots. It does not protect against hostile but well-formed contact sets. An on-path attacker can still provide a structurally valid `nodes.dat` that:

- biases initial routing
- slows bootstrap
- increases bad-contact density
- steers early lookups into attacker-preferred neighborhoods

### Security Assessment

Severity: High

This is high not because the file parser is weak, but because trust still begins from an unauthenticated source.

### Protocol-Safe Improvements

All of the following are safe and do not alter Kad protocol behavior:

- switch the default source to HTTPS
- support multiple official bootstrap mirrors
- pin expected hostnames or certificates for trusted mirrors
- support optional detached signatures for bootstrap snapshots
- log the source URL and acceptance reason for every imported snapshot
- import downloaded contacts into a probationary bootstrap set first
- merge remote bootstrap contacts with locally trusted high-quality contacts instead of trusting the remote set blindly

### Recommendation

This should be the first implementation item if Kad work continues.

---

## 2. `FastKad`

### What It Adds

`FastKad` adds two useful local capabilities:

1. adaptive timeout estimation from recent accepted responses
2. persisted bootstrap-quality hints in `nodes.fastkad.dat`

This is strong local policy because it changes only:

- how long the client waits
- which contacts it prefers first

It does not change any Kad packet semantics.

### Strong Points

- response times are bounded
- the estimated maximum response time is clamped
- bootstrap ranking uses recency, health, and observed latency
- metadata is keyed by Kad ID plus UDP port
- sidecar persistence is separate from `nodes.dat`
- dormant metadata survives `nodes.dat` churn

### Weak Points

The current implementation is still intentionally simple:

- health is a small local heuristic, not a full trust model
- ranking is not diversity-aware
- long-dormant nodes can keep stale positive hints for a long time
- there is no explicit subnet balancing in bootstrap selection
- timing quality is global and not segmented by operation type

### Practical Value

Related items:

- `AUD_KAD_004`
- `AUD_KAD_005`

`FastKad` is worth keeping. It is one of the cleanest examples in the target tree of a protocol-safe local performance enhancement.

### Protocol-Safe Improvements

- decay health more aggressively as nodes age
- add subnet diversity bias when ranking bootstrap candidates
- cap very old dormant sidecar influence
- track jitter and not only approximate response-time spread
- maintain separate quality bands for bootstrap, hello verification, and search-response traffic
- use adaptive concurrency limits in addition to adaptive timeout

### Recommendation

Keep and extend `FastKad`.

---

## 3. `SafeKad`

### What It Adds

`SafeKad` is the main local Kad anti-abuse layer in the target. It adds:

- identity stability tracking by `(IP, UDP port)`
- short-lived problematic-node tracking
- temporary IP bans for stronger abuse signals
- admission-time rejection of suspicious contacts

### Why It Helps

Classic Kad is too trusting about:

- rapid ID flipping
- repeated low-value responders
- repeated timeout-heavy contacts
- same-host route stuffing

`SafeKad` directly addresses those classes of weakness.

### Strong Points

- bounded caches
- explicit cleanup
- optional hard-ban mode
- integration into routing admission
- integration into verification paths
- integration into search response trust decisions

### Important Limitation

Related items:

- `AUD_KAD_006`
- `AUD_KAD_007`

The current same-IP policy is still too absolute.

In practice, it behaves close to "one routed node per public IP" in important decision points. That helps against simple Sybil stuffing, but it also clashes with modern deployment reality:

- CGNAT
- mobile carriers
- enterprise NAT
- campus NAT
- shared residential gateways

The issue is not that the defense is wrong. The issue is that it is too blunt.

### Security Assessment

Severity: Medium

This is a correctness and scalability issue more than a pure exploit bug.

### Protocol-Safe Improvements

- use one good contact per IP per bucket as a preference instead of a global hard rule
- allow same-IP contacts into a probation state if they use different UDP ports and remain stable
- separate "not trusted for routing/store preference" from "completely rejected"
- use diversity as a ranking and replacement heuristic before using it as a hard reject
- keep hard bans for stronger signals only:
  - verified-ID flips
  - repeated malformed expensive requests
  - repeated flood behavior

### Recommendation

Keep `SafeKad`, but evolve it from a coarse gate into a layered trust model:

- probation
- scoring
- diversity preference
- hard bans only for the strongest signals

---

## 4. Search Logic

### Target Improvements

The target's search behavior is already better than stock in several practical ways:

- adaptive stall detection through `FastKad`
- failure learning from unanswered contacts
- short-lived problematic-node avoidance
- rejection of routing responses that exceed the expected contact count

This is meaningfully better than classic static-timeout Kad.

### What Still Looks Weak

Related items:

- `AUD_KAD_008`
- `AUD_KAD_009`

Search is still primarily driven by:

- XOR closeness
- pending/timeout state
- duplicate suppression

It does not yet seem to use a full response-quality model:

- no explicit usefulness score for responders
- no explicit low-yield penalty beyond short-lived problematic state
- no route-quality memory by subnet/prefix
- no adaptive query fanout based on current quality conditions

### Protocol-Safe Improvements

- score response usefulness, not just liveness
- penalize responders that repeatedly send:
  - dead contacts
  - duplicate contacts
  - low-diversity contact sets
  - no closer contacts
- rank future candidates by both closeness and historical utility
- maintain per-search subnet diversity caps
- adapt query fanout to current timeout rate and answer quality

### Recommendation

This is a good next-stage improvement area because it improves:

- latency
- search quality
- passive poisoning resistance

without changing any Kad packet syntax.

---

## 5. `PUBLISH_SOURCE` Handling

### Target Improvements

Related items:

- `AUD_KAD_010`
- `AUD_KAD_011`

This is one of the better hardening areas in the target tree.

The target adds:

- accepted source-type validation
- low-ID buddy metadata completeness checks
- per-IP source-publish throttling
- drop and ban escalation for abusive publish rates
- rejection of malformed publish metadata

### Why This Matters

Source publishing is a natural abuse surface for:

- spam
- malformed low-ID metadata
- index pollution
- CPU and memory pressure

The target improves this materially relative to stock.

### Strength Assessment

This code is worth keeping.

### Remaining Limitation

The same budget model should be applied more broadly. Today, the target has:

- specific `PUBLISH_SOURCE` throttling

But not a broader Kad expensive-op budget framework.

### Protocol-Safe Improvements

- extend the same throttle style to other expensive Kad paths
- add byte-based quotas, not only request-count quotas
- add per-prefix budgets in addition to per-IP budgets
- keep counters for:
  - dropped expensive requests
  - malformed expensive requests
  - escalated abusive senders

### Recommendation

Generalize `KadPublishGuard` concepts into a wider Kad abuse-budget framework.

---

## 6. Buddy and Callback Handling

### `eMuleAI` Direction

Related items:

- `AUD_KAD_015`

`eMuleAI` experimented with:

- served-buddy capacity limits
- more elaborate callback forwarding selection
- requester-provided external callback IP handling
- NAT traversal-specific filtering

### Target Direction

The target uses a simpler one-buddy model:

- `GetBuddy()`
- `RequestBuddy()`
- `IncomingBuddy()`

That simpler model is easier to reason about and less invasive.

### Porting Assessment

The `eMuleAI` buddy work is not a good direct cherry-pick target because it assumes:

- a richer served-buddy state model
- callback routing state that the target does not currently model
- more intrusive NAT traversal behavior

### Useful Idea Worth Revisiting

One family of idea is still worth re-evaluating carefully:

- more defensive callback forwarding when source endpoint attribution is ambiguous or private

But that should be redesigned specifically for the target's current one-buddy architecture, not merged from `eMuleAI` as-is.

### Recommendation

Do not port `eMuleAI` buddy/callback logic wholesale.

---

## 7. IPv6

### What `eMuleAI` Added

Related items:

- `AUD_KAD_016`
- `AUD_KAD_017`

`eMuleAI` added some Kad-related IPv6 metadata handling through extra publish/result tags.

### Why It Is Not a Safe Cherry-Pick

The target still has an IPv4-oriented Kad result consumer boundary. In particular, the search-result handoff into the download queue remains IPv4-shaped.

That means importing only the IPv6 tag plumbing would create a half-feature:

- some IPv6 Kad metadata would be parsed or published
- but the rest of the application could not consume it coherently

### Better Direction

If IPv6 Kad support becomes a real target feature, it should be a full dual-stack effort covering:

- transport
- endpoint representation
- routing storage
- search result delivery
- buddy logic
- bootstrap persistence

Libtorrent is helpful here because it persists distinct DHT bootstrap state for:

- `nodes`
- `nodes6`

That separation is architecturally sound and worth copying conceptually.

### Recommendation

Do not cherry-pick the partial IPv6 Kad work from `eMuleAI` in isolation.

---

## Security Findings

## High Severity

### H1. Plain-HTTP default bootstrap source (`AUD_KAD_001`)

This remains the most important practical Kad security issue in the target.

Impact:

- an on-path attacker can still supply a structurally valid but malicious bootstrap set
- local structural validation does not establish trust in the routing snapshot source
- the initial routing horizon can still be biased by bootstrap poisoning

Fix priority: immediate

Compatibility risk: none

## Medium Severity

### M1. Same-IP hard rejection is too blunt for modern NAT topologies (`AUD_KAD_006`)

Impact:

- legitimate peers can be rejected on dense public-IP edges
- route diversity may degrade unintentionally
- anti-Sybil policy becomes accidental anti-density policy

Fix priority: high

Compatibility risk: none if limited to local policy

### M2. Lost network-change grace handling (`AUD_KAD_012`)

Impact:

- more stale-contact probing after rebind or interface churn
- more risk of writing a poor `nodes.dat` snapshot immediately after a local transport transition
- worse behavior on VPN, Wi-Fi, mobile, and laptop-style networks

Fix priority: high

Compatibility risk: none

### M3. Abuse budgeting is still too narrow (`AUD_KAD_011`)

The target throttles source publishing well, but it does not yet expose a generalized expensive-op budget across Kad.

Impact:

- abusers can pressure adjacent costly paths
- flood handling and expensive-op budgeting remain separate concerns

Fix priority: medium-high

Compatibility risk: none

## Low Severity

### L1. `FastKad` ranking is not diversity-aware (`AUD_KAD_005`)

Impact:

- bootstrap may over-prefer a dense cluster of once-good nodes

Fix priority: medium

Compatibility risk: none

### L2. End-to-end Kad regression coverage is still limited (`AUD_KAD_014`)

The test tree already covers helper logic for:

- `FastKad`
- `SafeKad`
- `KadPublishGuard`
- `NodesDatSupport`

But there is still limited integration coverage for:

- routing churn after network change
- bootstrap queue behavior
- buddy/callback edge cases
- live-response quality scoring

Fix priority: medium

Compatibility risk: none

---

## What Is Worth Porting

## Keep and Extend

The following target features are worth preserving and extending:

- `SafeKad` (`AUD_KAD_007`)
- `FastKad` (`AUD_KAD_004`)
- `KadPublishGuard` (`AUD_KAD_010`)
- `NodesDatSupport` (`AUD_KAD_002`, `AUD_KAD_003`)
- bootstrap progress reporting
- `nodes.fastkad.dat` sidecar persistence

## Worth Reintroducing from `eMuleAI`, But Only After Refactor

- recent-network-change grace handling around routing-table writes and probing (`AUD_KAD_012`)
- some callback-path caution where endpoint attribution is ambiguous

## Not Worth Porting As-Is

- multi-served-buddy logic (`AUD_KAD_015`)
- partial Kad IPv6 tag handling without full dual-stack support (`AUD_KAD_016`)
- invasive NAT traversal experiments
- pointer-heavy helper internals where the target already has simpler value-based logic

---

## What Can Be Borrowed from Libtorrent

This section needs a careful reading. Libtorrent's DHT is BitTorrent mainline DHT, not eMule Kad. The goal is not to copy protocol mechanics blindly. The goal is to borrow local policy and architecture where it helps.

## 1. Node-ID / IP Relationship Checks

Related item:

- `AUD_KAD_019`

Libtorrent's DHT security extension ties a valid node ID to the node's external IP prefix. The purpose is to make targeted keyspace occupation harder.

What is worth borrowing:

- the idea that node identity should have some externally checkable relationship to reachable addressing
- the idea that storage trust can be stricter than service trust
- the idea that local/private addresses may need different handling

What is not safe to borrow directly:

- hard-enforcing a BEP42-style node-ID formula on current eMule Kad peers

Why not:

- eMule Kad peers do not implement BitTorrent DHT security extension semantics
- hard enforcement would reject peers that are protocol-valid on today's eMule network
- there is no transition mechanism available here

Safe adaptation for eMule Kad:

- use IP/ID consistency as a local score, not a mandatory network validity test
- apply the strongest trust thresholds only when choosing store/publish targets
- continue to service ordinary requests even from suspicious peers
- never treat a non-matching formula as a wire-level protocol violation

That preserves protocol compatibility.

## 2. Storage Trust Should Be Stricter Than Service Trust

Related item:

- `AUD_KAD_018`

This is one of libtorrent's best transferable ideas.

Libtorrent explicitly distinguishes between:

- servicing DHT requests
- trusting a node enough to store data on it

That distinction maps well onto eMule Kad.

What eMule Kad should borrow:

- remain interoperable by answering requests broadly
- become much pickier about which nodes receive stored state
- become much pickier about which nodes become preferred bootstrap/search anchors

Practical adaptation:

- strict trust threshold for store targets
- moderate trust threshold for preferred bootstrap candidates
- lower trust threshold for ordinary traversal during search

This improves safety without partitioning the public network.

## 3. Explicit Resource Budgets

Related item:

- `AUD_KAD_020`

Libtorrent exposes and documents hard DHT budgets such as:

- upload rate limits
- stored-item limits
- peer count limits

The exact values are not the important thing here. The important thing is the design principle:

- expensive DHT work must have explicit hard memory and bandwidth ceilings

The target already has:

- classic Kad index limits
- source-publish throttling

It should go further.

What eMule Kad can safely borrow:

- per-opcode token buckets
- byte-budgeted expensive-response quotas
- explicit memory ceilings per index family
- visible eviction counters and reasons

All of this is protocol-safe because it changes only local resource governance.

## 4. Dual-Stack State Separation

Related item:

- `AUD_KAD_017`

Libtorrent persists separate bootstrap state for IPv4 and IPv6. That is architecturally sound.

What eMule Kad can borrow if dual-stack work ever starts:

- separate persisted state
- separate bootstrap pools
- separate endpoint validation logic
- shared trust policy over transport-specific endpoint models

What it should not do:

- bolt IPv6 tags onto an otherwise IPv4-only Kad data path and call the feature complete

## 5. Token and Cookie Thinking

Libtorrent uses rotating write-token logic for store-related trust decisions. The underlying idea is useful even where the exact mechanism is not.

What eMule Kad can borrow conceptually:

- keep validation state-light where possible
- prefer rotating short-lived secrets to large long-lived trust tables when validating expensive operations
- tie expensive-operation trust to short-lived local context

What it should not do in a compatibility-preserving pass:

- introduce new mandatory token semantics into current Kad messages

That would be a protocol change.

## 6. Policy Belongs in Local Behavior, Not in Surprise Wire Changes

Libtorrent is a useful reminder that:

- local aggressiveness belongs in settings and policy
- protocol compatibility belongs in the wire format

For this eMule Kad tree, that means:

- aggressive local timeout, diversity, and ranking policy is safe
- mandatory new tags are not safe
- mandatory new request semantics are not safe

---

## Protocol-Safe Improvement Catalogue

This section lists improvements that can be made without breaking Kad protocol behavior in any shape or form.

## A. Bootstrap Trust

- switch the default `nodes.dat` source to HTTPS (`AUD_KAD_001`)
- support multiple mirrors (`AUD_KAD_002`)
- support optional detached signatures (`AUD_KAD_002`)
- import downloaded bootstrap contacts into probation first (`AUD_KAD_003`)
- merge imported contacts with local trusted candidates instead of fully trusting the imported set (`AUD_KAD_003`)
- log bootstrap source and acceptance reason

## B. Routing Admission

- replace global same-IP hard rejection with weighted diversity policy (`AUD_KAD_006`)
- allow same-IP contacts into probation
- promote same-IP contacts only after successful verified interactions (`AUD_KAD_007`)
- prefer verified and productive contacts for bucket retention
- use subnet diversity as a replacement preference

## C. Search Quality

- score response usefulness, not just reachability (`AUD_KAD_008`)
- penalize nodes that repeatedly return dead, duplicate, or low-diversity contacts
- rank future candidates by both XOR closeness and historical utility
- maintain per-search subnet diversity limits (`AUD_KAD_009`)
- adapt query fanout to observed timeout rate and answer quality (`AUD_KAD_009`)

## D. Abuse Budgets

- generalize throttle logic beyond `PUBLISH_SOURCE` (`AUD_KAD_011`)
- add per-opcode token buckets
- add byte-based send quotas
- add per-prefix budgets
- count drops by reason

## E. Storage and Index Hardening

- expose explicit per-index memory ceilings
- make eviction policy visible in logs
- evict low-trust and stale publishers first
- track malformed publish patterns separately from quota exhaustion

## F. Network-Transition Resilience

- restore a recent-rebind grace period (`AUD_KAD_012`)
- delay routing probes briefly after local endpoint churn
- avoid overwriting persisted routing state immediately after a transport transition
- distinguish "network unstable" from "routing table empty" in diagnostics

## G. Telemetry

- count verified contacts (`AUD_KAD_013`)
- count probation contacts
- count same-IP rejections
- count verified-ID flips
- count dropped expensive requests by reason
- expose adaptive timeout estimate and sample count
- record bootstrap success and failure reasons

## H. Testing

- integration tests for bootstrap import and live reload (`AUD_KAD_014`)
- tests for routing behavior after simulated network change
- tests for buddy and callback edge cases
- tests for response-quality scoring
- fuzzing for Kad packet and tag parsing

None of the above requires changing Kad packet syntax or mandatory peer behavior.

---

## Changes That Should Not Be Done in a Compatibility-Preserving Pass

Related item:

- `AUD_KAD_021`

The following changes are likely to fork behavior or break network expectations:

- making new Kad tags mandatory
- making new token or challenge semantics mandatory
- requiring BitTorrent-style node-ID derivation from current eMule Kad peers
- changing opcode meaning or response cardinality
- changing current publish semantics so old peers can no longer satisfy them
- changing on-disk routing serialization in a way legacy loaders cannot tolerate

If any of those ideas are pursued, they belong in an explicit protocol-evolution effort, not in a hardening pass.

---

## Suggested Roadmap

Related item:

- `AUD_KAD_022`

## Phase 1: Immediate, Low-Risk Hardening

- move the default bootstrap source to HTTPS or a signed/mirrored equivalent
- restore recent-network-change grace handling
- generalize expensive-op budgets beyond source publishing
- extend Kad diagnostics and counters

## Phase 2: Better Local Trust Model

- evolve `SafeKad` from a coarse same-IP gate into layered trust and diversity scoring
- add probation contacts
- add response usefulness scoring
- use stronger trust thresholds for store-target selection

## Phase 3: Better Resource Governance

- add per-opcode byte budgets
- expose indexed-storage ceilings and eviction stats
- add more integration tests for bootstrap and routing churn

## Phase 4: Optional Larger Work

- full dual-stack Kad design
- callback-path validation redesign
- signed bootstrap distribution

---

## Final Assessment

The current target Kad implementation is materially stronger than stock `eMule-0.60d` and `eMule-0.72a` in the areas that matter most under modern hostile-network conditions:

- bootstrap usefulness
- routing admission discipline
- timeout adaptation
- source-publish abuse handling

The target also made a sensible choice in not importing every `eMuleAI` experiment blindly.

The most important work still left is not protocol invention. It is careful continuation of the same local-policy approach already visible in the current branch:

- stricter local trust
- hard resource ceilings
- safer bootstrap origin
- better diversity policy
- better observability
- better tests

Libtorrent is most useful here as a source of design principles, not as a drop-in protocol template:

- strict trust only where it matters most
- explicit budgets for expensive behavior
- dual-stack separation where transport families differ
- careful distinction between compatibility and local policy

Those principles can be adapted into this eMule Kad tree without changing the wire protocol and without partitioning the current network.
