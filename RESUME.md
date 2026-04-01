# Resume

## Last Chunk

- Removed proxy support as a product feature from the live tree:
  - deleted the legacy socket-layer sources [`srchybrid/AsyncSocketExLayer.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncSocketExLayer.cpp) and [`srchybrid/AsyncProxySocketLayer.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncProxySocketLayer.cpp),
  - deleted the proxy preferences page sources [`srchybrid/PPgProxy.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\PPgProxy.cpp) and [`srchybrid/PPgProxy.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\PPgProxy.h),
  - removed proxy persistence from [`srchybrid/Preferences.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Preferences.cpp) and [`srchybrid/Preferences.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Preferences.h),
  - removed proxy-specific connect/error branches from [`srchybrid/EMSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EMSocket.cpp), [`srchybrid/BaseClient.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\BaseClient.cpp), [`srchybrid/ListenSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\ListenSocket.cpp), and [`srchybrid/ServerSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\ServerSocket.cpp),
  - removed the proxy page wiring from [`srchybrid/PreferencesDlg.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\PreferencesDlg.cpp) and [`srchybrid/PreferencesDlg.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\PreferencesDlg.h),
  - removed the proxy dialog resource from [`srchybrid/emule.rc`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.rc) and cleaned stale ids in [`srchybrid/Resource.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Resource.h) and [`srchybrid/HelpIDs.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\HelpIDs.h),
  - removed the obsolete project entries from [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj).
- Added the new header-only regression seam [`srchybrid/SocketPolicySeams.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\SocketPolicySeams.h) and used it to pin the simplified dead-source and verbose-connect policies.
- Added the shared regression file [`src/socket_policy.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\socket_policy.tests.cpp) and wired it into [`emule-tests.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\emule-tests.vcxproj).
- Updated active docs to stop advertising proxy support:
  - [`docs/ARCH-NETWORKING.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\ARCH-NETWORKING.md)
  - [`docs/ARCH-PREFERENCES.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\ARCH-PREFERENCES.md)
  - [`docs/ARCH-THREADING.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\ARCH-THREADING.md)
  - [`docs/INDEX.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\INDEX.md)
  - [`docs/PLAN-CMAKE.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\PLAN-CMAKE.md)
- Fixed the pre-existing `Release|x64` blocker in [`srchybrid/Preferences.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Preferences.h) by making the upload-slot limit definitions visible through `Opcodes.h`.

## Current State

- `Debug|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) builds successfully through `..\23-build-emule-debug-incremental.cmd`.
- `Release|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) now builds successfully with the proxy removal in place.
- The full shared test suite passes:
  - [`emule-tests.exe`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\build\default\x64\Debug\emule-tests.exe) reports `107/107` passing.
  - [`src/socket_policy.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\socket_policy.tests.cpp) reports `2/2` passing.
- Runtime validation is clean after the removal:
  - [`helpers/helper-runtime-wsapoll-smoke.ps1`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\helpers\helper-runtime-wsapoll-smoke.ps1) passed again with artifact [`logs/20260401-193019-wsapoll-smoke`](C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260401-193019-wsapoll-smoke),
  - the `Release|x64` [`emule.exe`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\x64\Release\emule.exe) stayed up under a disposable explicit `-c` profile and was stopped cleanly after the sanity interval.
- The live codebase no longer contains proxy runtime wiring, proxy preferences, proxy UI, or the old socket-layer translation units. Remaining proxy strings in localized `.rc` files are dead resource leftovers only.

## Next Chunk

- Remove the dead localized proxy string entries from `srchybrid/lang/*.rc` if a fully clean resource search is desired.
- Do a longer `WSAPoll` soak or targeted live-traffic replay now that the feature surface is smaller and the release build is green.
- Decide whether to update the historical audit documents (`AUDIT-SECURITY.md`, `AUDIT-WWMOD.md`) to mark their proxy/layer references as removed-history rather than current-state analysis.
