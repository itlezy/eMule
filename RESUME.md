# Resume

## Last Chunk

- Completed the `WWMOD_006` runtime API cleanup pass.
- Replaced the remaining dynamic `GetProcAddress` and `LoadLibrary` paths for `ChangeWindowMessageFilter`, `DwmGetColorizationColor`, `SHGetKnownFolderPath`, `DwmIsCompositionEnabled`, and `MiniDumpWriteDump` with direct calls.
- Added the direct import libraries required by that pass in `srchybrid/emule.vcxproj`.
- Removed the obsolete `CMiniDumper::GetDebugHelperDll` compatibility helper.
- Updated `docs/AUDIT-WWMOD.md` to mark `WWMOD_006` fixed.
- Built the target workspace with both `..\23-build-emule-debug-incremental.cmd` and `..\22-build-emule-release-incremental.cmd`.

## Current State

- The tree now uses direct imports for the Win10-guaranteed APIs covered by `WWMOD_006`, and the old dynamic-loading shims are gone.
- `docs/AUDIT-WWMOD.md` records `WWMOD_001` through `WWMOD_006`, `WWMOD_009`, `WWMOD_035`, `WWMOD_038`, and `WWMOD_049` as fixed, with `WWMOD_046` marked stale.
- `WWMOD_007`, `WWMOD_008`, `WWMOD_019`, and `WWMOD_030` are still linked and should be handled together in a call-site modernization pass.

## Next Chunk

- Continue with the next small but meaningful safety-oriented cleanup bundle:
  - start with `WWMOD_019` and the coupled suppression removals in `WWMOD_008`
  - keep the deprecated Winsock cleanup (`WWMOD_030` and `WWMOD_007`) as a separate follow-up unless the touched files overlap enough to justify combining them
- Keep the next chunk scoped so it can be built and committed as a single audit-driven block.
