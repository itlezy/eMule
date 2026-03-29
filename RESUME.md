# Resume

## Last Chunk

- Removed the remaining connection presets dialog from Preferences > Connection, the project, and the resource/icon lists.
- Deleted `srchybrid\Wizard.cpp`, `srchybrid\Wizard.h`, and `srchybrid\res\Wizard.ico`.
- Removed the `SetLanguage()` popup that announced fallback to English while keeping language autodetection and silent English fallback intact.
- Verified with `..\23-build-emule-debug-incremental.cmd`.

## Current State

- Fresh installs no longer open a first-run wizard.
- Preferences > Connection now has no wizard/presets helper at all; users tune settings directly on the page.
- Falling back to English no longer shows an informational startup messagebox.
- The build is green, and the only remaining uncommitted changes in the worktree are the unrelated existing `AGENTS.md` and `helpers\POWERSHELL_MISTAKES.md` modifications.

## Next Chunk

- Manually inspect the Connection page layout after removing the bottom-left button and tighten spacing only if the gap looks bad.
- Decide whether to prune the remaining first-run wizard string/dialog resources from `emule.rc` and the language files, or leave them as dead localization baggage for now.
- If desired, replace the `IDS_MB_LANGUAGEINFO` fallback sanity check with a less semantically awkward sentinel string.
