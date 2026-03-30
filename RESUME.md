# Resume

## Last Chunk

- Added a new pure serialized parser seam in `srchybrid/ProtocolParsers.h` on the current branch and routed the live packet/string/tag read paths through it in `Packets.cpp` and `SafeFile.cpp`.
- Hardened the current tree against malformed serialized packet headers, truncated explicit tag names, truncated fixed-size tag payloads, truncated bool-array payloads, and truncated blob/string reads before the production code touches those bytes.
- Mirrored the same parser seam into the oracle tree with legacy semantics so the shared live-diff harness can keep surfacing dev-vs-oracle parser behavior splits.
- Extended the shared `protocol.tests.cpp` suite with inline serialized packet/tag fixtures that now exercise real header/tag spans instead of only scalar helper seams.
- Refreshed `docs/AUDIT-BUGS.md` to record that the shared harness now covers serialized packet/tag replay for the guarded parser slice, and updated `helpers/POWERSHELL_MISTAKES.md` with the shell errors from this turn.

## Current State

- Latest current-tree parser-seam commit: `fb4573932fe0018dad11d9c89b14455b827d4f15`.
- Latest shared-tests commit in `C:\prj\p2p\eMule\eMulebb\eMule-build-tests`: `62c164b1a33f2d678e9ca40d8e78293c336a5dea`.
- Latest oracle seam commit: `19a1b7de0c555799ec2742e28bba1fdd1994b660`.
- `C:\prj\p2p\eMule\eMulebb\eMule-build-tests\scripts\build-emule-tests.ps1 -Run` passes on the current workspace with 29 test cases / 60 assertions.
- `C:\prj\p2p\eMule\eMulebb\eMule-build-tests\scripts\run-live-diff.ps1` passes:
  parity cases pass in both workspaces, and the new serialized parser divergence cases pass on dev while failing on oracle as expected.
- The live harness now covers:
  ring behavior, scalar guard seams, serialized packet-header replay, serialized tag-header replay, truncated string/blob tag payloads, and the zero-length packet-header underflow split.
- `..\23-build-emule-debug-incremental.cmd` currently stops during the environment precheck before compilation:
  the script reports missing `C:\prj\p2p\eMule\eMulebb\eMule-build\eMule-zlib\contrib\vstudio\vc\x64\Debug\zlib.lib` plus sibling dependency branch/output mismatches, so the full app build was not revalidated in this turn.

## Next Chunk

- Start the low-risk crash-hardening batch from `docs/AUDIT-BUGS.md`, beginning with the remaining live `GetCurrentServer()` null dereferences in `BaseClient.cpp`, `Emule.cpp`, `PartFile.cpp`, and `SearchResultsWnd.cpp`.
- Add shared regression coverage for that batch by extracting another narrow test seam for connected-server snapshotting/caching rather than trying to compile the MFC UI call sites directly into the standalone test project.
- Re-run the parent build once the sibling dependency environment is back in a passing state, so the parser seam changes and the crash-hardening batch both have a full current-workspace compile check again.
