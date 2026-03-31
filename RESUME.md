# Resume

## Last Chunk

- Added a startup-only `-c <base-dir>` override which points eMule at an explicit config base directory.
- The override is parsed before any startup profile reads, so `preferences.ini`, `IgnoreInstances`, `Port`, and mutex naming all come from `<base-dir>\config\preferences.ini` when `-c` is present.
- `Preferences.cpp` now treats that base dir as authoritative for `EMULE_CONFIGBASEDIR`, `EMULE_CONFIGDIR`, and `EMULE_LOGDIR`, while leaving incoming/temp and the rest of the directory model unchanged.
- Added shared regression coverage in `eMule-build-tests` for the `-c` parser and path normalization.
- Added explicit `@todo` comments in `CemuleApp::ProcessCommandline()` around the legacy port-mutex and WM_COPYDATA shell-forwarding branches to mark them as future cleanup targets.
- Captured a live high-CPU profile for `emule.exe -c C:\tmp\emule-testing` while hashing the problematic shared tree under `C:\tmp\videodupez\`.

## Current State

- Startup config resolution is now explicit when `-c` is supplied and no longer depends on registry or auto-detected config roots for the config/log side of the profile.
- Invalid `-c` usage fails fast instead of silently falling back to another config location.
- Shared tests cover supported and rejected `-c` forms.
- The isolated non-UI probes still show:
  - raw buffered reads are fine
  - raw mapped reads are fine
  - offline full MD4+AICH hashing is fine
- The live ETW capture artifact is at `C:\prj\p2p\eMule\eMulebb\eMule-build\logs\20260401-013112-largefile-cpu-profile`.
- That capture shows `emule.exe` consuming roughly one full core for an extended period, and the hot samples are dominated by:
  - `emule.exe!CryptoPP::Weak1::MD4::Transform`
  - `emule.exe!CryptoPP::'anonymous namespace'::SHA1_HashBlock_CXX`
  - small `CAICHHashTree::*` activity
- Current evidence points to active hashing work in the live share path, not an idle UI loop and not the previous logging path.

## Next Chunk

- Use the existing ETW artifact and a debugger-capable tool path to recover the higher-level caller above the Crypto++ hashing leaf functions in the live process.
- Keep using `-c C:\tmp\emule-testing`, VPN bind, and file logging for every live `emule.exe` run.
- Prefer ETW sampling, dump capture, or debugger attachment before adding any more logging when investigating the hashing hot-loop.
