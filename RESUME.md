# Resume

## Last Chunk

- Added `srchybrid/ProtocolGuards.h` on the current branch and routed the live parser guards through it for packet payload sizing, TCP receive-buffer bounds, sane tag counts, and server UDP header-length checks.
- Added the matching oracle-side `srchybrid/ProtocolGuards.h` seam on `v0.72a-oracle` so the shared tests can compile the same guard API against both workspaces while preserving the legacy oracle semantics.
- Expanded the shared `tests` repo into parity and divergence suites, switched the live diff from raw text comparison to doctest XML parsing, and stabilized the shared runner with `/FS` plus stricter suite filtering.

## Current State

- `C:\prj\p2p\eMulebb\tests` and `C:\prj\p2p\eMulebb-oracle\tests` are both on shared-tests commit `9ece132b3525aae3ad315099490a4b59af1808b7`.
- The current `eMule` tree is on `d29e8629455b66a19c6df15afdc055e7c963f640`, and the oracle `eMule` tree is on `3b5147acf0dc2ac8edb0e58c6aab7e71ddc5fd79`.
- `36-run-emule-tests-debug.cmd` passes on the current workspace with 12 test cases / 28 assertions.
- `37-run-emule-tests-live-diff.cmd` now reports case-level results:
  parity cases pass in both workspaces, and divergence cases pass on dev while failing on oracle as expected.
- The live harness now covers the ring behavior split plus the first parser-guard slice:
  TCP payload length gates, hostile tag counts, and short server UDP payload handling.

## Next Chunk

- Extend the guard-level protocol coverage into real packet and tag fixture replay so more of `Packets`, `SafeFile`, and parser entry behavior is exercised directly rather than only through the seam helpers.
- Decide whether to keep the divergence lane as advisory-only or begin failing on unexpected oracle convergence once the expected-difference set stabilizes.
- Add the next audit-driven cases from `AUDIT-BUGS.md`, especially the remaining malformed packet and truncated parser paths that can be expressed deterministically in the shared harness.
