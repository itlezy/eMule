# Resume

## Last Chunk

- Closed `BBUG_010`, `BBUG_011`, and `BBUG_012` from `docs/AUDIT-BUGS.md`.
- Changed `srchybrid/UploadQueue.h` / `srchybrid/UploadQueue.cpp` so upload entries now retire in two phases: the main thread removes them from the live upload list, flushes and detaches the client pointer, clears queued block metadata, and only reclaims the struct later from a retired list once pending overlapped reads are gone.
- Updated `srchybrid/UploadDiskIOThread.cpp` so `StartCreateNextBlockPackage()` treats retired upload entries as inert, tracks outstanding overlapped reads per upload struct, and only releases that read-count after the completion callback has finished touching the struct.
- Refreshed `docs/AUDIT-BUGS.md` so `BBUG_010`, `BBUG_011`, and `BBUG_012` are marked fixed and removed from the deferred lifetime bucket.
- Re-ran `..\23-build-emule-debug-incremental.cmd`; the current wrapper log is `C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260330-173435-build-project-eMule-Debug\eMule-Debug.log`, and it completed successfully.

## Current State

- `docs/AUDIT-BUGS.md` now leaves the remaining ownership/thread-safety backlog at `BBUG_019`, `BBUG_023` through `BBUG_025`, and `BBUG_044`.
- The dependency workspace is still restored on the expected local `emule-build-v0.72a` branches, and the parent debug wrapper continues to pass environment precheck and the `eMule` Debug build.
- The current working tree has the upload-queue lifetime edits in:
  - `srchybrid/UploadQueue.h`
  - `srchybrid/UploadQueue.cpp`
  - `srchybrid/UploadDiskIOThread.cpp`
  - `docs/AUDIT-BUGS.md`

## Next Chunk

- Continue `docs/AUDIT-BUGS.md` with the remaining ownership/thread-safety backlog: `BBUG_019`, `BBUG_023`, `BBUG_024`, and `BBUG_025`.
- Re-check whether `BBUG_044` should stay in the deferred bucket now that the parent-window notification guard landed earlier, and drop it if the backlog line is now just stale carry-over.
- Commit the upload-queue lifetime batch and the matching doc refresh as separate `FIX` and `DOC` commits once this slice is complete.
