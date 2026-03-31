# Resume

## Last Chunk

- Added a persisted `EnablePipeApiServer` preference under the `Remote` INI section to control the local named-pipe API server lifecycle.
- Exposed the toggle in `Tweaks -> Startup and session` as `Enable local pipe API server`.
- Gated `CemuleDlg` startup so the pipe server only starts when that preference is enabled.
- Applied the toggle live from `PPgTweaks` so enabling starts the server immediately and disabling stops it immediately without restarting eMule.
- Set the default for clean configurations to `false` so the pipe server stays off unless explicitly enabled.
- Added the new UI string and resource ID for the tweak entry.
- Verified the change builds successfully with `..\23-build-emule-debug-incremental.cmd`.

## Current State

- Existing configs keep their saved `EnablePipeApiServer` value.
- New configs default to the pipe server being disabled.
- The pipe server still stops unconditionally during shutdown; `Stop()` remains safe when it was never started.
- No dedicated shared regression test was added because this change is preference/UI wiring rather than a core serialization or protocol change.

## Next Chunk

- Manually verify the new toggle in the Tweaks UI against `eMule-remote`:
  - disabled => remote cannot connect to the pipe
  - enabled => remote reconnects and `/health` reports `pipeConnected: true`
- Continue the idle CPU investigation independently of the pipe toggle, because earlier probes showed the baseline `emule.exe` CPU burn also happens without the remote sidecar.
