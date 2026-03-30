# Resume

## Last Chunk

- Implemented D-03 / D-04 Kad hardening in `4798953` (`FIX port Fast Kad Safe Kad and Kad publish guards`).
- Added `FastKad`, `SafeKad`, and `KadPublishGuard` helpers and wired them into Kad search timeout handling, routing admission/cleanup, hello/challenge verification, publish-source validation, and bootstrap progress reporting.
- Added advanced-preference wiring for `BanBadKadNodes` and `KadPublishSourceThrottle`, including `PPgTweaks` controls and localized strings.
- Added shared Kad regression coverage in the sibling test repo via `a2c7638` (`TEST add Kad guard regression coverage`).
- Validation passed:
  - Main build: `..\23-build-emule-debug-incremental.cmd`
  - Wrapper log: `C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260330-223157-build-project-eMule-Debug\eMule-Debug.log`
  - Shared tests: `C:\prj\p2p\eMule\eMulebb\eMule-build-tests\build\default\x64\Debug\emule-tests.exe`
  - Test result: `42` cases passed, `94` assertions passed

## Current State

- Fast Kad now learns recent response times and uses that estimate to avoid fixed Kad search jump-start timing.
- Safe Kad now tracks node identity per `IP:UDPPort`, keeps short-lived problematic-node state, and can temporarily ban verified fast-flipping identities when enabled.
- `KADEMLIA2_PUBLISH_SOURCE_REQ` now has dedicated per-IP throttling plus stricter source and buddy metadata validation on top of the generic packet tracker.
- The Kad status text now shows bootstrap progress while consuming the bootstrap list.
- The tree still has the unrelated user-side `AGENTS.md` modification in the main repo working tree; leave it alone unless explicitly requested.

## Next Chunk

- Revisit the remaining D-03 gap from the modernization plan: persisted recency-aware `nodes.dat` reuse and stronger startup prioritization of recently seen Kad contacts if we want closer parity with the eMuleAI description.
- Audit whether Safe Kad should also pause or suppress selected routing actions during explicit reconnect / IP-change transitions instead of relying only on cache cleanup during `RecheckFirewalled()`.
- If Kad work continues, add focused regression coverage for the status-bar bootstrap text path or additional publish-source malformed-tag cases if a concrete bug appears.
