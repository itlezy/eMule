# Resume

## Last Chunk

- Added durable checkpoint logs to `CKnownFile::CreateFromFile`, including in-loop hash progress markers every 256 MiB.
- The new checkpoints mark `start`, `raw-hash-complete`, `aich-recalculate-*`, `aich-save-*`, `post-stat`, `metadata-*`, and `parts-info-done`.
- Added more explicit metadata extraction logs in `UpdateMetaDataTags` for skip, begin, success, failure, and exception cases with elapsed milliseconds.
- Added a standalone `--full-hash-probe` mode in `eMule-build-tests` to replay the offline MD4 plus AICH pipeline without launching `emule.exe` or relying on `preferences.ini`.

## Current State

- The isolated non-UI probes now show both `C:\tmp\videodupez\a11.bin` and the problematic long-path MP4 complete successfully in:
  - raw buffered reads
  - raw mapped reads
  - full offline MD4 plus AICH hashing with buffered reads
  - full offline MD4 plus AICH hashing with mapped reads
- That means the hot loop is not explained by plain file access, long-path handling, `MappedFileReader`, or the offline MD4/AICH hashing pipeline alone.
- The remaining suspects are higher-level `CreateFromFile` work outside the offline probe, such as metadata extraction, known-file registration, shared-file bookkeeping, or progress/event interactions.

## Next Chunk

- Reproduce the problematic share/hash run with the new trace file and in-loop progress markers, then inspect the last emitted `CreateFromFile checkpoint`, `hash-progress`, or `Shared meta extraction` line.
- If the trace reaches `raw-hash-complete`, isolate post-hash work next.
- If the trace loops before `raw-hash-complete`, compare the live `CreateFromFile` implementation against the new offline full-hash probe to identify what extra live-path behavior remains.
