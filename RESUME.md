# Resume

## Last Chunk

- Completed the second `docs/AUDIT-WWMOD.md` implementation pass for the isolated project-file cleanup bundle.
- Removed the obsolete `UpgradeFromVC71.props` import from `srchybrid/emule.vcxproj`.
- Removed guaranteed-system `DelayLoadDLLs` entries and the matching `delayimp.lib` linker dependency from all project configurations.
- Set Release `OmitFramePointers` to `false` to improve dump and profiling quality.
- Removed the obsolete RC manifest stanza from `srchybrid/emule.rc` so the project relies on the existing per-platform manifests instead of a missing `res\emule.manifest`.
- Added the direct `Opcodes.h` include to `srchybrid\kademlia\utils\SafeKad.cpp` so the non-PCH unit builds cleanly in Release as well as Debug.
- Updated `docs/AUDIT-WWMOD.md` to mark `WWMOD_035`, `WWMOD_038`, and `WWMOD_049` fixed.
- Built the target workspace with both `..\23-build-emule-debug-incremental.cmd` and `..\22-build-emule-release-incremental.cmd`.

## Current State

- The low-risk WWMOD cleanup work now covers both dead legacy compatibility code and the standalone build-system cleanup items in `emule.vcxproj`.
- The tree no longer depends on the old VC upgrade shim to suppress a stale RC manifest path, and the Release build now covers the non-PCH `SafeKad.cpp` constant use explicitly.
- `docs/AUDIT-WWMOD.md` records `WWMOD_001` through `WWMOD_005`, `WWMOD_009`, `WWMOD_035`, `WWMOD_038`, and `WWMOD_049` as fixed, with `WWMOD_046` marked stale.
- The next easy/high-impact runtime pass is still `WWMOD_006`, while `WWMOD_007`, `WWMOD_008`, and `WWMOD_019` remain coupled to wider call-site remediation.

## Next Chunk

- Continue with the next small but meaningful runtime modernization bundle:
  - start with `WWMOD_006` by replacing dynamic loading for always-present Win10 APIs in `EmuleDlg.cpp`, `Preferences.cpp`, and `Mdump.cpp`
  - keep `WWMOD_021` out of that pass unless the build policy explicitly changes, because it would widen the chunk into a language-standard migration
- Keep the next chunk scoped so it can be built and committed as a single audit-driven block.
