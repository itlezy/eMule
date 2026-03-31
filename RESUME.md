# Resume

## Last Chunk

- Completed the first `docs/AUDIT-WWMOD.md` implementation pass for the dead-legacy cleanup bundle.
- Removed dead Win9x and old-MSVC compatibility shims from `srchybrid/stdafx.h` and `srchybrid/Opcodes.h`.
- Removed the no-op `CCM_SETUNICODEFORMAT` calls from the confirmed Unicode common-control initialization sites.
- Deleted the obsolete Win98 icon-limit commentary from `srchybrid/SelfTest.cpp`.
- Rewrote or removed stale Win9x and NT-era comments in active code paths without changing behavior.
- Updated `docs/AUDIT-WWMOD.md` to mark `WWMOD_001` through `WWMOD_005` and `WWMOD_009` fixed, and to mark `WWMOD_046` stale in the current tree.
- Built the target workspace with `..\23-build-emule-debug-incremental.cmd`.

## Current State

- The first WWMOD cleanup tranche is complete and the current branch no longer carries the verified dead compatibility shims covered by this pass.
- `docs/AUDIT-WWMOD.md` now records both the completed cleanup items and the stale taskbar-progress audit entry.
- `WWMOD_007` and `WWMOD_008` remain deferred until their deprecated Winsock and CRT call sites are migrated safely.

## Next Chunk

- Continue with a second WWMOD pass that still has good impact without becoming a broad refactor:
  - either project-file cleanup such as `WWMOD_035`, `WWMOD_038`, and `WWMOD_049`
  - or direct always-present Win10 API cleanup starting with `WWMOD_006`
- Keep the next chunk scoped so it can be built and committed as a single audit-driven block.
