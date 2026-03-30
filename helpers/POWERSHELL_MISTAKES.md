# PowerShell Mistakes

## 2026-03-29

- Error:
  `ParserError: An empty pipe element is not allowed.`
- Cause:
  a pipeline was attached directly after a `foreach` block in a single inline `pwsh` command without first materializing the results.
- Fix:
  assign the loop results to a variable or wrap the loop in a subexpression before piping to `Sort-Object`.

- Error:
  `Get-ChildItem` was called with multiple literal paths, and missing parent `AGENTS.md` paths produced non-terminating path-not-found errors.
- Cause:
  I probed `AGENTS.md`, `..\\AGENTS.md`, and `..\\..\\AGENTS.md` in one call without guarding each candidate with `Test-Path`.
- Fix:
  when checking a small set of optional paths, filter them through `Test-Path` first and only pass existing paths to `Get-ChildItem`.

- Error:
  `rg` failed with `regex parse error: unclosed group` while searching for several literal tokens including `pragma comment(lib, "bcrypt.lib")`.
- Cause:
  I passed a complex alternation with unescaped metacharacters directly as a regular expression instead of either escaping it fully or using fixed-string searches.
- Fix:
  for mixed literal tokens, use separate `rg -F` calls or simplify the pattern to avoid regex metacharacters.

- Error:
  `rg` reported `The filename, directory name, or volume label syntax is incorrect. (os error 123)` for `.\\srchybrid\\*.vcxproj*`.
- Cause:
  I passed a Windows wildcard path as a literal search target to `rg`; `rg` expects actual paths or glob filters, not shell-expanded wildcard filenames on Windows.
- Fix:
  use `rg --glob '*.vcxproj*' ... .\\srchybrid` or pass concrete file paths discovered first with `Get-ChildItem`.

## 2026-03-30

- Error:
  `Get-ChildItem` returned path-not-found errors while probing `AGENTS.md`, `..\\AGENTS.md`, and `..\\..\\AGENTS.md` in one call.
- Cause:
  I mixed an existing current-repo path with optional parent paths without filtering the missing candidates first, which produced avoidable non-terminating errors.
- Fix:
  build the candidate list first, keep only paths where `Test-Path` succeeds, and then pass the filtered set to `Get-ChildItem`.

- Error:
  `rg` returned `The filename, directory name, or volume label syntax is incorrect. (os error 123)` when I passed paths like `.\\srchybrid\\*.cpp` and `.\\srchybrid\\Preferences.*`.
- Cause:
  I reused shell-style wildcard path arguments with `rg`, which expects real paths plus `--glob` filters instead of Windows wildcard filenames.
- Fix:
  pass a concrete directory such as `.\\srchybrid` and add `--glob '*.cpp'` or `--glob 'Preferences.*'` when filtering file names.

- Error:
  `rg` failed with `regex parse error: unclosed group` while searching for literal text containing `(`, `)`, and `"` inside a PowerShell one-liner.
- Cause:
  I used a regular-expression search for a literal pattern instead of switching to fixed-string search or escaping the metacharacters first.
- Fix:
  use `rg -F` for literal tokens, or use `Select-String` when the pattern is already naturally expressed as a PowerShell regex string.

- Error:
  `Get-ChildItem` failed with `Cannot convert 'System.Object[]' to the type 'System.String' required by parameter 'Filter'.`
- Cause:
  I passed multiple filter values to `-Filter`, which accepts only a single string.
- Fix:
  use `Where-Object` with `$_ .Extension` / `$_ .Name`, or run separate `Get-ChildItem` calls when matching several filename patterns.

- Error:
  `rg` again returned `The filename, directory name, or volume label syntax is incorrect. (os error 123)` when I passed wildcard paths such as `.\\srchybrid\\*.h` and `.\\srchybrid\\*.cpp`.
- Cause:
  I slipped back into shell-style wildcard path arguments instead of using `rg` globs against a concrete directory.
- Fix:
  pass `.\\srchybrid` as the search root and add `--glob '*.h' --glob '*.cpp'` when filtering by extension.

- Error:
  `rg` returned `The filename, directory name, or volume label syntax is incorrect. (os error 123)` when I passed `.\\srchybrid\\*` as a search target.
- Cause:
  I reused a shell wildcard path with `rg`, which expects a real directory path and optional `--glob` filters rather than a Windows wildcard path.
- Fix:
  pass `.\\srchybrid` as the search root and add `--glob` filters only when file-name filtering is needed.

- Error:
  `Select-String` failed with `Cannot find path ...\\srchybrid\\WebServer.cpp because it does not exist.`
- Cause:
  I searched for a removed source file name from stale audit notes without verifying the current tree path first.
- Fix:
  confirm the live file list with `rg --files` or `Test-Path` before issuing targeted `Select-String` reads against audit-referenced paths.

- Error:
  `Select-Object` failed with `Cannot bind parameter 'Index'. Cannot convert value "338..356" to type "System.Int32".`
- Cause:
  I tried to pass a PowerShell range expression to `Select-Object -Index`, which expects already-materialized integers rather than a quoted range token.
- Fix:
  slice the content array directly (`$c[338..356]`) or pass an unquoted integer array to `-Index`.

- Error:
  `git submodule add ..\\eMulebb-tests tests` resolved to `https://github.com/itlezy/eMulebb-tests` instead of the intended local sibling repo.
- Cause:
  I passed a relative submodule URL in a repo with a GitHub `origin`, so Git resolved it relative to the remote URL rather than as a local filesystem path.
- Fix:
  use an explicit local path when adding the submodule, then normalize `.gitmodules` to the desired relative URL afterward.

- Error:
  `git submodule add C:/prj/p2p/eMulebb-tests tests` failed with `transport 'file' not allowed`.
- Cause:
  modern Git blocks local `file` transport for submodule add unless it is explicitly permitted.
- Fix:
  run the add with `-c protocol.file.allow=always`, then re-stage `.gitmodules` after any URL cleanup.

- Error:
  the parent `.cmd` wrappers passed `"%~dp0"` directly to PowerShell parameters and the called script interpreted the workspace path as part of the remaining argument string.
- Cause:
  the quoted `%~dp0` value ends with a trailing backslash, which is fragile when forwarded through `pwsh -File ... -WorkspaceRoot`.
- Fix:
  normalize the wrapper argument first, for example with `SET "WORKSPACE_ROOT=%~dp0."`, and pass that stabilized path to PowerShell instead of the raw `%~dp0`.

- Error:
  `git checkout <sha>` in the `tests` submodule failed with `fatal: unable to read tree (...)`.
- Cause:
  I pasted an incorrect full commit hash while advancing the submodule to the latest sibling-repo commit.
- Fix:
  verify the exact commit with `git rev-parse HEAD` in the source repo before checking it out in the submodule worktrees.

- Error:
  `Get-Content` failed for `tests\\reports\\dev-parity.log` because the file did not exist.
- Cause:
  I assumed the doctest XML reporter would still emit console output to tee into the `.log` file, but with `--out=<xml>` it did not produce a companion log.
- Fix:
  confirm whether a reporter writes to stdout before reading a derived log path, or guard the read with `Test-Path`.

- Error:
  `Get-Content` failed with `Cannot find path ...\\srchybrid\\Tag.h because it does not exist.`
- Cause:
  I assumed the tag declarations lived in a dedicated `Tag.h` file instead of confirming the current header layout first.
- Fix:
  use `rg --files .\\srchybrid` or `Test-Path` to confirm the live header path before issuing a targeted `Get-Content`.

- Error:
  two `MSBuild` invocations for `emule-tests.vcxproj` collided and `CL.exe` failed with `Cannot open compiler generated file ...\\protocol.tests.obj: Permission denied`.
- Cause:
  I started the standalone test build and the live-diff build in parallel, and both tried to write the same intermediate/object paths at the same time.
- Fix:
  do not parallelize builds that share the same output tree; run the shared test build and the live-diff script serially.

- Error:
  `MSBuild` failed with `MSB3491: Could not write lines to file ...\\emule-tests.lastbuildstate ... because it is being used by another process`.
- Cause:
  I repeated the same mistake and launched the standalone shared-test build and the live-diff build in parallel, so both commands contended for the same `.tlog` state files.
- Fix:
  treat all `emule-tests.vcxproj` builds as mutually exclusive when they target the same `BuildTag` output tree; never run those scripts in parallel.

- Error:
  parallel `git commit` commands in the `eMule` submodule failed with `Unable to create ...\\.git\\modules\\eMule\\index.lock: File exists`.
- Cause:
  I launched two commits against the same repository at the same time, so both processes contended for the submodule index lock and one commit ended up recording the wrong staged payload/message pairing.
- Fix:
  never parallelize `git add`/`git commit` operations that target the same repository; stage and commit those changes serially.

- Error:
  `mt.exe` failed with `Missing command-line option "-inputresource:"` when extracting the embedded manifest.
- Cause:
  I constructed the PowerShell invocation as separate concatenated tokens, so `mt.exe` did not receive the `-inputresource:<path>;#1` argument as one complete string.
- Fix:
  build the full argument with interpolation, for example `"-inputresource:$($exe);#1"`, and pass it as a single token to `mt.exe`.
