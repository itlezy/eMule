# Resume

## Last Chunk

- Expanded `srchybrid/ProtocolGuards.h` on the current branch with audit-driven helpers for compressed UDP payload gating, blob remaining-length checks, bounded progress calculations, and dotted IPv4 literal parsing.
- Routed the live dev code through those helpers in `ClientUDPSocket.cpp`, `Packets.cpp`, `KnownFile.cpp`, `SharedFileList.cpp`, `EmuleDlg.cpp`, and `ED2KLink.cpp`, which hardens `BBUG_018`, `BBUG_020`, `BBUG_026`, `BBUG_028`, `BBUG_029`, and `BBUG_035`.
- Mirrored the expanded guard seam into the oracle tree with legacy semantics so the dev/oracle split stays observable in shared tests.
- Extended the shared `protocol.tests.cpp` suite with new parity/divergence coverage for the added audit cases and kept the live diff in advisory-only divergence mode.
- Refreshed `docs/AUDIT-BUGS.md` so the fixed findings above are now marked resolved in the current tree.

## Current State

- Latest current-tree guard-code commit: `e11fe0daa5ec4676a8af36e1acb9ba036818a878`.
- Latest shared-tests commit in `C:\prj\p2p\eMulebb\tests`: `43768d5f178c2ca2841cae1309107836a0cd1921`.
- Latest oracle seam commit: `e4226e72f3ac5baf2a33b4aa94dc28f1d09fd8af`.
- Latest shared-tests commit in `C:\prj\p2p\eMulebb-oracle\tests`: `b0e8bb02b26e164eb4baadc30539e4ce8acbf076`.
- `..\23-build-emule-debug-incremental.cmd` passes on the current workspace after the guard-wiring changes.
- `36-run-emule-tests-debug.cmd` passes on the current workspace with 22 test cases / 40 assertions.
- `37-run-emule-tests-live-diff.cmd` now reports case-level results:
  parity cases pass in both workspaces, and divergence cases pass on dev while failing on oracle as expected.
- The live harness now covers the ring behavior split plus the expanded guard slice:
  TCP payload bounds, hostile tag counts, short UDP headers, compressed UDP body gating, blob remaining-length checks, zero-denominator progress guards, and broadcast IPv4 literal parsing.
- Oracle convergence is still advisory-only in the live diff; unexpected oracle passes warn but do not fail the run.

## Next Chunk

- Extend the shared harness from pure guard helpers into real packet/tag fixture replay so `Packets`, `SafeFile`, and parser entry behavior are exercised with serialized buffers rather than only scalar seams.
- Pull the next deterministic `AUDIT-BUGS.md` backlog into tests, especially the remaining malformed packet and truncated parser paths that do not require UI state, real sockets, or thread scheduling.
- Plan the follow-up low-risk crash-hardening batch around the remaining live `GetCurrentServer()` null dereferences and other small guardable call sites that are still outside the seam coverage.
