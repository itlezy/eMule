# Resume

## Last Chunk

- Replaced the old `WSAAsyncSelect` helper-window backend in [`srchybrid/AsyncSocketEx.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncSocketEx.cpp) with a dedicated `WSAPoll`-driven TCP poller thread.
- Simplified [`srchybrid/AsyncSocketEx.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncSocketEx.h) to the new poll-managed socket model and removed the active helper-window / layer wiring from the implementation surface.
- Hard-disabled proxy layering for the TCP path in [`srchybrid/EMSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EMSocket.cpp) and [`srchybrid/EMSocket.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EMSocket.h); proxy mode now fails explicitly with `WSAEOPNOTSUPP` under the new backend.
- Removed the legacy layer translation units from [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) so the active build only compiles the new TCP backend.
- Cleaned the remaining TCP-side proxy conditionals in [`srchybrid/ListenSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\ListenSocket.cpp), [`srchybrid/ServerConnect.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\ServerConnect.cpp), and [`srchybrid/DownloadClient.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\DownloadClient.cpp).
- Removed the obsolete MFC socket-thread-state setup block from [`srchybrid/Emule.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Emule.cpp) because the new backend no longer uses those private maps/lists.
- Expanded the architecture notes in [`docs/ARCH-THREADING.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\ARCH-THREADING.md) and linked the audit note from [`docs/AUDIT-WWMOD.md`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\docs\AUDIT-WWMOD.md).

## Current State

- `Debug|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) builds successfully with the new `WSAPoll` TCP backend.
- `Release|x64` is currently blocked by an unrelated pre-existing issue in [`srchybrid/Preferences.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Preferences.h) where `MIN_UP_CLIENTS_ALLOWED` and `MAX_UP_CLIENTS_ALLOWED` are undefined; that was not introduced by the socket refactor.
- No shared regression coverage exists yet for the new TCP backend. The test workspace [`C:\prj\p2p\eMule\eMulebb\eMule-build-tests`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests) does not currently contain a socket/poll harness, so the next chunk needs a focused seam or lightweight harness first.
- Runtime validation is still open. The highest-risk areas are callback ordering, close/read draining, event-mask updates racing the poll thread, and socket teardown during in-flight callbacks.

## Next Chunk

- Create the WIP commit for the `WSAPoll` TCP backend cutover before adding more code.
- Add shared regression coverage in [`C:\prj\p2p\eMule\eMulebb\eMule-build-tests`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests) for:
  - connect success and failure,
  - accept burst draining,
  - read-before-close handling,
  - idle `POLLOUT` suppression,
  - close / detach during callback delivery.
- Use the parent build script `..\23-build-emule-debug-incremental.cmd` for the workspace build path expected by this repo.
- After the shared tests exist, run a live `emule.exe -c ...` smoke pass with explicit config root, VPN bind preference, and file logging enabled to catch thread-affinity or lifecycle issues that unit coverage will not expose.
