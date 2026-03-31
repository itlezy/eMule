# Restructure Guidance

## Table of Contents

- [Current problem](#current-problem)
- [Main recommendation](#main-recommendation)
- [Suggested module layout](#suggested-module-layout)
- [Highest-value split targets](#highest-value-split-targets)
- [Dependency rules](#dependency-rules)
- [Refactoring principles](#refactoring-principles)
- [Recommended sequence](#recommended-sequence)
- [Feature Identifier](#feature-identifier) (PLAN_001)

## Current problem

The main structural issue is not only file size, but that too much code still lives flat under `srchybrid`.

Current tree characteristics:

- roughly 226 `.cpp` files and 239 `.h` files live directly in `srchybrid`
- only a few real subdirectories exist there today
- many responsibilities are mixed in the same files and same folder

That weakens ownership boundaries and makes local refactors more expensive than they should be.

## Main recommendation

Introduce real module folders first, then move code into them as files become single-purpose.

Do not start with a giant move-only refactor. Folder moves help only after responsibilities are clear.

## Suggested module layout

### `srchybrid/core`

Purpose:

- files and part files
- queues
- preferences model/state
- hashing and shared file state
- search state

Candidate files:

- `PartFile.cpp`
- `KnownFile.cpp`
- `DownloadQueue.cpp`
- `Preferences.cpp`
- `SearchList.cpp`
- `SharedFileList.cpp`

### `srchybrid/net`

Purpose:

- sockets
- packet handling
- clients and servers
- connection management
- UPnP and NAT-related transport helpers

Candidate files:

- `ListenSocket.cpp`
- `ServerSocket.cpp`
- `UDPSocket.cpp`
- `BaseClient.cpp`
- `DownloadClient.cpp`
- `UploadClient.cpp`
- `UPnPImpl*.cpp`

### `srchybrid/ui`

Purpose:

- dialogs
- controls
- property pages
- window composition
- rendering and user interaction

Candidate files:

- `EmuleDlg.cpp`
- `TransferWnd.cpp`
- `DownloadListCtrl.cpp`
- `SearchResultsWnd.cpp`
- all `PPg*`
- all `*Dlg*`
- all `*Wnd*`
- all `*Ctrl*`

### `srchybrid/media`

Purpose:

- media parsing
- metadata extraction
- frame grabbing
- preview support
- thumbnails and format helpers

This direction already started with the `MediaInfo_*` split.

### `srchybrid/web`

Purpose:

- web server
- web templates/rendering
- future HTTP API handlers

Candidate files:

- `WebServer.cpp`
- `WebSocket.cpp`
- `webinterface`

### `srchybrid/platform`

Purpose:

- Windows-specific integration
- registry helpers
- shell integration
- service/process/platform shims

## Highest-value split targets

Before broad directory moves, split the worst god files first:

- `PartFile.cpp`
- `WebServer.cpp`
- `OtherFunctions.cpp`
- `EmuleDlg.cpp`
- `Preferences.cpp`

`OtherFunctions.cpp` is especially worth eliminating as a concept. It should be replaced by domain-specific helpers instead of remaining a generic junk drawer.

## Dependency rules

Enforce simple boundaries:

- `ui` may depend on `core`, `net`, `media`
- `core` must not depend on `ui`
- `net` must not depend on `ui`
- `media` should expose helpers and parsed results, not reach into dialogs directly
- `web` should sit on top of `core` and `net`, not embed business logic directly in template handlers

## Refactoring principles

### Split by product domain, not by technology

Do not organize primarily around MFC, sockets, or MediaInfo.

Prefer product/domain boundaries:

- transfers
- library/shared files
- search
- peers/servers
- media
- web/API

That maps better to future work, especially if a qBittorrent-style HTTP API is added later.

### Separate model/state from rendering

Many dialogs and list controls likely mix:

- data acquisition
- formatting
- state mutation
- painting
- command handling

That should be separated before larger feature work.

### Avoid new wrapper layers

Given the repo rules, prefer narrow domain helpers over generic manager/wrapper classes.

Use direct names and obvious ownership rather than adding abstraction for its own sake.

## Recommended sequence

1. Split the largest mixed-responsibility files into smaller domain files without intended behavior changes.
2. Move the split files into module folders once their purpose is clear.
3. Enforce dependency direction between `ui`, `core`, `net`, `media`, and `web`.
4. Continue removing junk-drawer files and cross-layer reach-through.

## Practical conclusion

The best structural improvement is not a cosmetic directory shuffle.

It is:

- define domain modules
- split the god files first
- move only clarified files
- enforce one-way dependencies
- keep UI out of core logic

That will make future MediaInfo work, web API work, and general modernization much easier.

---

## Feature Identifier

### PLAN_001: Module Restructuring

This document describes the plan for introducing real module folders under `srchybrid/` and splitting god files into domain-specific units. The recommended sequence is:

1. Split the largest mixed-responsibility files (`PartFile.cpp`, `WebServer.cpp`, `OtherFunctions.cpp`, `EmuleDlg.cpp`, `Preferences.cpp`)
2. Move clarified files into module folders (`core/`, `net/`, `ui/`, `media/`, `web/`, `platform/`)
3. Enforce one-way dependency rules between modules
4. Eliminate junk-drawer files and cross-layer reach-through

**Status:** Planning phase. No moves have been made yet.
