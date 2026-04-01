# Resume

## Last Chunk

- Hardened the post-migration runtime after live repro work:
  - marshaled `CPartFile` and `CUpDownClient` display refreshes back to the UI thread through new `UM_PARTFILE_DISPLAY_UPDATE` and `UM_CLIENT_DISPLAY_UPDATE` messages,
  - removed direct list-control refreshes from the socket/upload/listen paths that could run off the `WSAPoll` thread,
  - reproduced the crash-recovery `PS_HASHING` stall in the disposable profile at `C:\tmp\emule-testing`,
  - fixed the startup ordering so crash-recovery part-file hashing is queued before `CAICHSyncThread` starts its background AICH backfill batch,
  - added the bounded live-session helper [`helpers/helper-runtime-pipe-live-session.ps1`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\helpers\helper-runtime-pipe-live-session.ps1) for named-pipe-driven sessions with dump/assert capture.

## Current State

- `Debug|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) builds successfully through `..\23-build-emule-debug-incremental.cmd`.
- `Release|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) builds successfully through `..\22-build-emule-release-incremental.cmd`.
- The full shared test suite passes:
  - [`emule-tests.exe`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\build\eMule-build\x64\Debug\emule-tests.exe) reports `111/111` passing,
  - the focused hostname resolver seam batch passes `2/2`.
- Runtime validation is clean after the full socket migration:
  - [`helpers/helper-runtime-wsapoll-smoke.ps1`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\helpers\helper-runtime-wsapoll-smoke.ps1) passed with the latest artifact [`logs/20260401-231931-wsapoll-smoke`](C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260401-231931-wsapoll-smoke).
- Live repro work confirmed and fixed two runtime regressions on the `WSAPoll` branch:
  - the poll/network thread no longer performs synchronous transfer-list UI work,
  - crash-recovery part-file rehashing now starts before background AICH sync hashing claims the global hash lane.
- Live socket ownership remains fully off `CAsyncSocket`, and there are no remaining live
  `WSAAsyncGetHostByName` usages under [`srchybrid`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid).
- Socket migration and closeout are complete enough for normal development to move on.
- The only remaining socket-side work is optional stress/soak hardening of the current `WSAPoll` backend.

## Next Chunk

- Add focused shared-test coverage for:
  - queued display-refresh routing on the UI thread,
  - crash-recovery part-file hashing priority over background AICH sync at startup.
- Re-run the named-pipe live session helper with heavier search/download churn once the focused regression tests are in place.
- Treat socket migration itself as closed; remaining work is runtime hardening and test coverage, not more transport replacement.
