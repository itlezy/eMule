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
