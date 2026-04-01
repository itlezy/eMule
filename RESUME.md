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
- Hardened the runtime validation workflow with [`helpers/helper-runtime-wsapoll-smoke.ps1`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\helpers\helper-runtime-wsapoll-smoke.ps1), including explicit `-c` profile cloning, `hide.me` bind enforcement, sampled socket/window state capture, and failure checks for startup assertion dialogs or zero observed socket activity.
- Fixed the `WSAPoll` bring-up regressions found during live validation:
  - restored the required static-MFC socket thread-state bootstrap in [`srchybrid/Emule.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Emule.cpp) for UDP `CAsyncSocket` startup,
  - corrected [`srchybrid/SharedFileList.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\SharedFileList.cpp) so part files still bypass normal shared-directory checks,
  - corrected the delayed server-handshake send transition in [`srchybrid/EncryptedStreamSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EncryptedStreamSocket.cpp),
  - serialized [`srchybrid/EMSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EMSocket.cpp) receive reassembly to prevent races between the poll thread and legacy download-limit receive pokes.
- Added new seam coverage for the runtime fixes:
  - [`srchybrid/SharedFileListSeams.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\SharedFileListSeams.h) plus updated [`src/shared_file_list.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\shared_file_list.tests.cpp) now cover part-file admission,
  - [`srchybrid/EncryptedStreamSocketSeams.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EncryptedStreamSocketSeams.h) plus [`src/encrypted_stream_socket.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\encrypted_stream_socket.tests.cpp) now cover the delayed-server-send completion rule.

## Current State

- `Debug|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) builds successfully with the new `WSAPoll` TCP backend, both directly and through `..\23-build-emule-debug-incremental.cmd`.
- `Release|x64` is currently blocked by an unrelated pre-existing issue in [`srchybrid/Preferences.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Preferences.h) where `MIN_UP_CLIENTS_ALLOWED` and `MAX_UP_CLIENTS_ALLOWED` are undefined; that was not introduced by the socket refactor.
- Shared regression coverage now exists for the new TCP backend event-classification rules:
  - `emule-tests.exe --source-file='*async_socket_ex.tests.cpp'` passes.
  - `emule-tests.exe --source-file='*shared_file_list.tests.cpp'` passes.
  - `emule-tests.exe --source-file='*encrypted_stream_socket.tests.cpp'` passes.
  - the shared suite also passes with `--source-file-exclude='*mapped_file_reader.tests.cpp'`.
- The remaining shared-suite failure is pre-existing and environment-sensitive in [`src/mapped_file_reader.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\mapped_file_reader.tests.cpp), where recursive enumeration of sampled temp files threw `recursive_directory_iterator::operator++: The system cannot find the path specified.`
- Runtime smoke validation now passes:
  - [`helpers/helper-runtime-wsapoll-smoke.ps1`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\helpers\helper-runtime-wsapoll-smoke.ps1) completes successfully against the debug build with explicit `-c` profile, `hide.me` binding, observed TCP/UDP socket activity, non-empty logs, no crash dumps, no startup failure window, and graceful shutdown.
  - the passing smoke artifact is [`logs/20260401-184019-wsapoll-smoke`](C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260401-184019-wsapoll-smoke).
- The `WSAPoll` TCP backend is now compile-clean, seam-tested, and smoke-validated for the current debug workflow, but it has not yet been stress-tested under heavier concurrency or long-duration live traffic.

## Next Chunk

- Commit the runtime-hardening fixes in granular source and shared-test commits.
- Add deeper regression coverage for execution behavior that is still only smoke-covered:
  - receive/write interleaving around `CEMSocket`,
  - connect / delayed-send lifecycle around `CEncryptedStreamSocket`,
  - close/drain ordering and callback serialization in the poller.
- Run a longer soak or targeted traffic replay against the passing smoke profile to look for CPU spin, stale callback delivery, or teardown races that do not show up in the bounded 90-second run.
- Decide whether to stabilize or quarantine the `mapped_file_reader` sampled-temp-file suite so it stops masking unrelated regressions during full shared-suite runs.
