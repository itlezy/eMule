# Resume

## Last Chunk

- Hardened the audited GDI/DC lifetime paths in `MeterIcon.cpp`, `TreeOptionsCtrlEx.cpp`, `HTRichEditCtrl.cpp`, `TitledMenu.cpp`, and `EnBitmap.cpp` by replacing raw desktop-DC ownership with RAII where possible and by consolidating meter-icon cleanup on every early-return path.
- Restored the build workspace through `workspace.ps1 setup` / `build-project`, which moved the third-party deps back onto their local `emule-build-v0.72a` branches, rebuilt the missing zlib debug library, and let `..\23-build-emule-debug-incremental.cmd` run through the `eMule` Debug build again.
- Hardened the remaining low-risk runtime guards in `PartFile.cpp` and `ColourPopup.cpp` by inlining the import-progress zero-denominator fallback, guarding `m_nNumColumns` before division, and sending colour-popup notifications only to a live parent window.
- Hardened the audited UI dialog crash sites in `ArchivePreviewDlg.cpp`, `PPgDirectories.cpp`, and `AddFriend.cpp` by guarding the flagged `GetDlgItem()` dereferences and by validating the archive-preview `CPartFile` downcast once before using part-file-only methods.
- Added the missing explicit `#include "Opcodes.h"` dependency to `srchybrid/MediaInfo_DLL.cpp` so the `SEC2MS` uses in that translation unit no longer depend on transitive headers.
- Re-synced `eMule-zlib` from an ad hoc detached-HEAD fix back onto the canonical local `emule-build-v0.72a` dependency branch at commit `884172c664fd7b92127ebb53968ec04ee8679d41`, matching the already-recorded `zlib-v1.3.2.patch` workflow instead of carrying a one-off checkout-only commit.
- Restored the missing `eMule-zlib/.gitignore` entries for the generated `cmake-build/` tree and the workspace-owned `contrib/vstudio/vc/zlib.vcxproj` wrapper so the zlib submodule stops reporting disposable build noise as untracked content.
- Added a new connected-server snapshot seam in `srchybrid/ServerConnectionGuards.h` on the current branch and routed the live `GetCurrentServer()` call sites through it in `BaseClient.cpp`, `Emule.cpp`, `PartFile.cpp`, `SearchResultsWnd.cpp`, and `DownloadQueue.cpp`.
- Hardened the current tree against null `GetCurrentServer()` snapshots by caching the server pointer once per call site before dereferencing endpoint or capability fields, which fixes the audited `BaseClient`, `Emule`, `PartFile`, and `SearchResultsWnd` TOCTOU/null-deref paths and also cleans up the same pattern in nearby `DownloadQueue`/`PartFile` helpers.
- Mirrored the same seam into the oracle tree with legacy semantics so the shared live-diff harness can keep surfacing the dev-vs-oracle connected-server snapshot split.
- Extended the shared `protocol.tests.cpp` suite with connected-server snapshot parity/divergence coverage, and reran the shared standalone tests plus the live diff successfully.
- Refreshed `docs/AUDIT-BUGS.md` to mark the connected-server snapshot findings as resolved in the current tree, and updated `helpers/POWERSHELL_MISTAKES.md` with the repeated MSBuild state-file collision from an incorrect parallel test-build run.

## Current State

- `docs/AUDIT-BUGS.md` now marks `BBUG_030` through `BBUG_034` as fixed in the current tree after the GDI/DC cleanup pass.
- `docs/AUDIT-BUGS.md` now marks `BBUG_036`, `BBUG_037`, and `BBUG_044` as fixed in the current tree after the latest runtime-guard cleanup.
- `docs/AUDIT-BUGS.md` now marks `BBUG_038` through `BBUG_041` as fixed in the current tree after the dialog/control guard pass.
- `workspace.ps1 setup -Config Debug` restored `eMule-cryptopp`, `eMule-miniupnp`, `eMule-ResizableLib`, and `eMule-zlib` onto their local `emule-build-v0.72a` branches, and `workspace.ps1 build-project -Config Debug -Project zlib` regenerated `eMule-zlib\contrib\vstudio\vc\x64\Debug\zlib.lib`.
- `..\23-build-emule-debug-incremental.cmd` now passes workspace precheck and completes the `eMule` Debug build again; the latest wrapper log is `C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260330-163723-build-project-eMule-Debug\eMule-Debug.log`, ending in `emule.vcxproj -> ...\srchybrid\x64\Debug\emule.exe`.
- `C:\prj\p2p\eMule\eMulebb\eMule-build\eMule-zlib` is back on the intended local dependency branch `emule-build-v0.72a` with the recorded patch commit `884172c664fd7b92127ebb53968ec04ee8679d41`; the detached helper commit `30b8e3f181e037e8be23681e538803215962d75e` is no longer the active checkout.
- `C:\prj\p2p\eMule\eMulebb\eMule-build\eMule-zlib\.gitignore` now matches the workspace patch intent again, so generated `cmake-build/` output and the materialized `contrib\vstudio\vc\zlib.vcxproj` wrapper are ignored instead of surfacing as local noise in the zlib submodule.
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

- Continue the low-risk crash-hardening pass from `docs/AUDIT-BUGS.md` with the remaining small behavioural/runtime findings that still do not require ownership refactors, especially `BBUG_042` in `DownloadQueue.cpp`, the `inet_addr` ambiguity in `ED2KLink.cpp`, and the low-risk string-copy cleanups.
- Decide whether the next seam should stay in the same shared `protocol.tests.cpp` file or split into a second guard-oriented test file before the dialog/control guard cases accumulate further.
- Re-run the parent build once the sibling dependency environment is restored, so the connected-server guard changes and the next crash-hardening batch both get a full current-workspace compile check again.
