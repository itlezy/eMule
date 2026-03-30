# Resume

## Last Chunk

- Closed `BBUG_013` from `docs/AUDIT-BUGS.md`.
- Changed `srchybrid/ListenSocket.h` / `srchybrid/ListenSocket.cpp` so `CClientReqSocket` no longer deletes itself after the grace timer.
- Kept the existing 10-second delay, but moved final socket destruction into `CListenSocket::Process()` after the socket handle is already closed and the grace period has elapsed.
- Refreshed `docs/AUDIT-BUGS.md` so `BBUG_013` is marked fixed and removed from the deferred lifetime bucket.
- Re-ran `..\23-build-emule-debug-incremental.cmd`; the current wrapper log is `C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260330-170506-build-project-eMule-Debug\eMule-Debug.log`, and it completed successfully.

## Current State

- `docs/AUDIT-BUGS.md` now leaves the remaining ownership/thread-safety backlog at `BBUG_008` through `BBUG_012`, `BBUG_019`, `BBUG_023` through `BBUG_025`, and `BBUG_044`.
- The dependency workspace is still restored on the expected local `emule-build-v0.72a` branches, and the parent debug wrapper continues to pass environment precheck and the `eMule` Debug build.
- The current working tree has the new socket-lifetime edits in:
  - `srchybrid/ListenSocket.h`
  - `srchybrid/ListenSocket.cpp`
  - `docs/AUDIT-BUGS.md`

## Next Chunk

- Continue `docs/AUDIT-BUGS.md` with the still-live `CUpDownClient` / upload-queue lifetime cluster: `BBUG_008`, `BBUG_009`, `BBUG_010`, `BBUG_011`, and `BBUG_012`.
- Decide whether that next slice should remain mechanical and build-verified only, or whether it now warrants shared `eMule-build-tests` coverage because it crosses `TryToConnect`, `PartFile`, and upload I/O paths.
- Commit the new socket-lifetime batch and the matching doc refresh as separate `FIX` and `DOC` commits once this slice is complete.
