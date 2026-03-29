# Resume

## Last Chunk

- Removed the obsolete `GetMaxWindowsTCPConnections()` helper and its dead UI warning path.
- Simplified `CPreferences::GetRecommendedMaxConnections()` to return the explicit eMule-side default of `500`.
- Removed the old "OS supports" max-connections warning logic from `srchybrid\PPgConnection.cpp`.
- Verified with `..\23-build-emule-debug-incremental.cmd`.

## Current State

- The code no longer pretends to query a Windows TCP connection limit on modern systems.
- The `MaxConnections` preference still exists unchanged, but its default is now clearly app policy rather than a fake OS-derived value.
- The old warning resource strings remain in resources only; the runtime code path using them is gone.

## Next Chunk

- Continue the one-by-one audit queue with `REFAC_016`: remove the obsolete `FileBufferSizePref` and `QueueSizePref` compatibility reads from `Preferences.cpp`.
- After that, the next larger pending audit item remains `REFAC_013`, the Source Exchange v1 branch removal.
