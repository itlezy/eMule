# Resume

## Last Chunk

- Added a new connected-server snapshot seam in `srchybrid/ServerConnectionGuards.h` on the current branch and routed the live `GetCurrentServer()` call sites through it in `BaseClient.cpp`, `Emule.cpp`, `PartFile.cpp`, `SearchResultsWnd.cpp`, and `DownloadQueue.cpp`.
- Hardened the current tree against null `GetCurrentServer()` snapshots by caching the server pointer once per call site before dereferencing endpoint or capability fields, which fixes the audited `BaseClient`, `Emule`, `PartFile`, and `SearchResultsWnd` TOCTOU/null-deref paths and also cleans up the same pattern in nearby `DownloadQueue`/`PartFile` helpers.
- Mirrored the same seam into the oracle tree with legacy semantics so the shared live-diff harness can keep surfacing the dev-vs-oracle connected-server snapshot split.
- Extended the shared `protocol.tests.cpp` suite with connected-server snapshot parity/divergence coverage, and reran the shared standalone tests plus the live diff successfully.
- Refreshed `docs/AUDIT-BUGS.md` to mark the connected-server snapshot findings as resolved in the current tree, and updated `helpers/POWERSHELL_MISTAKES.md` with the repeated MSBuild state-file collision from an incorrect parallel test-build run.

## Current State

- Latest current-tree connected-server guard commit: `e2578dafe11a96b0623ea8214c07c4fc12d06427`.
- Latest shared-tests commit in `C:\prj\p2p\eMule\eMulebb\eMule-build-tests`: `39211de9761f8b6b3340399b8c7aa6e6d753373a`.
- Latest oracle seam commit: `898d419d6ce57184f4b35a426f2c400751952046`.
- `C:\prj\p2p\eMule\eMulebb\eMule-build-tests\scripts\build-emule-tests.ps1 -Run` passes on the current workspace with 33 test cases / 66 assertions after the connected-server seam coverage was added.
- `C:\prj\p2p\eMule\eMulebb\eMule-build-tests\scripts\run-live-diff.ps1` passes:
  parity cases pass in both workspaces, and the new connected-server divergence cases pass on dev while failing on oracle as expected.
- The live harness now covers:
  ring behavior, scalar guard seams, serialized packet-header replay, serialized tag-header replay, truncated string/blob tag payloads, zero-length packet-header underflow, and the connected-server snapshot null-guard split.
- `..\23-build-emule-debug-incremental.cmd` still stops during the environment precheck before compilation:
  the script reports missing `C:\prj\p2p\eMule\eMulebb\eMule-build\eMule-zlib\contrib\vstudio\vc\x64\Debug\zlib.lib` plus sibling dependency branch/output mismatches, so the full app build was not revalidated in this turn either.

## Next Chunk

- Continue the low-risk crash-hardening pass from `docs/AUDIT-BUGS.md` with the remaining UI-only null/guard findings that do not require ownership refactors, especially `GetDlgItem()` dereferences in `ArchivePreviewDlg.cpp`, `PPgDirectories.cpp`, and `AddFriend.cpp`.
- Decide whether the next seam should stay in the same shared `protocol.tests.cpp` file or split into a second guard-oriented test file before the dialog/control guard cases accumulate further.
- Re-run the parent build once the sibling dependency environment is restored, so the connected-server guard changes and the next crash-hardening batch both get a full current-workspace compile check again.
