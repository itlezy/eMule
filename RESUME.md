# Resume

## Last Chunk

- Fixed defect `D-02` in `srchybrid\Emule.cpp`.
- Replaced the startup `srand((unsigned)time(NULL))` seed with a new `GetLegacyRandSeed()` helper.
- `GetLegacyRandSeed()` now seeds the legacy CRT `rand()` state from `BCryptGenRandom(BCRYPT_USE_SYSTEM_PREFERRED_RNG)` and falls back to a mixed process-local value from `QueryPerformanceCounter`, `GetTickCount`, process ID, and thread ID only if the OS RNG call fails.
- This keeps the existing `rand()` call sites untouched while removing the predictable time-based seed from the main application startup path.
- Verified with:
  - `..\23-build-emule-debug-incremental.cmd`
  - confirmed `srchybrid\x64\Debug\Emule.obj` and `srchybrid\x64\Debug\emule.exe` were rebuilt after the edit
- Left the existing unrelated `docs\*.md` worktree churn untouched.

## Current State

- The main application RNG seed is no longer derived from `time(NULL)`.
- The `D-02` fix compiles in the Debug incremental build path.

## Next Chunk

- Review the remaining `srand(time(NULL))` sites outside `Emule.cpp`, especially `WebSocket.cpp` and `WebServer.cpp`, to decide whether the same stronger seeding helper should replace them too.
- Continue the defect sweep for other legacy `rand()` and string-copy call sites that are still using weak or implicit safety contracts.
