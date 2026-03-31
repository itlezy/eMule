# Resume

## Last Chunk

- Completed `FEAT_018` by moving connection and download timeouts into persisted preferences, reducing their defaults to `30s` and `75s`, and tightening the remaining fixed UDP/source-latency constants to `20s` and `15000`.
- Completed the active `FEAT_019` exposure work by adding `Connection timeout`, `Download timeout`, and `Per-client upload cap` to `Preferences > Tweaks` while reusing the existing `TCP/IP` and `Broadband` groups.
- Raised the `MaxSourcesPerFile` default to `600`, removed the now-dead timeout/upload-cap compile-time macros from `Opcodes.h`, and kept `KADEMLIAASKTIME`, `ServerKeepAliveTimeout`, and `UPNP_TIMEOUT` unchanged.
- Added the shared `ModernLimits.h` defaults/normalization helper and new shared regression coverage in `eMule-build-tests` for the FEAT_018/019 default values, timeout normalization, and upload-cap math.
- Built the target workspace with `..\23-build-emule-debug-incremental.cmd`.
- Built and ran the shared test suite with `..\36-run-emule-tests-debug.cmd`.

## Current State

- `FEAT_018` is implemented with persisted connection/download timeout defaults and shorter fixed UDP/source-latency constants.
- `FEAT_019` is effectively complete for the active modern-limits knobs; the remaining advanced limit controls stay in their existing Tweaks groups or other existing UI pages.
- `FEAT_017` is still partial because `QueueSize` remains `5000` even though `MaxSourcesPerFile` is now `600`.
- `WWMOD_008`, `WWMOD_019`, and `WWMOD_050` remain available as the next low-risk modernization candidates.

## Next Chunk

- Continue with a similarly scoped modernization block:
  - take `WWMOD_050` next for typed time-conversion helper cleanup, or
  - return to the deferred CRT-hardening pair `WWMOD_019` plus `WWMOD_008`
- Separately, `FEAT_017` still needs the queue-size default increase to `10000` if the modern-limits track continues before the next WWMOD item.
