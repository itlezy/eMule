# Resume

## Last Chunk

- Marked [`docs/PLAN-API-SERVER.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\PLAN-API-SERVER.md) as the implemented canonical contract for the local pipe and grouped `/api/v2/...` sidecar surface.
- Expanded the local pipe server in [`srchybrid/PipeApiServer.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\PipeApiServer.cpp) with:
  - broader curated `app/preferences/get|set` coverage,
  - strict rejection of unsupported mutable preference keys,
  - `shared/add` and `shared/remove`,
  - `uploads/remove` and `uploads/release_slot`.
- Added stable seam helpers in [`srchybrid/PipeApiSurfaceSeams.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\PipeApiSurfaceSeams.h) for the expanded mutable-preference vocabulary and shared-file removal policy.
- Added the missing preference setters in [`srchybrid/Preferences.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Preferences.h) needed by the pipe API for upload-slot and queue sizing.
- Added regression coverage in [`pipe_api.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\pipe_api.tests.cpp) for the expanded preference vocabulary and shared-file removal guard.

## Current State

- The pipe surface now covers:
  - `app/*`
  - `stats/*`
  - `transfers/*`
  - `uploads/*`
  - `servers/*`
  - `kad/*`
  - `shared/*`
  - `search/*`
  - `log/*`
- The canonical contract and the implementation now both include share-management mutations and minimal upload queue / slot controls.
- Builds currently pass with `..\23-build-emule-debug-incremental.cmd`.
- Shared tests currently pass with:
  - `MSBuild C:\prj\p2p\eMule\eMulebb\eMule-build-tests\emule-tests.vcxproj /p:Configuration=Debug /p:Platform=x64 /m`
  - `C:\prj\p2p\eMule\eMulebb\eMule-build-tests\build\default\x64\Debug\emule-tests.exe --source-file='*pipe_api.tests.cpp'`

## Next Chunk

- Run a live `eMule-remote` smoke pass against the expanded pipe surface, especially:
  - `shared/add`
  - `shared/remove`
  - `uploads/remove`
  - `uploads/release_slot`
  - expanded `app/preferences/set`
- Decide whether the remote HTTP layer should expose the new share/upload mutations immediately or hold them until a UI pass is ready.
- If the API is considered stable after live smoke coverage, close out the API refactor and shift focus back to runtime stability review only.
