# Resume

## Last Chunk

- Vendored `nlohmann/json.hpp` into `srchybrid\nlohmann\json.hpp` instead of keeping it as a sibling dependency.
- Added a first real named-pipe backend in `srchybrid\PipeApiServer.{h,cpp}`:
  - pipe server at `\\.\pipe\emule-api`
  - JSON-lines request/response/event framing
  - UI-thread command dispatch through `UM_PIPE_API_COMMAND`
  - v1 commands for `system/version`, `system/stats`, `downloads/list|get|sources|add|pause|resume|stop|delete|recheck`, and `log/get`
  - event emission for `stats_updated`, `download_added`, `download_removed`, `download_updated`, `download_completed`, and `download_error`
- Wired the pipe lifecycle into `CemuleDlg` startup/shutdown and into existing add/remove/completion/transfer-rate hooks.
- Added a bounded in-memory recent log buffer in `Log.{h,cpp}` so the remote API can expose logs without scraping the rich edit controls.
- Updated `srchybrid\emule.vcxproj` to compile the pipe server and include the vendored JSON header in the project.
- Built the eMule workspace successfully with `..\23-build-emule-debug-incremental.cmd`.
- Created the new sibling Node + Svelte project at `C:\prj\p2p\eMule\eMulebb\eMule-remote`.
- Implemented the first standalone `eMule-remote` slice:
  - Fastify HTTP server
  - named-pipe client with reconnect
  - bearer-token API for external clients
  - cookie-backed same-origin UI access for the built-in web UI
  - REST endpoints for the current v1 command set
  - SSE forwarding at `/api/v1/events`
  - basic Svelte/Vite dashboard for stats, downloads, sources, add-link, and logs
- Fixed the sidecar bootstrap path so missing pipes no longer crash the process with an unhandled EventEmitter `error`.
- Installed dependencies in `C:\prj\p2p\eMule\eMulebb\eMule-remote` and verified `npm run build` passes.
- Smoke-tested the sidecar process with `/health`; it now stays up and reports `{"ok":true,"pipeConnected":false}` when eMule is not running.

## Current State

- The eMule core now exposes a local named-pipe API server, but it has only been compile-verified so far; it still needs live end-to-end exercising against a running `emule.exe`.
- The implemented C++ API surface is intentionally narrow and download-centric. There is no search, no Kad/server management API, no settings writes, and no shutdown command.
- The delete semantics are intentionally strict:
  - completed download deletion currently requires `deleteFiles=true`
  - partial download deletion currently requires `deleteFiles=true`
- The recent-log API is backed by an in-memory ring buffer, not persisted storage.
- `C:\prj\p2p\eMule\eMulebb\eMule-remote` is now a standalone Node app, not a workspace/monorepo.
- The web UI is basic but functional: live status, queue table, source panel, link submission, and recent logs.
- The sidecar is resilient when the named pipe does not exist yet and keeps retrying in the background.

## Next Chunk

- Run the new sidecar against a live `emule.exe` session and verify:
  - pipe connect/disconnect
  - `/api/v1/system/*`
  - `/api/v1/downloads*`
  - SSE updates during add/pause/resume/stop/completion
- Fix any runtime mismatches between the current pipe payloads and the Svelte UI assumptions; this has only been build-validated, not exercised against live transfer data yet.
- Add at least a small automated backend test layer for `eMule-remote`:
  - pipe-response parsing
  - auth guard behavior
  - HTTP error mapping
- Decide whether `eMule-remote` should become its own git repo or stay as an unversioned sibling project before doing cleanup commits there.
- If the API surface expands next, keep new eMule-specific areas under the same `/api/v1/...` style instead of drifting back toward qBittorrent compatibility naming.
