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

- Error:
  `Select-String` failed with `A positional parameter cannot be found that accepts argument '\$\(Platform\)'`.
- Cause:
  I pushed a heavily escaped pattern through a PowerShell one-liner and the quoting broke before `Select-String` received the intended search string.
- Fix:
  prefer simpler literal searches, or put the pattern in a PowerShell variable and pass that variable to `Select-String -Pattern`.

- Error:
  `rg` failed with `regex parse error: unclosed group` while I searched for several literal XML / MSBuild tokens in one alternation.
- Cause:
  I mixed regex alternation with literal strings containing backslashes, quotes, and angle brackets instead of switching to fixed-string or PowerShell-native matching.
- Fix:
  when searching literal manifest or project tokens, use `rg -F` with one token at a time or `Select-String` with a small literal pattern list.

- Error:
  `rg` reported path-not-found errors under `srchybrid\lang\x64\...` while I was checking for renamed identifiers.
- Cause:
  I searched the whole `srchybrid` tree in parallel with a cleanup that deleted the generated `srchybrid\lang\x64` build output, so `rg` raced stale paths.
- Fix:
  do not run recursive searches against a tree that is being deleted in parallel; delete generated output first, then search, or exclude that path explicitly with `--glob '!srchybrid/lang/x64/**'`.

- Error:
  `rg` again returned `The filename, directory name, or volume label syntax is incorrect. (os error 123)` when I passed wildcard paths such as `srchybrid\*.cpp` and `srchybrid\*.h`.
- Cause:
  I slipped back into Windows wildcard path arguments instead of passing a real search root and using `rg` globs for extension filtering.
- Fix:
  pass `srchybrid` as the concrete search root and use `--glob '*.cpp' --glob '*.h'` when restricting file types.
- 2026-03-31: g -n "ID_TOOLS|Tools|IDS_NETWORK_INFO|ShowNetworkInfo|ON_COMMAND\(|ON_BN_CLICKED\(|IDD_NETWORK_INFO|MENUITEM" .\srchybrid\emule.rc .\srchybrid\*.h .\srchybrid\*.cpp failed because ripgrep on Windows does not expand wildcard path arguments. Use a directory root such as .\srchybrid and filter with the pattern instead.
- 2026-03-31: g -n "MP_.*TOOLS|MP_.*NETWORK|IDS_NETWORK_INFO|ShowNetworkInfo|NetworkInfo" .\srchybrid\EmuleDlg.cpp .\srchybrid\MenuCmds.h .\srchybrid\*.h failed for the same reason: ripgrep does not expand Windows wildcard path arguments. Search the .\srchybrid directory directly instead.

- Error:
  `Get-ChildItem ..\\..\\eMule-zlib`, `Get-ChildItem ..\\..\\eMule-cryptopp`, and `Get-ChildItem ..\\..\\eMule-ResizableLib` failed with path-not-found errors.
- Cause:
  I misapplied the workspace-root math from the `eMule` submodule working directory and probed two levels up instead of the sibling dependency repositories one level up.
- Fix:
  from `eMule-build\\eMule`, probe sibling dependencies with `..\\eMule-zlib`, `..\\eMule-cryptopp`, and `..\\eMule-ResizableLib`, or resolve the expected absolute path before calling `Get-ChildItem`.

- Error:
  `Select-Object -Index 576..590` failed with `Cannot bind parameter 'Index'. Cannot convert value "576..590" to type "System.Int32".`
- Cause:
  I passed a PowerShell range expression directly to `Select-Object -Index` instead of materializing the content slice first.
- Fix:
  use array slicing like `$content[576..590]`, or pass an actual integer array variable to `-Index`.

- Error:
  `Select-String` failed because I hard-coded `logs\20260331-202626-build-project-eMule-Debug-x64\eMule-Debug.log`, which did not exist.
- Cause:
  I assumed the latest build log timestamp instead of querying the logs directory for the current path.
- Fix:
  resolve the newest log directory first with `Get-ChildItem ..\logs -Directory | Sort-Object LastWriteTime -Descending | Select-Object -First 1`, then read `eMule-Debug.log` from that path.

## 2026-04-01

- Error:
  `msbuild` failed with `The term 'msbuild' is not recognized as a name of a cmdlet, function, script file, or executable program.`
- Cause:
  I assumed `MSBuild.exe` was available on `PATH` in the current PowerShell environment instead of resolving the installed Visual Studio path first.
- Fix:
  probe the Visual Studio installation with `Get-ChildItem 'C:\Program Files\Microsoft Visual Studio' -Recurse -Filter MSBuild.exe` and invoke the explicit executable path once discovered.

- Error:
  `rg` reported `The filename, directory name, or volume label syntax is incorrect. (os error 123)` when I passed literal targets like `C:\...\srchybrid\*.cpp` and `C:\...\srchybrid\*.h`.
- Cause:
  I again passed Windows wildcard path arguments directly to `rg`, which expects a real search root plus optional `--glob` filters.
- Fix:
  pass the concrete root directory, for example `C:\...\srchybrid`, and use `--glob '*.cpp' --glob '*.h'` only when file filtering is required.

- Error:
  `rg` failed with `regex parse error: unclosed group` while I bundled multiple literal search tokens into one pattern.
- Cause:
  I used a regex alternation for literal strings that contained metacharacters instead of switching to fixed-string or separate searches.
- Fix:
  use `rg -F` for literal tokens, or split the searches into separate `rg` / `Select-String` calls when the literals are not safe regex input.

- Error:
  `rg` failed with `unrecognized flag -|`.
- Cause:
  I passed a pattern beginning with `-` as a bare positional argument, so `rg` parsed the search pattern as another command-line flag.
- Fix:
  pass `--` before patterns that may begin with `-`, or use `rg -e '<pattern>' ...` to force explicit pattern parsing.

- Error:
  `helper-runtime-wsapoll-smoke.ps1` failed with `Cannot find an overload for "new" and the argument count: "1".`
- Cause:
  I tried to construct `System.Collections.Generic.List[string]` directly from the `Get-Content` result, but that constructor overload was not available in the active PowerShell/.NET binding.
- Fix:
  create the generic list with the parameterless constructor and append file lines explicitly before mutating the INI content.

- Error:
  `helper-runtime-wsapoll-smoke.ps1` failed with `Cannot bind argument to parameter 'Value' because it is an empty string.`
- Cause:
  I declared the INI `Value` parameter as a mandatory string without allowing empty-string input, but the smoke profile intentionally writes empty values such as `BindInterface=` and `TempDirs=`.
- Fix:
  add `[AllowEmptyString()]` to the parameter declaration when empty INI values are valid and expected.

- Error:
  `Select-Object -Index 160..190` failed with `Cannot bind parameter 'Index'. Cannot convert value "160..190" to type "System.Int32".`
- Cause:
  I again passed a PowerShell range expression directly to `Select-Object -Index` instead of materializing the slice first.
- Fix:
  use array slicing like `$content[160..190]`, or pass an actual integer array variable to `-Index`.

- Error:
  `cdb.exe -pv -p <pid>` failed with `Unable to examine process id <pid>, Win32 error 0n87`.
- Cause:
  I tried to use a non-invasive attach mode that was not accepted for that live debug target / process state.
- Fix:
  prefer a regular attach when safe, or fall back to `-assertfile`, dump capture, or other diagnostics instead of assuming `-pv` will work.

- Error:
  `rg` failed with `regex parse error: unclosed group` when I searched for several literal tokens around `Send(NULL|SendNegotiatingData...`.
- Cause:
  I again used a regex alternation for literal search text containing metacharacters instead of switching to fixed-string or split searches.
- Fix:
  use separate `rg -F` searches or `Select-String` for these literal code probes.

- Error:
  `rg` failed with `The filename, directory name, or volume label syntax is incorrect. (os error 123)` when I passed `...\AsyncSocketEx.*` as a target path.
- Cause:
  I slipped back into using a wildcard path argument with `rg`, which expects a concrete path plus `--glob` filters rather than Windows wildcard filenames.
- Fix:
  pass the real directory or file path, and add `--glob` only when filename filtering is actually needed.

- Error:
  `& 'C:\prj\p2p\eMule\eMulebb\23-build-emule-debug-incremental.cmd'` failed with `The term ... is not recognized`.
- Cause:
  I launched the parent build script from the repo root instead of the actual `eMule-build` sibling directory called out by `AGENTS.md`.
- Fix:
  resolve helper/build script locations from the real sibling workspace layout first, and call `eMule-build\23-build-emule-debug-incremental.cmd` from there.

- Error:
  `& msbuild ...` failed with `The term 'msbuild' is not recognized`.
- Cause:
  I assumed Visual Studio build tools were exposed on `PATH` instead of using the repo's existing wrapper scripts that bootstrap the right environment.
- Fix:
  prefer the checked-in `26-build-emule-tests-debug.cmd` and related workspace scripts unless I have already confirmed the toolchain executable path.

- Error:
  `Select-String` failed with `Cannot find path '...eMule-build\helpers\helper-runtime-wsapoll-smoke.ps1' because it does not exist.`
- Cause:
  I again forgot that repo helpers live under `eMule-build\eMule\helpers`, not directly under `eMule-build\helpers`.
- Fix:
  resolve helper paths from the actual repo root (`eMule-build\eMule`) before probing or invoking them.

- Error:
  `Get-Content ... | Select-Object -Index (190..210),(2004..2055)` failed because `-Index` expects one integer array, not multiple range arguments.
- Cause:
  I passed multiple ranges directly to `Select-Object -Index` instead of materializing one combined index list or reading the slices separately.
- Fix:
  use array slicing on the loaded content or issue separate read commands per range when inspecting non-contiguous sections.

- Error:
  `Select-Object -Index (80..125),(826..850)` failed for the same reason while sampling multiple doc ranges.
- Cause:
  I repeated the same mistake by passing multiple PowerShell ranges directly to `Select-Object -Index`.
- Fix:
  load the file once and use array slicing, or run separate reads for each range instead of combining them in `-Index`.

- Error:
  `rg` returned `The filename, directory name, or volume label syntax is incorrect. (os error 123)` when I passed paths like `C:\...\srchybrid\*.h` and `C:\...\srchybrid\*.cpp`.
- Cause:
  I again mixed Windows wildcard path arguments into `rg` instead of searching a concrete directory and filtering with `--glob`.
- Fix:
  pass the actual root directory such as `C:\...\srchybrid` to `rg` and use `--glob '*.h' --glob '*.cpp'` only when file-name filtering is needed.

- Error:
  `Get-Content` failed for `...\srchybrid\BaseClient.h` because that header does not exist in the live tree.
- Cause:
  I assumed there was a dedicated `BaseClient.h` without checking the current repo layout first.
- Fix:
  confirm the live file path with `rg --files` or `Test-Path` before issuing a targeted file read against a guessed header name.

- Error:
  `Get-Content` failed for `C:\prj\p2p\eMule\eMulebb\eMule-remote\src\shared\emule.ts` because that file does not exist.
- Cause:
  I guessed the remote shared types path instead of confirming the current `eMule-remote` layout first.
- Fix:
  locate the live file with `rg --files` or `rg -n` before reading a guessed path in sibling workspaces.

- Error:
  `pwsh -File ... -StressQueries 'ubuntu','debian netinst',...` mis-bound a later token to `RemotePort` with `Cannot convert value "media" to type "System.Int32"`.
- Cause:
  I passed an array-style argument list to a script invoked through `pwsh -File`; the command-line binder did not preserve the intended array parameter shape.
- Fix:
  when a script parameter needs an array value, prefer `pwsh -Command { & .\script.ps1 -Param @('a','b') }` or repeat the parameter in a form the binder supports.

- Error:
  running two live-session helpers in parallel against the same `C:\tmp\emule-testing` profile failed with `preferences.ini ... is being used by another process`.
- Cause:
  I parallelized runtime sessions that mutate the same disposable profile and launch the same UI process, which is not an independent workload.
- Fix:
  keep live `emule.exe` stress sessions strictly serial unless each run has its own isolated profile root and runtime ports.
