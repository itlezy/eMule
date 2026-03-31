# Resume

## Last Chunk

- Added a startup-only `-c <base-dir>` override which points eMule at an explicit config base directory.
- The override is parsed before any startup profile reads, so `preferences.ini`, `IgnoreInstances`, `Port`, and mutex naming all come from `<base-dir>\config\preferences.ini` when `-c` is present.
- `Preferences.cpp` now treats that base dir as authoritative for `EMULE_CONFIGBASEDIR`, `EMULE_CONFIGDIR`, and `EMULE_LOGDIR`, while leaving incoming/temp and the rest of the directory model unchanged.
- Added shared regression coverage in `eMule-build-tests` for the `-c` parser and path normalization.

## Current State

- Startup config resolution is now explicit when `-c` is supplied and no longer depends on registry or auto-detected config roots for the config/log side of the profile.
- Invalid `-c` usage fails fast instead of silently falling back to another config location.
- Shared tests cover supported and rejected `-c` forms.

## Next Chunk

- If needed, do a live manual smoke run of `emule.exe -c <base-dir>` against a prepared alternate profile root to confirm the override on a real launch.
- Continue the hashing hot-loop investigation from the existing non-UI full-hash probe baseline once the config-root ambiguity is out of the way.
