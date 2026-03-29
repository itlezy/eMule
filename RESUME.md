# Resume

## Last Chunk

- Finished `REFAC_016` from `docs\REFACTOR-TASKS.md`.
- Removed the obsolete `FileBufferSizePref` and `QueueSizePref` compatibility keys from `srchybrid\Preferences.cpp`.
- Simplified preference loading to use the current defaults directly before reading the canonical `FileBufferSize` and `QueueSize` keys.
- Removed the old `ini.DeleteKey(...)` cleanup calls for those deprecated keys from `SavePreferences()`.
- Verified with `..\23-build-emule-debug-incremental.cmd`.

## Current State

- The code no longer reads, migrates, or deletes the legacy `FileBufferSizePref` and `QueueSizePref` keys.
- The canonical preference keys `FileBufferSize` and `QueueSize` remain unchanged.
- The recent one-by-one cleanup chain now covers the Win95 comment-block removal, the fake Windows TCP-limit helper removal, and the legacy INI key removal.

## Next Chunk

- Continue with `REFAC_013`: remove the remaining Source Exchange v1 compatibility branches from the client/source-exchange path.
- After that, revisit the stale docs (`docs\AUDIT-DEADCODE.md` and `docs\REFACTOR-TASKS.md`) so completed chunks stop showing as planned.
