# Resume

## Last Chunk

- Added a standalone `--hash-probe` mode to `eMule-build-tests\build\<tag>\x64\Debug\emule-tests.exe`.
- The probe runs outside the GUI app and compares buffered reads against the shared `MappedFileReader` path on an arbitrary Unicode/long file path.
- Verified the long-path `videodupez` MP4 (`prepared-path-length=363`) completes in both buffered and mapped modes in about 24.5 seconds with identical digests.
- This rules out `CreateFile` long-path handling and `VisitMappedFileRange` itself as the source of the observed 100% CPU loop during full eMule share hashing.

## Current State

- The isolated non-UI scanner can now be used against any suspect path with:
  `eMule-build-tests\build\eMule-build\x64\Debug\emule-tests.exe --hash-probe "<path>"`.
- The problematic `videodupez` long-path file is not getting stuck in raw buffered or mapped sequential scanning.
- The remaining suspect area is higher in the startup/share pipeline: AICH tree integration, metadata extraction, or another post-read stage that only runs inside full eMule shared-file processing.

## Next Chunk

- Instrument or isolate the next stage above raw scanning, starting with the `CKnownFile::CreateFromFile` pipeline after `CreateHash`.
- Compare pure scan timing against AICH-tree population and metadata-tag extraction on the same long-path file.
