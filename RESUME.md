# Resume

## Last Chunk

- Removed the MiniMule feature, including its tray/main-window hooks, persisted preference, dialog resources, and translated UI string entries.
- Deleted the MiniMule-only IE hosting code path by removing `MiniMule.*`, `IESecurity.*`, `MiniMule.htm`, and `MiniMuleBack.gif`.
- Updated the affected docs so the preference reference, dead-code audit, roadmap, and CMake inventory match the current tree.

## Current State

- The source tree no longer contains live MiniMule or IE-hosted dialog references in `srchybrid/`.
- A verification build is still required because the removal touched core UI code, resources, and the project file.
- The parent repo should only be updated after the submodule commit is finalized.

## Next Chunk

- Build with `..\23-build-emule-debug-incremental.cmd` and fix any fallout from the resource/project cleanup.
- Commit the MiniMule removal as a dedicated WIP chunk.
- After the tree is green, move on to the next modernization or security defect.
