# Resume

## Last Chunk

- Added durable verbose checkpoint logs to `CKnownFile::CreateFromFile`.
- The new checkpoints mark `start`, `raw-hash-complete`, `aich-recalculate-*`, `aich-save-*`, `post-stat`, `metadata-*`, and `parts-info-done`.
- Added more explicit metadata extraction logs in `UpdateMetaDataTags` for skip, begin, success, failure, and exception cases with elapsed milliseconds.
- Verified the instrumentation builds successfully with `..\23-build-emule-debug-incremental.cmd`.

## Current State

- The isolated non-UI scanner still shows the problematic long-path file is fine in both buffered and mapped raw reads.
- The full eMule pipeline now has enough logging to tell whether the 100% CPU loop is before or after metadata extraction.
- To see the new checkpoints clearly, keep `Verbose=1` while reproducing the hashing/share issue.

## Next Chunk

- Reproduce the problematic share/hash run and inspect the last emitted `CreateFromFile checkpoint` or `Shared meta extraction` line.
- If the last line is `metadata-begin`, isolate `GetMediaInfoDllInfo` further or disable metadata extraction for an A/B confirmation.
- If the last line is earlier, break down the AICH finalize/save path next.
