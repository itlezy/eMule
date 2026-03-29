# Minimal Long-Path Support

## Scope

This guide describes the reduced long-path surface implemented for this branch. It intentionally does not replace or modify `docs/GUIDE-LONGPATHS.md`.

Supported:

- sharing a single file whose full path exceeds `MAX_PATH`
- sharing a directory whose descendant files exceed `MAX_PATH`
- recursive shared-directory scanning
- junction and symlink traversal during recursive scanning, with loop protection
- hashing shared files
- rebuilding AICH data for shared files
- uploading shared files after they are accepted into the share list
- extracting media metadata from shared media files
- completing a download into a long final destination path

Deferred:

- preview and thumbnail generation for long-path shared media files

Out of scope:

- `.lnk` shortcut sharing
- long temp directories
- active `.part` / `.part.met` download I/O
- collections, archive recovery, and unrelated path consumers

## Prerequisites

Long-path support in this branch assumes both of these are true:

1. the executable manifest is marked `longPathAware`
2. Windows long paths are enabled through the system policy / registry

There is no runtime fallback path. If the OS policy is disabled, overlong paths are not a supported configuration.

## Implemented Areas

### Shared-file discovery

`SharedFileList.cpp` now uses long-path-safe Win32 enumeration for:

- adding a single shared file
- scanning shared directories recursively
- reloading shared directories

The recursive scanner tracks visited directories by file identity when possible, which prevents junction and symlink loops without disabling reparse-point traversal.

`.lnk` entries are rejected instead of being resolved.

### Shared-file processing

`KnownFile.cpp` now uses long-path-safe read opens for:

- `CreateFromFile`
- `CreateAICHHashSetOnly`
- shared media metadata extraction entry points

`UploadDiskIOThread.cpp` now opens shared files through long-path-safe Win32 paths so accepted shared files remain uploadable.

`MediaInfo.cpp` and `MediaInfo_DLL.cpp` now use long-path-safe file access for shared-file metadata extraction.

### Download completion

`PartFile.cpp::PerformFileComplete` now treats the final completed destination path as long-path-safe for:

- category incoming selection
- destination collision checks
- final move into incoming / category incoming
- post-move metadata stat calls

This does not extend long-path support to active temp download storage.

## Deferred Preview Work

Preview and thumbnail generation for shared long-path media files is intentionally deferred. The current code marks this with a `TODO` near `CKnownFile::GrabImage`.

## Validation

Recommended manual checks:

1. Share a single file with a path longer than 260 characters and confirm hash, list entry, and upload work.
2. Share a directory containing a nested file with a path longer than 260 characters and confirm recursive discovery works.
3. Include a junction or symlink in a shared tree and confirm the scan does not loop.
4. Trigger AICH rebuild for a long-path shared file and confirm it succeeds.
5. Share a long-path audio or video file and confirm metadata extraction still works.
6. Complete a download whose final incoming path exceeds 260 characters and confirm rename / move succeeds.
