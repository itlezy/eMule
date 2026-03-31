# Resume

## Last Chunk

- Removed the obsolete `Resolve shell links in shared directories` toggle from `PPgTweaks`.
- Removed the dead `ResolveSharedShellLinks` preference storage and accessor from `CPreferences`.
- Removed the primary English resource string entry for that obsolete toggle.
- Kept the shared resource ID define in `Resource.h` so untranslated language resource files still compile unchanged.
- Verified the cleanup builds successfully with `..\23-build-emule-debug-incremental.cmd`.

## Current State

- `.lnk` files are still ignored unconditionally by the sharing code, which matches the actual runtime behavior before this cleanup.
- There is no longer a misleading preference or UI checkbox suggesting that shell-link resolution can be enabled.
- Localized resource files may still contain unused `IDS_RESOLVELINKS` string entries, but they are no longer referenced by code or the main UI.

## Next Chunk

- If desired, clean up the leftover localized `IDS_RESOLVELINKS` entries across `srchybrid\lang\*.rc` in a dedicated resource-only pass.
- Continue the idle CPU investigation independently of this UI cleanup.
