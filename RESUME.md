# Resume

## Last Chunk

- Closed `BBUG_008` and `BBUG_009` from `docs/AUDIT-BUGS.md`.
- Changed `srchybrid/UpdownClient.h` / `srchybrid/BaseClient.cpp` so `TryToConnect()` no longer deletes the client in its immediate-failure branches and now documents the caller-owned liveness contract explicitly.
- Updated the affected seams so the legacy convenience wrappers keep their prior behavior where needed (`RequestSharedFileList`, `SafeConnectAndSendPacket`, and the URL redirect reconnect path), while `AskForDownload()` / `PartFile.cpp` now delete the failed source explicitly at the iteration site.
- Updated `srchybrid/UploadQueue.cpp` so the upload-list admission path explicitly deletes a client when `TryToConnect(true)` reports failure.
- Refreshed `docs/AUDIT-BUGS.md` so `BBUG_008` and `BBUG_009` are marked fixed and removed from the deferred lifetime bucket.
- Re-ran `..\23-build-emule-debug-incremental.cmd`; the current wrapper log is `C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260330-171558-build-project-eMule-Debug\eMule-Debug.log`, and it completed successfully.

## Current State

- `docs/AUDIT-BUGS.md` now leaves the remaining ownership/thread-safety backlog at `BBUG_010`, `BBUG_011`, `BBUG_012`, `BBUG_019`, `BBUG_023` through `BBUG_025`, and `BBUG_044`.
- The dependency workspace is still restored on the expected local `emule-build-v0.72a` branches, and the parent debug wrapper continues to pass environment precheck and the `eMule` Debug build.
- The current working tree has the new `TryToConnect` lifetime edits in:
  - `srchybrid/UpdownClient.h`
  - `srchybrid/BaseClient.cpp`
  - `srchybrid/DownloadClient.cpp`
  - `srchybrid/PartFile.cpp`
  - `srchybrid/UploadQueue.cpp`
  - `srchybrid/URLClient.cpp`
  - `docs/AUDIT-BUGS.md`

## Next Chunk

- Continue `docs/AUDIT-BUGS.md` with the remaining upload-queue / disk-thread lifetime cluster: `BBUG_010`, `BBUG_011`, and `BBUG_012`.
- Decide whether that next slice should stay build-verified only or add targeted shared `eMule-build-tests` coverage if a seam can be extracted cheaply around upload-struct ownership.
- Commit the new `TryToConnect` lifetime batch and the matching doc refresh as separate `FIX` and `DOC` commits once this slice is complete.
