# Resume

## Last Chunk

- Fixed a debug-build Kad assertion introduced by Fast Kad bootstrap seeding.
- Replaced the bootstrap-queue copy path in `RoutingZone.cpp` so seeded bootstrap contacts are rebuilt from primitive fields instead of copy-constructing GUI-owned routing contacts.
- Validation passed:
  - Main build: `..\23-build-emule-debug-incremental.cmd`
  - Wrapper log: `C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260331-005629-build-project-eMule-Debug-x64\eMule-Debug-x64.log`
  - Shared tests: `C:\prj\p2p\eMule\eMulebb\eMule-build-tests\build\default\x64\Debug\emule-tests.exe`
  - Test result: `49` cases passed, `203` assertions passed

## Current State

- D-05 `nodes.dat` refresh hardening remains in place.
- Fast Kad startup bootstrap seeding no longer copy-constructs contacts that are already owned by the Kad GUI list, so debug builds can bootstrap without tripping `CContact::Copy`.
- Shared Kad guard coverage still passes after the bootstrap seeding fix.

## Next Chunk

- If Kad work continues, decide whether D-05 should grow an automatic stale-`nodes.dat` refresh path or stay as a hardened manual/on-demand refresh only.
- Consider hardening `server.met` and related bootstrap file replacement flows with the same validation/replacement seam used for `nodes.dat`.
- Keep IPv6 transport work separate; the Kad routing and bootstrap persistence path remains IPv4-oriented by the current transport model.
