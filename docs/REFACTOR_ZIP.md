# Refactor: Replace Custom ZIP/GZIP Readers with zlib minizip

## Motivation

The codebase contains two hand-rolled archive readers:

| File | Origin | Lines | Purpose |
|------|--------|-------|---------|
| `ZIPFile.cpp` / `.h` | Shareaza (2002-2004) | ~530 | Custom ZIP central-directory parser + deflate extractor |
| `GZipFile.cpp` / `.h` | eMule | ~120 | Thin wrapper around zlib `gz*` API |

Both can be replaced by code already present in the zlib dependency at `../../eMule-zlib/`:

- **ZIP** -> zlib's **minizip** (`contrib/minizip/unzip.h`) -- a mature, widely-used ZIP reader already bundled in our zlib tree.
- **GZIP** -> zlib's built-in **`gz*` API** (`gzopen`, `gzread`, `gzclose`) -- `GZipFile.cpp` already wraps these; the wrapper can be inlined at call sites or kept as a trivial facade.

### Why not 7za DLL?

- zlib is already linked -- zero new dependencies
- minizip is ~2 source files (`unzip.c` + `ioapi.c`), already sitting in `eMule-zlib/contrib/minizip/`
- 7za DLL is ~1.5 MB, needs COM-style interfaces (`IInArchive`, `IInStream`), and is overkill for "extract a file from a zip/gz"
- No runtime DLL distribution or version-matching concerns

---

## Current Consumers

### CZIPFile consumers (3 call sites)

**1. `IPFilterDlg.cpp` (~line 329-353)** -- IP filter import from local `.zip` file
```cpp
CZIPFile zip;
if (zip.Open(strFilePath)) {
    CZIPFile::File *zfile = zip.GetFile(_T("ipfilter.dat"));
    if (!zfile) zfile = zip.GetFile(_T("guarding.p2p"));
    if (!zfile) zfile = zip.GetFile(_T("guardian.p2p"));
    if (zfile) zfile->Extract(strTempUnzipFilePath);
    zip.Close();
}
```
**Pattern:** Open zip, find file by exact name, extract to temp path.

**2. `PPgSecurity.cpp` (~line 236-273)** -- IP filter download + extract from `.zip`
```cpp
CZIPFile zip;
if (zip.Open(strTempFilePath)) {
    CZIPFile::File *zfile = zip.GetFile(_T("ipfilter.dat"));
    // ... same fallback names ...
    if (zfile) zfile->Extract(strTempUnzipFilePath);
    zip.Close();
}
```
**Pattern:** Identical to above.

**3. `OtherFunctions.cpp` (~line 2879-2930)** -- Skin package (.zip) installation
```cpp
CZIPFile zip;
if (zip.Open(pszSkinPackage)) {
    // iterate all files, find *.eMuleSkin.ini
    for (int i = zip.GetCount(); --i >= 0;) {
        CZIPFile::File *zf = zip.GetFile(i);
        // check name suffix
    }
    // extract all files, creating subdirectories as needed
    for (int i = 0; i < zip.GetCount(); ++i) {
        CZIPFile::File *zf = zip.GetFile(i);
        zf->Extract(strDstFilePath);
    }
    zip.Close();
}
```
**Pattern:** Open zip, enumerate entries, extract multiple files and create directories.

### CGZIPFile consumers (3 call sites)

**4. `IPFilterDlg.cpp` (~line 389-409)** -- IP filter import from `.gz`
```cpp
CGZIPFile gz;
if (gz.Open(strFilePath)) {
    gz.Extract(strTempUnzipFilePath);
    gz.Close();
}
```

**5. `PPgSecurity.cpp` (~line 316-334)** -- IP filter download + extract from `.gz`
```cpp
CGZIPFile gz;
if (gz.Open(strTempFilePath)) {
    gz.Extract(strTempUnzipFilePath);
    gz.Close();
}
```

**6. `IP2Country.cpp` (~line 744)** -- GeoIP database extraction from `.gz`
```cpp
CGZIPFile gzipFile;
if (!gzipFile.Open(strArchivePath) || !gzipFile.Extract(strExtractedPath)) { ... }
```

---

## CZIPFile API -> minizip Equivalents

| CZIPFile method | minizip equivalent |
|---|---|
| `Open(path)` | `unzOpen64(path)` |
| `Close()` | `unzClose(uf)` |
| `GetCount()` | `unzGetGlobalInfo64(uf, &gi)` then `gi.number_entry` |
| `GetFile(index)` | `unzGoToFirstFile(uf)` + `unzGoToNextFile(uf)` loop |
| `GetFile(name)` | `unzLocateFile(uf, name, caseInsensitive)` |
| `File::m_sName` | `unzGetCurrentFileInfo64(uf, &fi, szName, ...)` |
| `File::m_nSize` | `fi.uncompressed_size` |
| `File::Extract(path)` | `unzOpenCurrentFile(uf)` + read loop + `unzCloseCurrentFile(uf)` |

---

## Implementation Plan

### Phase 1: Add minizip to the build

1. Add to `emule.vcxproj`:
   ```xml
   <ClCompile Include="..\..\eMule-zlib\contrib\minizip\unzip.c" />
   <ClCompile Include="..\..\eMule-zlib\contrib\minizip\ioapi.c" />
   ```
   On Windows, also add:
   ```xml
   <ClCompile Include="..\..\eMule-zlib\contrib\minizip\iowin32.c" />
   ```
   `iowin32.c` provides a Win32 `HANDLE`-based I/O backend, which integrates better than the default `fopen` backend and supports long paths natively.

2. Add the minizip include path to the project's Additional Include Directories:
   ```
   ..\..\eMule-zlib\contrib\minizip
   ```
   Or use relative includes in code: `#include "../../eMule-zlib/contrib/minizip/unzip.h"`

3. Verify the project compiles with minizip sources added (no functional changes yet).

### Phase 2: Write a thin wrapper (optional but recommended)

To keep consumer code clean and minimize churn, write a thin `MiniZipReader.h` / `.cpp` that mirrors the existing `CZIPFile` API surface but delegates to minizip internally. This is optional -- consumers can call minizip directly if preferred.

Suggested wrapper API:
```cpp
class CMiniZipReader
{
public:
    CMiniZipReader();
    ~CMiniZipReader();                          // calls Close()

    bool    Open(LPCTSTR pszFile);              // unzOpen64
    void    Close();                            // unzClose
    int     GetCount() const;                   // cached from unzGetGlobalInfo64

    // Find by exact name (case-insensitive)
    bool    LocateFile(LPCTSTR pszName);        // unzLocateFile

    // Enumerate
    bool    GoToFirstFile();                    // unzGoToFirstFile
    bool    GoToNextFile();                     // unzGoToNextFile
    bool    GetCurrentFileName(CString &sName); // unzGetCurrentFileInfo64
    ULONGLONG GetCurrentFileSize();             // unzGetCurrentFileInfo64

    // Extract current file to disk
    bool    ExtractCurrentTo(LPCTSTR pszDstPath);

private:
    unzFile m_uf;
    int     m_nFiles;
};
```

### Phase 3: Migrate consumers

For each of the 3 `CZIPFile` call sites, replace with `CMiniZipReader` (or direct minizip calls):

**IPFilterDlg.cpp and PPgSecurity.cpp** (sites 1 & 2 -- nearly identical):
```cpp
// Before:
CZIPFile zip;
if (zip.Open(strFilePath)) {
    CZIPFile::File *zfile = zip.GetFile(_T("ipfilter.dat"));
    if (!zfile) zfile = zip.GetFile(_T("guarding.p2p"));
    if (!zfile) zfile = zip.GetFile(_T("guardian.p2p"));
    if (zfile) zfile->Extract(strTempPath);
    zip.Close();
}

// After:
CMiniZipReader zip;
if (zip.Open(strFilePath)) {
    if (zip.LocateFile(_T("ipfilter.dat"))
        || zip.LocateFile(_T("guarding.p2p"))
        || zip.LocateFile(_T("guardian.p2p")))
    {
        zip.ExtractCurrentTo(strTempPath);
    }
    zip.Close();
}
```

**OtherFunctions.cpp** (site 3 -- skin installer, enumerates all entries):
```cpp
// Before:
CZIPFile zip;
if (zip.Open(pszSkinPackage)) {
    for (int i = zip.GetCount(); --i >= 0;) {
        CZIPFile::File *zf = zip.GetFile(i);
        if (zf && zf->m_sName matches suffix) { zfIniFile = zf; break; }
    }
    for (int i = 0; i < zip.GetCount(); ++i) {
        CZIPFile::File *zf = zip.GetFile(i);
        zf->Extract(dstPath);
    }
}

// After:
CMiniZipReader zip;
if (zip.Open(pszSkinPackage)) {
    // First pass: find the .eMuleSkin.ini
    CString sName;
    bool bFoundIni = false;
    for (bool ok = zip.GoToFirstFile(); ok; ok = zip.GoToNextFile()) {
        zip.GetCurrentFileName(sName);
        if (sName matches suffix) { bFoundIni = true; break; }
    }
    // Second pass: extract all
    if (bFoundIni) {
        for (bool ok = zip.GoToFirstFile(); ok; ok = zip.GoToNextFile()) {
            zip.GetCurrentFileName(sName);
            // ... path validation, directory creation ...
            zip.ExtractCurrentTo(dstPath);
        }
    }
}
```

### Phase 4: Simplify or keep GZipFile

`CGZIPFile` is already a thin wrapper over zlib's `gz*` API. Two options:

**Option A -- Keep it.** It's only ~80 lines of real code, uses zlib correctly, and provides convenience methods (`GetUncompressedFileName`). Low value in removing it.

**Option B -- Inline at call sites.** The 3 consumers each do `Open` + `Extract` + `Close`. Replace with direct `gzopen`/`gzread`/`gzclose` calls. The `GetUncompressedFileName()` helper (strips `.gz` extension) can be a free function or inlined.

**Recommendation:** Option A (keep). The wrapper is trivial, correct, and already uses zlib directly. Focus effort on the ZIP side.

### Phase 5: Remove old files

1. Delete from `srchybrid/`:
   ```
   ZIPFile.cpp
   ZIPFile.h
   ```

2. Remove from `emule.vcxproj`:
   ```xml
   <ClCompile Include="ZIPFile.cpp" />
   <ClInclude Include="ZIPFile.h" />
   ```

3. Remove `#include "ZIPFile.h"` from:
   - `IPFilterDlg.cpp`
   - `PPgSecurity.cpp`
   - `OtherFunctions.cpp`

4. If also removing `GZipFile` (Option B):
   - Delete `GZipFile.cpp`, `GZipFile.h`
   - Remove from `emule.vcxproj`
   - Remove `#include "GZipFile.h"` from `IPFilterDlg.cpp`, `PPgSecurity.cpp`, `IP2Country.cpp`

---

## Build Considerations

- minizip's `unzip.c` compiles as C, not C++. Ensure the vcxproj treats `.c` files correctly (default MSVC behavior is fine).
- `iowin32.c` uses `_wfopen` / Win32 file APIs. It handles Unicode paths natively.
- For long path support (`\\?\` prefix), either:
  - Pre-process paths with `PreparePathForWin32LongPath()` before passing to `unzOpen64()`, or
  - Use the `iowin32` backend which accepts wide strings via `unzOpen2_64()` with a custom `fill_fopen64_filefunc` using `CreateFileW`.
- minizip links against the same zlib already in the project -- no duplicate symbol risk.
- Compile minizip with `HAVE_AES` undefined (we don't need AES-encrypted ZIP support).

---

## Verification Checklist

- [ ] Project compiles with minizip sources added
- [ ] All 3 ZIP consumers migrated and tested:
  - [ ] IP filter import from `.zip` (IPFilterDlg -- local file dialog)
  - [ ] IP filter download from URL as `.zip` (PPgSecurity)
  - [ ] Skin package installation from `.zip` (OtherFunctions)
- [ ] All 3 GZIP consumers still work (if GZipFile kept) or migrated (if removed)
- [ ] `ZIPFile.cpp` / `.h` deleted and removed from project
- [ ] No remaining references: `grep -ri "CZIPFile\|ZIPFile\.h" srchybrid/ --include="*.cpp" --include="*.h"`
- [ ] ZIP files with stored (uncompressed) entries still extract correctly
- [ ] ZIP files with deflated entries still extract correctly
- [ ] Paths with Unicode characters work
- [ ] Long paths (>260 chars) work
