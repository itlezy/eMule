# Resume

## Last Chunk

- Completed `WWMOD_034` by routing whole-file hashing, part verification, and AICH recovery through a shared memory-mapped file-range reader.
- Replaced the file-backed `FILE*` hashing path in `CKnownFile` with direct long-path Win32 handles so hashing can use `CreateFileMapping` and `MapViewOfFile`.
- Added the standalone `MappedFileReader` helper and wired it into both the app project and the shared test project.
- Added shared regression coverage in `eMule-build-tests` to verify exact byte-range replay across allocation-granularity boundaries, multi-window reads, zero-length reads, and invalid-handle failures.
- Extended the shared tests with actual-file full-pipeline MD4 and AICH parity coverage plus sampled `C:\tmp` benchmark reporting against both the dev and oracle workspaces.
- Updated `docs/AUDIT-WWMOD.md` to mark `WWMOD_034` fixed.
- Built the target workspace with both `..\23-build-emule-debug-incremental.cmd` and `..\22-build-emule-release-incremental.cmd`.
- Built and ran the shared test suite with `..\..\eMule-build-tests\scripts\build-emule-tests.ps1 -WorkspaceRoot .. -Configuration Debug -Platform x64 -Run`.

## Current State

- The tree now uses memory-mapped reads for the active large-file hashing paths and has shared regression coverage for both the mapped range reader and the full eMule hashing artifacts in `eMule-build-tests`.
- `docs/AUDIT-WWMOD.md` records `WWMOD_001` through `WWMOD_007`, `WWMOD_009`, `WWMOD_021`, `WWMOD_030`, `WWMOD_034`, `WWMOD_035`, `WWMOD_038`, and `WWMOD_049` as fixed, with `WWMOD_046` marked stale.
- `WWMOD_008` and `WWMOD_019` remain the deferred CRT-hardening pair.

## Next Chunk

- Continue with the next dead-code or low-risk modernization bundle:
  - take `WWMOD_050` next for the typed time-conversion helper cleanup, or
  - return to the deferred CRT work with `WWMOD_019` plus `WWMOD_008` when ready for the broader call-site sweep
- Keep the next chunk scoped so it can be built and committed as a single audit-driven block.
