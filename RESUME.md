# Resume

## Last Chunk

- Moved the remaining live UDP socket ownership off `CAsyncSocket` and onto the shared `WSAPoll` backend:
  - added the new datagram poll façade [`srchybrid/AsyncDatagramSocket.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncDatagramSocket.h) and [`srchybrid/AsyncDatagramSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncDatagramSocket.cpp),
  - extended [`srchybrid/AsyncSocketEx.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncSocketEx.h) and [`srchybrid/AsyncSocketEx.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncSocketEx.cpp) with datagram `recvfrom` / `sendto` support,
  - refactored [`srchybrid/ClientUDPSocket.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\ClientUDPSocket.h) and [`srchybrid/ClientUDPSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\ClientUDPSocket.cpp) to use poll-driven readiness with main-thread dispatch and explicit writable-interest gating,
  - refactored [`srchybrid/UDPSocket.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\UDPSocket.h) and [`srchybrid/UDPSocket.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\UDPSocket.cpp) the same way and removed the server UDP hidden DNS window plus `WSAAsyncGetHostByName`,
  - added the app-thread UDP dispatch message in [`srchybrid/UserMsgs.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\UserMsgs.h), [`srchybrid/EmuleDlg.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EmuleDlg.h), and [`srchybrid/EmuleDlg.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\EmuleDlg.cpp),
  - removed the last static-MFC socket bootstrap block from [`srchybrid/Emule.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\Emule.cpp),
  - updated the project file [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) for the new transport unit.
- Added the new datagram seam header [`srchybrid/AsyncDatagramSocketSeams.h`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\AsyncDatagramSocketSeams.h) and the shared regression file [`src/async_datagram_socket.tests.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\src\async_datagram_socket.tests.cpp), wired through [`emule-tests.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\emule-tests.vcxproj).

## Current State

- `Debug|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) builds successfully through `..\23-build-emule-debug-incremental.cmd`.
- `Release|x64` for [`srchybrid/emule.vcxproj`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\emule.vcxproj) still builds successfully after the UDP cutover.
- The full shared test suite passes:
  - [`emule-tests.exe`](C:\prj\p2p\eMule\eMulebb\eMule-build-tests\build\default\x64\Debug\emule-tests.exe) reports `109/109` passing,
  - the focused socket seam batch (`async_socket_ex` + `async_datagram_socket`) passes `9/9`.
- Runtime validation is clean after the UDP cutover:
  - [`helpers/helper-runtime-wsapoll-smoke.ps1`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\helpers\helper-runtime-wsapoll-smoke.ps1) passed with artifact [`logs/20260401-215848-wsapoll-smoke`](C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260401-215848-wsapoll-smoke).
- Live socket ownership is now fully off `CAsyncSocket`. The remaining legacy Winsock async hostname path is not a socket owner: [`srchybrid/DownloadQueue.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\DownloadQueue.cpp) still uses `WSAAsyncGetHostByName` for unresolved source hostnames.

## Next Chunk

- Decide whether to migrate the unresolved-source hostname path in [`srchybrid/DownloadQueue.cpp`](C:\prj\p2p\eMule\eMulebb\eMule-build\eMule\srchybrid\DownloadQueue.cpp) off `WSAAsyncGetHostByName` as part of the broader networking cleanup.
- Do a longer UDP-focused `WSAPoll` soak or live-traffic replay to exercise server DNS resolution, client UDP receive bursts, and writable backpressure.
- If deeper hardening is needed, add execution-level seams or targeted integration helpers for the server UDP DNS queue lifecycle rather than broad new transport refactors.
