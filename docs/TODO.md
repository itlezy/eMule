# TODO

## TODO-20260329-FIRSTSTART-SOCKET-REWORK

- Replace the first-run wizard and redesign the first-start initialization flow.
- Current known issue and temporary workaround:
  - on a fresh config, the first-time wizard can pre-open the TCP and UDP sockets via `Rebind()`
  - later, the normal startup phase tries to create those same sockets again
  - that double-create path was the cause of the debug assertions in `srchybrid/AsyncSocketEx.cpp` and MFC `sockcore.cpp`
  - the current fix makes same-port second creation a no-op in `srchybrid/ListenSocket.cpp` and `srchybrid/ClientUDPSocket.cpp`
- Follow-up goal:
  - remove the startup wizard entirely
  - replace it with a single, non-duplicated first-run initialization path that owns port selection, socket startup, and first-run defaults deterministically
