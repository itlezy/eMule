# eMule Code Quality Roadmap

Planned migration from Visual Studio project files to CMake, with layered static
analysis, formatting enforcement, and runtime bug-finding tooling. MFC stays.
C++17 target. Solo project. No CI/CD in scope for now.

---

## Table of Contents

- [Toolchain Inventory](#toolchain-inventory)
- [Current State (Baseline)](#current-state-baseline)
- [Phase 1 — CMake Migration](#phase-1--cmake-migration-foundation)
- [Phase 2 — Code Formatting (clang-format)](#phase-2--code-formatting-clang-format)
- [Phase 3 — MSVC Compiler Hardening](#phase-3--msvc-compiler-hardening)
- [Phase 4 — clang-tidy](#phase-4--clang-tidy)
- [Phase 5 — cppcheck](#phase-5--cppcheck)
- [Phase 6 — MSVC AddressSanitizer](#phase-6--msvc-addresssanitizer-debug-builds-only)
- [Phase 7 — scan-build (Clang Static Analyzer)](#phase-7--scan-build-clang-static-analyzer)
- [Phase 8 — clang-include-cleaner](#phase-8--clang-include-cleaner)
- [Recommended Execution Order](#recommended-execution-order)
- [Quick Reference — Tool Command Summary](#quick-reference--tool-command-summary)
- [Feature Identifier](#feature-identifier) (PLAN_005)

---

## Toolchain Inventory

### Installed (ready to use)

| Tool | Version | Path |
|------|---------|------|
| LLVM (clang, clang-format, clang-tidy, clangd, scan-build, clang-include-cleaner) | 22.1.0 | `C:\Program Files\LLVM\bin\` |
| CMake | 4.2.0 | `C:\Program Files\CMake\bin\` |
| MSVC | v143 (VS 2022 Professional) | via Developer Shell |
| Python | 3.13 | `C:\Python313\` |

### Must install before starting

```powershell
winget install Ninja-build.Ninja       # fast incremental builds, required for compile_commands.json flow
winget install Cppcheck.Cppcheck       # separate bug-class analysis from clang-tidy
```

Verify after install:

```powershell
ninja --version     # expect 1.12+
cppcheck --version  # expect 2.14+
```

### VS Code extensions to install

| Extension ID | Purpose |
|---|---|
| `ms-vscode.cmake-tools` | CMake configure/build/preset UI |
| `llvm-vs-code-extensions.vscode-clangd` | Real-time clang-tidy, go-to-def, completion via compile_commands.json |
| `xaver.clang-format` | clang-format on-save (or use clangd's formatter — pick one) |

> Disable `ms-vscode.cpptools` IntelliSense engine after installing clangd:
> set `"C_Cpp.intelliSenseEngine": "disabled"` in `.vscode/settings.json`.
> Leave the extension installed for its debugger (MSVC natvis, etc.).

---

## Current State (Baseline)

- Build system: `emule.vcxproj` (MSBuild, Visual Studio 2022)
- Compiler: MSVC v143, x64 primary, Win32 secondary
- C++ standard: unset (MSVC defaults to C++14 in v143 without explicit flag)
- Warning level: `/Wall` (`EnableAllWarnings`) — already enabled
- Runtime: `/MTd` debug, `/MT` release (static CRT, no DLL dependency)
- PCH: `stdafx.h` / `stdafx.cpp`
- Defines: `MINIUPNP_STATICLIB`, `SUPPORT_LARGE_FILES`,
  `_CRT_SECURE_NO_DEPRECATE`, `UNICODE`, `_UNICODE`
- **Note (2026-03-31):** `ID3LIB_LINKOPTION=1` and `MBEDTLS_ALLOW_PRIVATE_ACCESS` were removed when id3lib and MbedTLS were purged from the build.
- Existing linters/formatters/sanitizers: **none**
- Existing `.editorconfig`: tab-indent, CRLF for C++ sources

---

## Phase 1 — CMake Migration (Foundation)

Everything downstream (clang-tidy, clangd, scan-build, cppcheck with compile
commands) depends on having `compile_commands.json`. CMake with Ninja generates
this. This phase replaces `.vcxproj` / `.sln` entirely.

### 1.1 Prerequisites

All CMake builds must be launched from a **VS 2022 x64 Developer Command Prompt**
(or equivalent `vcvarsall.bat x64` environment), so MSVC is on PATH. CMake Tools
in VS Code handles this automatically if you configure the kit.

### 1.2 Directory layout

```
eMulebb/
├── eMule/
│   └── srchybrid/          ← sources stay here
│       ├── CMakeLists.txt  ← NEW: replaces emule.vcxproj
│       └── stdafx.h / stdafx.cpp
├── CMakeLists.txt           ← NEW: workspace root, wires deps
├── CMakePresets.json        ← NEW: Debug/Release x64 presets
└── .vscode/
    └── settings.json        ← NEW: clangd path, cmake kit
```

The two `CMakeLists.txt` files — workspace root wires dep paths and calls
`add_subdirectory`; `srchybrid/CMakeLists.txt` defines the `eMule` target.

### 1.3 Workspace root `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.25)
project(eMulebb)

# Must come before project() language setup to take effect on MSVC
cmake_policy(SET CMP0091 NEW)

# Dep roots — override on command line or via CMakePresets.json cache vars
# Default: sibling dirs matching the repo submodule layout
set(CRYPTOPP_ROOT  "${CMAKE_SOURCE_DIR}/eMule-cryptopp"    CACHE PATH "")
set(ID3LIB_ROOT    "${CMAKE_SOURCE_DIR}/eMule-id3lib"       CACHE PATH "")
set(MINIUPNP_ROOT  "${CMAKE_SOURCE_DIR}/eMule-miniupnp"     CACHE PATH "")
set(RESIZABLE_ROOT "${CMAKE_SOURCE_DIR}/eMule-ResizableLib" CACHE PATH "")
set(ZLIB_ROOT      "${CMAKE_SOURCE_DIR}/eMule-zlib"         CACHE PATH "")
set(MBEDTLS_ROOT   "${CMAKE_SOURCE_DIR}/eMule-mbedtls"      CACHE PATH "")

add_subdirectory(eMule/srchybrid)
```

### 1.4 `eMule/srchybrid/CMakeLists.txt` skeleton

```cmake
cmake_minimum_required(VERSION 3.25)

# ── C++ standard ──────────────────────────────────────────────────────────────
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)           # no /Ze, forces /Za-compatible code

# ── compile_commands.json (required by clang-tidy and clangd) ─────────────────
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Static runtime (/MT, /MTd) ────────────────────────────────────────────────
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ── Sources ────────────────────────────────────────────────────────────────────
# Collect all .cpp files; exclude third-party files copied into srchybrid if any
file(GLOB_RECURSE EMULE_SOURCES
    CONFIGURE_DEPENDS
    "*.cpp"
)
# Exclude generated / resource-only files that should not be compiled as C++
list(FILTER EMULE_SOURCES EXCLUDE REGEX ".*stdafx\\.cpp$")

# ── Target ─────────────────────────────────────────────────────────────────────
add_executable(eMule WIN32           # WIN32 = WinMain entry point, no console
    stdafx.cpp                       # PCH source listed explicitly first
    ${EMULE_SOURCES}
)

# ── Precompiled header ─────────────────────────────────────────────────────────
target_precompile_headers(eMule PRIVATE stdafx.h)

# ── MFC (static) ───────────────────────────────────────────────────────────────
set_target_properties(eMule PROPERTIES
    MFC_FLAG 1                       # 1 = static MFC (equivalent to /MT + UseOfMfc=Static)
)
# CMake's MFC_FLAG sets the right MSVC flags; MFC include dirs come from VS install
# If CMake does not pick up MFC automatically, add:
#   target_include_directories(eMule PRIVATE "$ENV{VCToolsInstallDir}/atlmfc/include")

# ── Include directories ─────────────────────────────────────────────────────────
target_include_directories(eMule PRIVATE
    .
    ..
    "${ID3LIB_ROOT}/include"
    "${MINIUPNP_ROOT}"
    "${MBEDTLS_ROOT}/include"
    "${MBEDTLS_ROOT}/tf-psa-crypto/include"
    "${MBEDTLS_ROOT}/tf-psa-crypto/drivers/builtin/include"
)

# ── Preprocessor defines ───────────────────────────────────────────────────────
target_compile_definitions(eMule PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX                         # prevent windows.h min/max macros
    ID3LIB_LINKOPTION=1
    MINIUPNP_STATICLIB
    SUPPORT_LARGE_FILES
    MBEDTLS_ALLOW_PRIVATE_ACCESS
    _CRT_SECURE_NO_DEPRECATE         # phase out in Phase 4 after SDL cleanup
    $<$<CONFIG:Release>:NDEBUG>
)

# ── Link libraries ─────────────────────────────────────────────────────────────
set(PLATFORM_NAME "$<IF:$<EQUAL:${CMAKE_SIZEOF_VOID_P},8>,x64,Win32>")
set(CRYPTOPP_LIB  "${CRYPTOPP_ROOT}/${PLATFORM_NAME}/$<CONFIG>/cryptlib.lib")
set(ID3LIB_LIB    "${ID3LIB_ROOT}/libprj/${PLATFORM_NAME}/$<CONFIG>/id3lib.lib")
set(MBEDTLS_LIB   "${MBEDTLS_ROOT}/visualc/VS2017/${PLATFORM_NAME}/$<CONFIG>/mbedtls.lib")
set(MINIUPNP_LIB  "${MINIUPNP_ROOT}/miniupnpc/msvc/${PLATFORM_NAME}/$<CONFIG>/miniupnpc.lib")
set(RESIZABLE_LIB "${RESIZABLE_ROOT}/${PLATFORM_NAME}/$<CONFIG>/resizablelib.lib")
set(ZLIB_LIB      "${ZLIB_ROOT}/contrib/vstudio/vc/${PLATFORM_NAME}/$<CONFIG>/zlib.lib")

target_link_libraries(eMule PRIVATE
    ADSIId.lib bcrypt.lib crypt32.lib delayimp.lib
    iphlpapi.lib version.lib winmm.lib ws2_32.lib
    "${CRYPTOPP_LIB}"
    "${ID3LIB_LIB}"
    "${MBEDTLS_LIB}"
    "${MINIUPNP_LIB}"
    "${RESIZABLE_LIB}"
    "${ZLIB_LIB}"
)
```

> **Note on MFC_FLAG**: CMake handles this via the legacy `MFC_FLAG` target
> property. CMake 3.25+ handles it correctly for Ninja generators. If the
> Ninja generator fails to find `afxres.h` during RC compilation, add the
> `atlmfc/include` path manually (see comment in snippet above).

### 1.5 `CMakePresets.json`

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/build/${presetName}",
      "cacheVariables": {
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "x64-debug",
      "displayName": "x64 Debug",
      "inherits": "base",
      "architecture": { "value": "x64", "strategy": "external" },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "x64-release",
      "displayName": "x64 Release",
      "inherits": "base",
      "architecture": { "value": "x64", "strategy": "external" },
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    }
  ],
  "buildPresets": [
    { "name": "x64-debug",   "configurePreset": "x64-debug"   },
    { "name": "x64-release", "configurePreset": "x64-release" }
  ]
}
```

`"strategy": "external"` tells CMake Tools that the architecture is set by the
VS Developer shell (the kit), not injected by CMake itself. This is correct for
Ninja + MSVC.

### 1.6 `.vscode/settings.json`

```json
{
  "cmake.configureOnOpen": true,
  "cmake.buildDirectory": "${workspaceFolder}/build/${buildKit}",
  "cmake.generator": "Ninja",
  "clangd.path": "C:/Program Files/LLVM/bin/clangd.exe",
  "clangd.arguments": [
    "--compile-commands-dir=${workspaceFolder}/build/x64-debug",
    "--clang-tidy",
    "--header-insertion=never",
    "--completion-style=detailed",
    "--background-index"
  ],
  "C_Cpp.intelliSenseEngine": "disabled",
  "editor.formatOnSave": true,
  "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd",
  "[cpp]": { "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd" },
  "[c]":   { "editor.defaultFormatter": "llvm-vs-code-extensions.vscode-clangd" }
}
```

### 1.7 Build workflow after migration

```powershell
# From VS 2022 x64 Developer PowerShell (or vcvarsall.bat x64 in cmd)
cd C:\prj\p2p\eMule\eMulebb\eMule-build

cmake --preset x64-debug
cmake --build --preset x64-debug

cmake --preset x64-release
cmake --build --preset x64-release
```

`compile_commands.json` is written to `build/x64-debug/` after configure.
Symlink or copy it to the workspace root for tools that look there:

```powershell
New-Item -ItemType SymbolicLink -Path ".\compile_commands.json" `
         -Target ".\build\x64-debug\compile_commands.json"
```

(Requires elevated shell or Developer Mode enabled.)

### 1.8 Validation checklist

- [ ] `cmake --preset x64-debug` exits 0
- [ ] `cmake --build --preset x64-debug` exits 0 — produces `eMule.exe`
- [ ] `cmake --build --preset x64-release` exits 0
- [ ] `build/x64-debug/compile_commands.json` exists and is non-empty
- [ ] clangd in VS Code shows no "file not found" errors for standard headers
- [ ] `.vcxproj` and `.sln` files deleted (or moved to `archive/` branch)

---

## Phase 2 — Code Formatting (clang-format)

### Goal

Consistent formatting enforced on save in VS Code, and checkable via script.
Start by matching the existing style exactly (tabs, CRLF, brace placement) so
the initial diff is zero. Tighten style in a later pass if desired.

### 2.1 `.clang-format` (place at `eMule/srchybrid/.clang-format`)

```yaml
---
# Based on existing eMule style: tabs, 4-wide, Allman braces, CRLF
BasedOnStyle: Microsoft
ColumnLimit: 120
IndentWidth: 4
TabWidth: 4
UseTab: ForIndentation
BreakBeforeBraces: Allman
AccessModifierOffset: -4
AlignAfterOpenBracket: Align
AlignConsecutiveAssignments: false
AlignTrailingComments: true
AllowShortBlocksOnASingleLine: Never
AllowShortFunctionsOnASingleLine: None
AllowShortIfStatementsOnASingleLine: Never
AllowShortLoopsOnASingleLine: false
BinPackArguments: false
BinPackParameters: false
PointerAlignment: Left
SortIncludes: CaseInsensitive
IncludeBlocks: Regroup
# Keep Windows-style line endings
DeriveLineEnding: true
UseCRLF: true
```

> Adjust `SortIncludes` with caution — `stdafx.h` must remain first. Add
> `IncludeCategories` rules to pin it if clang-format reorders it.

### 2.2 Check formatting without modifying files

```powershell
# Report files that differ from formatted output (no writes)
Get-ChildItem -Path eMule\srchybrid -Recurse -Include *.cpp,*.h |
    ForEach-Object {
        $result = & "clang-format" "--dry-run" "--Werror" $_.FullName 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Host "DIFF: $($_.FullName)" }
    }
```

### 2.3 Apply formatting to all files (one-time bulk pass)

```powershell
Get-ChildItem -Path eMule\srchybrid -Recurse -Include *.cpp,*.h |
    ForEach-Object { & "clang-format" "-i" $_.FullName }
```

Do this in a dedicated commit (`style: apply clang-format baseline`) so future
diffs are clean.

### 2.4 git pre-commit hook (optional, for discipline)

Create `.git/hooks/pre-commit`:

```bash
#!/usr/bin/env bash
set -e
changed=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.(cpp|h)$' || true)
[ -z "$changed" ] && exit 0
for f in $changed; do
    if ! clang-format --dry-run --Werror "$f" > /dev/null 2>&1; then
        echo "clang-format: $f needs formatting. Run: clang-format -i $f"
        exit 1
    fi
done
```

---

## Phase 3 — MSVC Compiler Hardening

Add these flags to `CMakeLists.txt` in the `eMule` target. All are MSVC-specific
and apply only when `MSVC` is true (which it always is in this project, but guard
anyway for correctness).

### 3.1 Flags to add now (warnings, not errors)

```cmake
if(MSVC)
    target_compile_options(eMule PRIVATE
        # Conformance
        /permissive-            # reject non-standard MSVC extensions
        /Zc:__cplusplus         # fix __cplusplus macro (needed for C++17 checks in headers)
        /Zc:throwingNew         # assume operator new throws (already in vcxproj)
        /Zc:inline              # remove unreferenced COMDAT (improves link time)
        /Zc:preprocessor        # conformant preprocessor (C++17 requirement)

        # Security (SDL = Security Development Lifecycle)
        /sdl                    # additional security checks: banned CRT functions,
                                # stack canaries, pointer validation

        # Static analysis built into MSVC (output: warnings in build log)
        /analyze                # enable MSVC SAL-based static analysis
        /analyze:WX-            # analysis warnings do NOT become errors yet (phase 3a)

        # Control flow integrity
        /guard:cf               # Control Flow Guard — mitigates ROP exploits
    )

    # Linker: CFG must also be set at link time
    target_link_options(eMule PRIVATE /guard:cf)
endif()
```

> `/permissive-` is the most likely to cause new diagnostics on legacy MFC code.
> It rejects things like string literal assignment to `char*`, non-standard
> base-class access patterns, and two-phase name lookup issues. Fix each
> diagnostic individually; do not suppress globally.

### 3.2 Phase 3b — after clearing Phase 3a noise

Once the build is clean under Phase 3a flags, promote `/analyze` warnings to
errors and add `/WX`:

```cmake
        /analyze:WX             # analysis warnings → errors (replace /analyze:WX-)
        /WX                     # all compiler warnings → errors
```

### 3.3 Defines to eventually remove

`_CRT_SECURE_NO_DEPRECATE` suppresses warnings about unsafe CRT functions
(`strcpy`, `sprintf`, etc.). The long-term goal is to fix each call site and
remove this define. Track with a search:

```powershell
grep -rn "strcpy\|sprintf\|strcat\|gets\b\|scanf\b" eMule/srchybrid --include="*.cpp" --include="*.h"
```

---

## Phase 4 — clang-tidy

clang-tidy reads `compile_commands.json` to understand exactly how each file
is compiled, then runs checkers on top. It requires the CMake migration
(Phase 1) to be complete.

### 4.1 `.clang-tidy` configuration (place at `eMule/srchybrid/.clang-tidy`)

Start with Wave 1 only. Uncomment successive waves as the previous wave's
findings are resolved.

```yaml
---
# Wave 1: Bug-finding only. No style changes, no modernization.
# Wave 2+: commented out — enable after Wave 1 is clean.

Checks: >
  -*,
  bugprone-assert-side-effect,
  bugprone-bool-pointer-implicit-conversion,
  bugprone-dangling-handle,
  bugprone-incorrect-roundings,
  bugprone-integer-division,
  bugprone-misplaced-operator-in-strlen-in-alloc,
  bugprone-misplaced-widening-cast,
  bugprone-move-forwarding-reference,
  bugprone-no-escape,
  bugprone-not-null-terminated-result,
  bugprone-parent-virtual-call,
  bugprone-sizeof-container,
  bugprone-sizeof-expression,
  bugprone-string-constructor,
  bugprone-string-integer-assignment,
  bugprone-suspicious-memset-usage,
  bugprone-suspicious-string-compare,
  bugprone-swapped-arguments,
  bugprone-terminating-continue,
  bugprone-undefined-memory-manipulation,
  bugprone-unhandled-self-assignment,
  bugprone-use-after-move,
  bugprone-virtual-near-miss,
  clang-analyzer-core.CallAndMessage,
  clang-analyzer-core.DivideZero,
  clang-analyzer-core.NonNullParamChecker,
  clang-analyzer-core.NullDereference,
  clang-analyzer-core.StackAddressEscape,
  clang-analyzer-core.UndefinedBinaryOperatorResult,
  clang-analyzer-core.uninitialized.Assign,
  clang-analyzer-core.uninitialized.Branch,
  clang-analyzer-cplusplus.Move,
  clang-analyzer-cplusplus.NewDelete,
  clang-analyzer-cplusplus.NewDeleteLeaks,
  clang-analyzer-security.FloatLoopCounter,
  clang-analyzer-security.insecureAPI.bcmp,
  clang-analyzer-security.insecureAPI.bcopy,
  clang-analyzer-security.insecureAPI.bzero,
  clang-analyzer-security.insecureAPI.rand,
  clang-analyzer-security.insecureAPI.strcpy,
  clang-analyzer-security.insecureAPI.UncheckedReturn

# Wave 2 (security + cert): uncomment after Wave 1 is clean
#  cert-dcl50-cpp,
#  cert-err34-c,
#  cert-err52-cpp,
#  cert-err60-cpp,
#  cert-flp30-c,
#  cert-mem57-cpp,
#  cert-msc30-c,
#  cert-msc50-cpp,
#  cert-msc51-cpp,
#  cert-oop57-cpp,
#  cert-oop58-cpp,
#  cppcoreguidelines-avoid-goto,
#  cppcoreguidelines-init-variables,
#  cppcoreguidelines-no-malloc,
#  cppcoreguidelines-prefer-member-initializer,
#  cppcoreguidelines-pro-type-const-cast,
#  cppcoreguidelines-pro-type-reinterpret-cast,
#  cppcoreguidelines-slicing,

# Wave 3 (C++17 modernization): uncomment after Wave 2 is clean
#  modernize-avoid-bind,
#  modernize-deprecated-headers,
#  modernize-loop-convert,
#  modernize-make-shared,
#  modernize-make-unique,
#  modernize-redundant-void-arg,
#  modernize-replace-auto-ptr,
#  modernize-replace-disallow-copy-and-assign-macro,
#  modernize-return-braced-init-list,
#  modernize-shrink-to-fit,
#  modernize-unary-static-assert,
#  modernize-use-auto,
#  modernize-use-bool-literals,
#  modernize-use-default-member-init,
#  modernize-use-emplace,
#  modernize-use-equals-default,
#  modernize-use-equals-delete,
#  modernize-use-nodiscard,
#  modernize-use-noexcept,
#  modernize-use-nullptr,
#  modernize-use-override,
#  modernize-use-std-numbers,
#  modernize-use-using,

# Wave 4 (readability + performance): uncomment last
#  performance-faster-string-find,
#  performance-for-range-copy,
#  performance-implicit-conversion-in-loop,
#  performance-inefficient-algorithm,
#  performance-inefficient-string-concatenation,
#  performance-inefficient-vector-operation,
#  performance-move-const-arg,
#  performance-move-constructor-init,
#  performance-no-automatic-move,
#  performance-trivially-destructible,
#  performance-type-promotion-in-math-fn,
#  performance-unnecessary-copy-initialization,
#  performance-unnecessary-value-param,
#  readability-avoid-const-params-in-decls,
#  readability-braces-around-statements,
#  readability-const-return-type,
#  readability-container-size-empty,
#  readability-delete-null-pointer,
#  readability-duplicate-include,
#  readability-else-after-return,
#  readability-make-member-function-const,
#  readability-misleading-indentation,
#  readability-misplaced-array-index,
#  readability-redundant-control-flow,
#  readability-redundant-preprocessor,
#  readability-simplify-boolean-expr,
#  readability-static-accessed-through-instance,
#  readability-string-compare,
#  readability-uniqueptr-delete-release,

# Treat no check as an error yet — review output, fix, then promote
WarningsAsErrors: ''

# Only report issues in eMule sources, not in dep headers
HeaderFilterRegex: '.*[/\\]srchybrid[/\\].*'

# Respect .clang-format when applying fixes
FormatStyle: file

CheckOptions:
  - key: bugprone-assert-side-effect.AssertMacros
    value: 'ASSERT,VERIFY,ENSURE'
  - key: modernize-use-auto.MinTypeNameLength
    value: '8'
  - key: modernize-use-default-member-init.UseAssignment
    value: '1'
```

### 4.2 Running clang-tidy manually

Single file:

```powershell
clang-tidy -p build\x64-debug `
           eMule\srchybrid\SomeFile.cpp
```

All files (uses the `run-clang-tidy` Python script bundled with LLVM):

```powershell
python "C:\Program Files\LLVM\bin\run-clang-tidy" `
       -p build\x64-debug `
       -j4 `
       "eMule/srchybrid/.*\.cpp$"
```

Apply auto-fixes (Wave 3 modernize checks are particularly auto-fixable):

```powershell
python "C:\Program Files\LLVM\bin\run-clang-tidy" `
       -p build\x64-debug `
       -fix `
       -fix-errors `
       "eMule/srchybrid/.*\.cpp$"
```

> Always commit before applying auto-fixes so you can `git diff` the result.

### 4.3 Known MFC gotchas with clang-tidy

- MFC macros (`DECLARE_MESSAGE_MAP`, `ON_COMMAND`, etc.) confuse some checks.
  Add `// NOLINT(check-name)` inline or suppress per-file with `.clang-tidy`
  path-specific overrides.
- `clang-analyzer-cplusplus.NewDelete` may fire on MFC `new`/delete patterns
  that are intentional. Suppress with `// NOLINT` or `clang-tidy: NOLINT` on
  the relevant lines.
- `modernize-use-override` is safe and high-value for MFC message handler
  overrides — apply early in Wave 3.

---

## Phase 5 — cppcheck

cppcheck catches different bug classes from clang-tidy (uninitialized variables
on complex paths, integer overflow, dead code, resource leaks in C-style code).
Run it separately as a batch report, not in the build.

### 5.1 Run cppcheck

```powershell
cppcheck `
  --enable=all `
  --suppress=missingIncludeSystem `
  --suppress=unmatchedSuppression `
  --inline-suppr `
  --std=c++17 `
  --platform=win64 `
  --template="{file}:{line}: [{severity}][{id}] {message}" `
  --output-file=logs\cppcheck.txt `
  -I eMule\srchybrid `
  -I eMule-id3lib\include `
  -I eMule-miniupnp `
  -I eMule-mbedtls\include `
  -D UNICODE -D _UNICODE -D WIN32 -D _WIN64 `
  -D ID3LIB_LINKOPTION=1 -D MINIUPNP_STATICLIB `
  eMule\srchybrid
```

Output lands in `logs\cppcheck.txt`. Review and fix; do not pipe to stderr/build
failure yet.

### 5.2 Using compile_commands.json with cppcheck

Once CMake migration is complete, prefer the compile-database form (more
accurate include resolution):

```powershell
cppcheck `
  --project=build\x64-debug\compile_commands.json `
  --enable=all `
  --suppress=missingIncludeSystem `
  --inline-suppr `
  --output-file=logs\cppcheck.txt
```

### 5.3 Suppressing false positives inline

```cpp
// cppcheck-suppress uninitvar
SomeType x;
DoSomethingWith(x);
```

---

## Phase 6 — MSVC AddressSanitizer (Debug builds only)

MSVC 2022 ships AddressSanitizer. It instruments the binary to catch:
heap buffer overflows, use-after-free, use-after-return, double-free,
stack buffer overflows, global buffer overflows.

Extremely valuable for a P2P application that processes untrusted network data.

### 6.1 Enable in CMake (Debug config only)

```cmake
if(MSVC)
    target_compile_options(eMule PRIVATE
        $<$<CONFIG:Debug>:/fsanitize=address>
    )
    # ASAN is incompatible with /RTC checks — disable in Debug when ASAN is on
    # (CMake sets /RTC1 by default in Debug; override it)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    # ASAN is also incompatible with static runtime — must use /MD in ASAN builds
    # Create a separate "asan" preset rather than modifying the regular Debug preset
endif()
```

> **Important**: MSVC ASAN requires the dynamic CRT (`/MD`/`/MDd`), not the
> static CRT (`/MT`/`/MTd`) used in the regular builds. The cleanest approach
> is a dedicated CMake preset:

Add to `CMakePresets.json`:

```json
{
  "name": "x64-asan",
  "displayName": "x64 ASAN (dynamic CRT)",
  "inherits": "base",
  "architecture": { "value": "x64", "strategy": "external" },
  "cacheVariables": {
    "CMAKE_BUILD_TYPE": "Debug",
    "EMULE_ASAN": "ON"
  }
}
```

And in `CMakeLists.txt`:

```cmake
option(EMULE_ASAN "Enable AddressSanitizer (requires dynamic CRT)" OFF)

if(EMULE_ASAN AND MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDebugDLL")
    target_compile_options(eMule PRIVATE /fsanitize=address)
    string(REPLACE "/RTC1" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
endif()
```

### 6.2 Running with ASAN

Build the `x64-asan` preset, then run `eMule.exe` normally. Violations print to
stderr and abort. The ASAN runtime DLLs must be alongside the binary; MSVC
copies them automatically when building with `/fsanitize=address`.

### 6.3 ASAN + MFC notes

ASAN is broadly compatible with MFC. Known issue: some MFC CRT init code
triggers ASAN false positives at process startup in very early heap operations.
If this occurs, add the ASAN suppression env var:

```powershell
$env:ASAN_OPTIONS = "detect_leaks=0"
```

Leak detection can be re-enabled once the startup noise is understood.

---

## Phase 7 — scan-build (Clang Static Analyzer)

`scan-build` wraps the build and runs Clang's interprocedural static analyzer,
which finds deeper bugs than clang-tidy checkers (e.g., cross-function null
deref, taint analysis for network input). Output is an HTML report.

### 7.1 Run

```powershell
# Must run from VS Developer Command Prompt
& "C:\Program Files\LLVM\bin\scan-build.bat" `
    --use-analyzer "C:\Program Files\LLVM\bin\clang.exe" `
    --html-title "eMule Static Analysis" `
    -o logs\scan-build-report `
    cmake --build build\x64-debug
```

Open `logs\scan-build-report\<date>\index.html` in a browser.

---

## Phase 8 — clang-include-cleaner

Finds `#include` directives that are unnecessary (included transitively) or
missing (used but not directly included). Reduces build times and makes
dependencies explicit.

```powershell
# Report mode (no writes)
clang-include-cleaner `
    -p build\x64-debug `
    eMule\srchybrid\SomeFile.cpp

# Apply changes (be conservative — review each file)
clang-include-cleaner `
    -p build\x64-debug `
    --edit `
    eMule\srchybrid\SomeFile.cpp
```

Run this after Wave 3 of clang-tidy, not before — modernization may add or
remove includes that would otherwise create churn.

---

## Recommended Execution Order

| Phase | Tool | Prerequisite | Commit scope |
|-------|------|-------------|-------------|
| 1 | CMake migration | Ninja installed | `build: migrate to CMake + Ninja` |
| 2 | clang-format baseline | Phase 1 | `style: apply clang-format baseline` |
| 3a | MSVC /sdl /permissive- /analyze (warnings) | Phase 1 | `build: add MSVC hardening flags` |
| 3b | MSVC /WX /analyze:WX | Phase 3a clean | `build: promote warnings to errors` |
| 4 Wave 1 | clang-tidy bug-finding | Phase 1 | one commit per logical fix group |
| 4 Wave 2 | clang-tidy security/cert | Wave 1 clean | — |
| 5 | cppcheck | Phase 1 | fix in same commits as Wave 2 |
| 4 Wave 3 | clang-tidy modernize (C++17) | Wave 2 clean | `refactor: apply clang-tidy modernize` |
| 4 Wave 4 | clang-tidy readability/perf | Wave 3 clean | — |
| 6 | ASAN Debug preset | Phase 1 | `build: add ASAN preset` |
| 7 | scan-build | Phase 1 | run periodically, fix as found |
| 8 | clang-include-cleaner | Wave 3 clean | `refactor: clean up includes` |

---

## Quick Reference — Tool Command Summary

```powershell
# Build
cmake --preset x64-debug && cmake --build --preset x64-debug
cmake --preset x64-release && cmake --build --preset x64-release

# Format check (no writes)
clang-format --dry-run --Werror eMule\srchybrid\SomeFile.cpp

# Format apply
clang-format -i eMule\srchybrid\SomeFile.cpp

# clang-tidy single file
clang-tidy -p build\x64-debug eMule\srchybrid\SomeFile.cpp

# clang-tidy all files
python "C:\Program Files\LLVM\bin\run-clang-tidy" -p build\x64-debug -j4 "eMule/srchybrid/.*\.cpp$"

# clang-tidy auto-fix
python "C:\Program Files\LLVM\bin\run-clang-tidy" -p build\x64-debug -fix "eMule/srchybrid/.*\.cpp$"

# cppcheck
cppcheck --project=build\x64-debug\compile_commands.json --enable=all --suppress=missingIncludeSystem --output-file=logs\cppcheck.txt

# scan-build
"C:\Program Files\LLVM\bin\scan-build.bat" -o logs\scan-build-report cmake --build build\x64-debug

# clang-include-cleaner
clang-include-cleaner -p build\x64-debug eMule\srchybrid\SomeFile.cpp

# ASAN build
cmake --preset x64-asan && cmake --build --preset x64-asan
```

---

## Feature Identifier

### PLAN_005: Code Quality Toolchain Migration

This document describes the code quality audit findings and the plan for migrating to a modern code quality toolchain, including:

- Static analysis integration (MSVC `/analyze`, clang-tidy)
- Compiler warning level escalation (`/W4` baseline, `/WX` target)
- Consistent formatting via `.clang-format`
- Automated CI checks for code quality regressions

**Status:** Audit findings documented. Toolchain migration is planned as part of the broader modernization effort (PLAN_002).
