# Resume

## Last Chunk

- Moved the two standalone Windows maintenance helpers out of `helpers\` into the new `scripts\` subtree.
- Added the new directory policy note in `scripts\AGENTS.md`:
  - all scripts in `scripts\` must stay compatible with Windows built-in `PowerShell.exe` (Windows PowerShell 5.1)
  - all scripts in `scripts\` must work on Windows 10 and Windows 11
  - PowerShell and launcher names in this directory must follow `category-purpose-action.ps1` / `category-purpose-action.cmd`
- Added the new script entrypoints:
  - `scripts\network-firewall-opener.ps1`
  - `scripts\system-long-paths-enabler.ps1`
  - `scripts\network-firewall-opener.cmd`
  - `scripts\system-long-paths-enabler.cmd`
- Removed the old legacy paths:
  - `helpers\firewall-opener.ps1`
  - `helpers\enable-long-paths.ps1`
- Upgraded the firewall helper behavior:
  - still accepts direct command-line parameters
  - auto-detects `emule.exe` under the parent workspace when `-ExePath` is omitted
  - prefers `srchybrid\x64\Debug\emule.exe`, then `srchybrid\x64\Release\emule.exe`
  - prompts for a path only when no repo executable is found
  - keeps firewall operations app-based and `-WhatIf` safe
- Upgraded the long-path helper behavior:
  - added explicit Windows PowerShell 5.1 / Windows 10-11 compatibility notes
  - kept the registry update idempotent and admin-gated
- Updated `README.md` to point at `scripts\network-firewall-opener.cmd` and to document the new auto-detection behavior.
- Verified with Windows built-in `powershell.exe`:
  - `scripts\network-firewall-opener.cmd -WhatIf`
  - `scripts\network-firewall-opener.cmd -WhatIf -Remove`
  - `scripts\system-long-paths-enabler.cmd -WhatIf`
  - direct `powershell.exe -File` invocation for both `.ps1` scripts
- Did not run `..\23-build-emule-debug-incremental.cmd` because this chunk only changed scripts and docs.
- WIP commits created in this chunk so far:
  - `c63893d` `WIP move helper scripts into scripts subtree`
  - `8656e2d` `WIP replace legacy helper script entrypoints`

## Current State

- The repo now has a dedicated `scripts\` subtree for Windows-maintenance scripts with an explicit compatibility rule.
- The preferred user entrypoints are the new `.cmd` wrappers, which force execution through Windows built-in `powershell.exe`.
- `scripts\network-firewall-opener.ps1` auto-discovers the local build output when possible and no longer requires `-ExePath` for the normal repo layout.
- `README.md` no longer points users at the deleted helper paths.
- There are unrelated doc deletions/additions already present in the worktree outside this chunk; they were intentionally left untouched.

## Next Chunk

- If broader docs are stabilized later, sweep remaining old helper-script name mentions in any retained planning docs and rename them to the new `scripts\` paths.
- Resume the earlier functional follow-up from the USS-removal chunk:
  - runtime-smoke the debug build
  - confirm Tweaks and status-bar behavior are still clean after the earlier USS cleanup
