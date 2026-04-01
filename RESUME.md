# Resume

## Last Chunk

- Replaced the old `WSAAsyncSelect` helper-window backend in [`srchybrid/AsyncSocketEx.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncSocketEx.cpp) with a dedicated `WSAPoll`-driven TCP poller thread.
- Simplified [`srchybrid/AsyncSocketEx.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncSocketEx.h) to the new poll-managed socket model and removed the active helper-window / layer wiring from the implementation surface.
- Hard-disabled proxy layering for the TCP path in [`srchybrid/EMSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EMSocket.cpp) and [`srchybrid/EMSocket.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EMSocket.h); proxy mode now fails explicitly with `WSAEOPNOTSUPP` under the new backend.
- Removed the legacy layer translation units from [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) so the active build only compiles the new TCP backend.
- Cleaned the remaining TCP-side proxy conditionals in [`srchybrid/ListenSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\ListenSocket.cpp), [`srchybrid/ServerConnect.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\ServerConnect.cpp), and [`srchybrid/DownloadClient.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\DownloadClient.cpp).
- Removed the obsolete MFC socket-thread-state setup block from [`srchybrid/Emule.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Emule.cpp) because the new backend no longer uses those private maps/lists.
- Expanded the architecture notes in [`docs/ARCH-THREADING.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\ARCH-THREADING.md) and linked the audit note from [`docs/AUDIT-WWMOD.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\AUDIT-WWMOD.md).
- Added the header-only seam [`srchybrid/AsyncSocketExSeams.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncSocketExSeams.h) to pin the `WSAPoll` event-selection and close-classification rules in unit tests.
- Added the first shared backend regression file [`src/async_socket_ex.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\async_socket_ex.tests.cpp) and wired it into [`emule-tests.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\emule-tests.vcxproj).

## Current State

- `Debug|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) builds successfully with the new `WSAPoll` TCP backend, both directly and through `..\23-build-emule-debug-incremental.cmd`.
- `Release|x64` is currently blocked by an unrelated pre-existing issue in [`srchybrid/Preferences.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Preferences.h) where `MIN_UP_CLIENTS_ALLOWED` and `MAX_UP_CLIENTS_ALLOWED` are undefined; that was not introduced by the socket refactor.
- Shared regression coverage now exists for the new TCP backend event-classification rules:
  - `emule-tests.exe --source-file='*async_socket_ex.tests.cpp'` passes.
  - the shared suite also passes with `--source-file-exclude='*mapped_file_reader.tests.cpp'`.
- The remaining shared-suite failure is pre-existing and environment-sensitive in [`src/mapped_file_reader.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\mapped_file_reader.tests.cpp), where recursive enumeration of sampled temp files threw `recursive_directory_iterator::operator++: The system cannot find the path specified.`
- Runtime validation is still open. The highest-risk areas are callback ordering, close/read draining, event-mask updates racing the poll thread, and socket teardown during in-flight callbacks.

## Next Chunk

- Commit the socket seam and the first shared regression batch as a separate WIP/FIX block.
- Broaden shared coverage beyond pure classification into connect retry, accept burst, and close/drain execution seams without binding the tests to the full UI runtime.
- Run a live `emule.exe -c ...` smoke pass with explicit config root, VPN bind preference, and file logging enabled to catch thread-affinity or lifecycle issues that unit coverage will not expose.
- Decide whether to stabilize or quarantine the `mapped_file_reader` sampled-temp-file suite so it stops masking unrelated regressions during full shared-suite runs.
