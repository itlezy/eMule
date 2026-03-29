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
