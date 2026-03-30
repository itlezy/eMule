# Resume

## Last Chunk

- Removed the legacy update checker end to end, including the startup DNS lookup, manual version-check commands, notifier path, server/web links, and related preference plumbing.
- Deleted the update-checker resource surface from the main dialogs and pruned the translated version-check strings from every maintained `lang/*.rc` file.
- Updated the preference and roadmap docs so they reflect that the old checker is gone and any future updater is a fresh implementation.

## Current State

- The source tree no longer contains live update-checker code paths or user-facing version-check UI in `srchybrid/`.
- A verification build is still required because the removal touched core UI code, resources, notifier plumbing, and localized string tables.
- The parent repo should only be updated after the submodule commit is finalized.

## Next Chunk

- Build with `..\23-build-emule-debug-incremental.cmd` and fix any fallout from the update-checker cleanup.
- Commit the update-checker removal as a dedicated WIP chunk.
- After the tree is green, move on to the next modernization or security defect.
