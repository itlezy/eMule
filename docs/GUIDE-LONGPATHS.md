# Enable Long Path Support

**Branch:** `v0.72a`
**Date:** 2026-03-28
**Reference implementation:** `eMuleAI` (at `/c/prj/p2p/eMule-my/analysis/eMuleAI/srchybrid/`)

This document describes every change required to allow eMule to share, hash, download, and manage files whose full paths exceed 260 characters (Windows `MAX_PATH`).

---

## Contents

1. [Background](#1-background)
2. [Prerequisites](#2-prerequisites)
3. [How it works](#3-how-it-works)
4. [Infrastructure — new helpers](#4-infrastructure--new-helpers)
5. [Initialization](#5-initialization)
6. [Manifests](#6-manifests)
7. [Call-site changes by file](#7-call-site-changes-by-file)
8. [Protocol and .met file impact](#8-protocol-and-met-file-impact)
9. [Limitations and known non-fixes](#9-limitations-and-known-non-fixes)
10. [Change summary table](#10-change-summary-table)

---

## 1. Background

Windows imposes a 260-character path limit (`MAX_PATH`) on most Win32 and CRT APIs by default. Two independent mechanisms must both be active to lift this limit:

- **System policy**: `HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled = 1` (registry, set by the user or by Group Policy on Windows 10 1607+)
- **Application manifest**: the `.exe` must declare `<ws2:longPathAware>true</ws2:longPathAware>` in its embedded manifest

When both are in place, Win32 APIs (`CreateFile`, `MoveFile`, `FindFirstFile`, etc.) accept paths up to ~32,767 characters. However, the MSVC CRT (`fopen`, `_wfopen`, `_open`, `_trename`, `_tremove`) does **not** benefit from the manifest flag — it has its own internal MAX_PATH checks. For CRT-based file operations, paths must be explicitly prefixed with `\\?\` (or `\\?\UNC\` for UNC paths) to bypass the limit.

Currently eMulebb has neither the manifest flag nor the `\\?\` prefix handling.

---

## 2. Prerequisites

- Windows 10 version 1607 (Anniversary Update) or later
- The system registry key must be set before eMule starts:
  ```
  HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled = 1 (DWORD)
  ```
  This can be set via Group Policy or directly in regedit. Without it the manifest flag alone does nothing for Win32 APIs, and the CRT helpers still need the `\\?\` prefix regardless.

---

## 3. How it works

eMuleAI's implementation has three layers:

**Layer 1 — Registry detection at startup**
`DetectWin32LongPathsSupportAtStartup()` reads the registry key once and stores the result in a process-global `bool g_bWin32LongPathsEnabled`. All subsequent checks are a simple bool test.

**Layer 2 — Path preparation**
`PreparePathForWin32LongPath(path)` prepends `\\?\` to any path when either:
- the OS policy is enabled (`g_bWin32LongPathsEnabled == true`), or
- the path is already at or beyond 260 characters

This makes it safe to call unconditionally — on short paths without the policy it is a no-op.

**Layer 3 — CRT-bypass open helpers**
`OpenFileStreamSharedReadLongPath` and `OpenCrtReadOnlyLongPath` open a file via `CreateFile` (which does respect the manifest) and attach the resulting OS handle to a CRT `FILE*` or fd via `_open_osfhandle` + `_fdopen`. This is the only way to give a `FILE*` to long-path filenames.

**Graceful degradation**: All guarded operations check `!IsWin32LongPathsEnabled() && path.GetLength() >= MAX_PATH` before attempting an operation that will definitely fail. They log a descriptive warning instead of silently failing or corrupting state.

---

## 4. Infrastructure — new helpers

### 4.1 OtherFunctions.cpp — new global variable and five functions

**File:** `srchybrid/OtherFunctions.cpp`
**Where:** Append to end of file (currently line 3962).

```cpp
static bool g_bWin32LongPathsEnabled = false;

bool IsWin32LongPathsEnabled()
{
    return g_bWin32LongPathsEnabled;
}

void DetectWin32LongPathsSupportAtStartup()
{
    // Detect HKLM\SYSTEM\CurrentControlSet\Control\FileSystem\LongPathsEnabled once at startup.
    HKEY hKey = NULL;
    DWORD dw = 0, type = 0, cb = sizeof(dw);

    if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Control\\FileSystem"), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueEx(hKey, _T("LongPathsEnabled"), NULL, &type, reinterpret_cast<LPBYTE>(&dw), &cb) == ERROR_SUCCESS && type == REG_DWORD)
            g_bWin32LongPathsEnabled = (dw != 0);

        RegCloseKey(hKey);
    }

    TRACE(_T("Win32 long paths support: %s\n"), g_bWin32LongPathsEnabled ? _T("enabled") : _T("disabled"));
}

CString PreparePathForWin32LongPath(const CString& path)
{
    // Always handle empty early.
    if (path.IsEmpty())
        return path;

    // Already prefixed.
    if (path.Left(4).CompareNoCase(_T("\\\\?\\")) == 0)
        return path;

    // If OS-wide long path policy is enabled, always use the long prefix for stability.
    // Otherwise, add the prefix only for overlong paths to bypass MAX_PATH limitations.
    const bool needPrefix = g_bWin32LongPathsEnabled || path.GetLength() >= MAX_PATH;
    if (!needPrefix)
        return path;

    // UNC path (\\server\share\...)
    if (path.Left(2) == _T("\\\\"))
        return _T("\\\\?\\UNC\\") + path.Mid(2);

    // Drive path (C:\...)
    return _T("\\\\?\\") + path;
}

// Long-path aware fopen replacement for read-only shared access.
FILE* OpenFileStreamSharedReadLongPath(const CString& path, bool bTextMode)
{
    CString prepared = PreparePathForWin32LongPath(path);
    HANDLE h = ::CreateFile(prepared, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return NULL;

    int fd = _open_osfhandle((intptr_t)h, _O_RDONLY | (bTextMode ? _O_TEXT : _O_BINARY));
    if (fd == -1) {
        ::CloseHandle(h);
        return NULL;
    }

    FILE* fp = _fdopen(fd, bTextMode ? "r" : "rb");
    if (fp == NULL) {
        _close(fd);
        return NULL;
    }

    return fp;
}

// Long-path aware CRT fd open for read-only binary access.
int OpenCrtReadOnlyLongPath(const CString& path)
{
    CString prepared = PreparePathForWin32LongPath(path);
    HANDLE h = ::CreateFile(prepared, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return -1;

    int fd = _open_osfhandle((intptr_t)h, _O_RDONLY | _O_BINARY);
    if (fd == -1) {
        ::CloseHandle(h);
        return -1;
    }

    return fd;
}
```

### 4.2 OtherFunctions.h — declarations

**File:** `srchybrid/OtherFunctions.h`
**Where:** Append to end of file (currently line 516, after the `RC4Crypt` declarations).

```cpp
bool IsWin32LongPathsEnabled();
void DetectWin32LongPathsSupportAtStartup();
CString PreparePathForWin32LongPath(const CString& path);
FILE* OpenFileStreamSharedReadLongPath(const CString& path, bool bTextMode);
int OpenCrtReadOnlyLongPath(const CString& path); // Returns CRT fd; -1 on failure
```

### 4.3 SafeFile.h — add OpenLongPath declaration to CSafeBufferedFile

**File:** `srchybrid/SafeFile.h`
**Where:** Inside the `CSafeBufferedFile` class body, after the existing constructor declaration (around line 142).

```cpp
bool OpenLongPath(LPCTSTR lpszFileName, UINT nOpenFlags); // Read-only long-path aware open
```

### 4.4 SafeFile.cpp — implement OpenLongPath

**File:** `srchybrid/SafeFile.cpp`
**Where:** Append after the existing `CSafeBufferedFile` section (after the class body, before `UINT CSafeBufferedFile::Read`).

```cpp
bool CSafeBufferedFile::OpenLongPath(LPCTSTR lpszFileName, UINT nOpenFlags)
{
    // Only read mode is supported for this helper.
    ASSERT((nOpenFlags & (CFile::modeRead | CFile::modeWrite | CFile::modeReadWrite)) == CFile::modeRead);
    CString prepared = PreparePathForWin32LongPath(lpszFileName);

    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if ((nOpenFlags & CFile::osSequentialScan) != 0)
        flags |= FILE_FLAG_SEQUENTIAL_SCAN;

    HANDLE h = ::CreateFile(prepared, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, flags, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;

    int fd = _open_osfhandle((intptr_t)h, _O_RDONLY | _O_BINARY);
    if (fd == -1) {
        ::CloseHandle(h);
        return false;
    }

    FILE* fp = _fdopen(fd, "rb");
    if (fp == NULL) {
        _close(fd);
        return false;
    }

    // Attach to CStdioFile/CFile internals.
    m_pStream = fp;
    HANDLE osHandleFromFd = (HANDLE)_get_osfhandle(_fileno(fp));
    m_hFile = (decltype(m_hFile))osHandleFromFd;
    m_bCloseOnDelete = TRUE;
    return true;
}
```

---

## 5. Initialization

**File:** `srchybrid/Emule.cpp`
**Where:** In `CemuleApp::InitInstance()`, immediately after the `AfxOleInit()` call (currently line 439).

```cpp
AfxOleInit();

DetectWin32LongPathsSupportAtStartup(); // Query OS long path support once per process.
```

This must run before any file I/O in the startup sequence.

---

## 6. Manifests

All three manifests need an `<application>` element added immediately before the closing `</assembly>` tag.

### 6.1 res/emulex64.manifest

Current last line: `</assembly>` (line 30).

Insert before that line:
```xml
  <!-- Enable Win32 long path support -->
  <application xmlns:asmv3="urn:schemas-microsoft-com:asm.v3">
    <asmv3:windowsSettings xmlns:ws2="http://schemas.microsoft.com/SMI/2016/WindowsSettings">
      <ws2:longPathAware>true</ws2:longPathAware>
    </asmv3:windowsSettings>
  </application>
```

### 6.2 res/emuleWin32.manifest

Same insertion before `</assembly>` (line 30). Identical XML block.

### 6.3 res/emuleARM64.manifest

Same insertion before `</assembly>` (line 26). Identical XML block.

---

## 7. Call-site changes by file

The 18 files below are listed in priority order: critical transfer I/O first, then directory/sharing code, then auxiliary features.

---

### 7.1 KnownFile.cpp — file hashing open

**Function:** `CKnownFile::CreateFromFile`
**Current (line 378):**
```cpp
FILE *file = _tfsopen(strFilePath, _T("rbS"), _SH_DENYNO);
if (!file) {
    LogError(GetResString(IDS_ERR_FILEOPEN) + _T(" - %s"), (LPCTSTR)strFilePath, _T(""), _tcserror(errno));
    return false;
}
```
**Replace with:**
```cpp
FILE* file = OpenFileStreamSharedReadLongPath(strFilePath, false);
if (!file) {
    LogError(GetResString(IDS_ERR_FILEOPEN) + _T(" - %s"), (LPCTSTR)strFilePath, _T(""), _tcserror(errno));
    return false;
}
```
**Why:** `_tfsopen` goes through the CRT and is blind to the manifest flag. `OpenFileStreamSharedReadLongPath` uses `CreateFile` + `_open_osfhandle` and respects the `\\?\` prefix.

---

### 7.2 UploadDiskIOThread.cpp — read handle for upload

**Function:** `CUploadDiskIOThread::AssociateFile`
**Current (lines 143–147):**
```cpp
pFile->m_hRead = ::CreateFile(fullname, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
if (pFile->m_hRead == INVALID_HANDLE_VALUE) {
    theApp.QueueDebugLogLineEx(LOG_ERROR, _T("Failed to open \"%s\" for overlapped read: %s"), (LPCTSTR)fullname, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));
    return false;
}
```
**Replace with:**
```cpp
const CString longFullname = PreparePathForWin32LongPath(fullname);
if (!IsWin32LongPathsEnabled() && fullname.GetLength() >= MAX_PATH) {
    theApp.QueueDebugLogLineEx(LOG_WARNING, _T("Skipped opening \"%s\" for upload - path too long (%u). Enable long path support to allow this."), (LPCTSTR)fullname, (UINT)fullname.GetLength());
    pFile->bNoNewReads = true;
    return false;
}
pFile->m_hRead = ::CreateFile(longFullname, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
if (pFile->m_hRead == INVALID_HANDLE_VALUE) {
    theApp.QueueDebugLogLineEx(LOG_ERROR, _T("Failed to open \"%s\" for overlapped read: %s"), (LPCTSTR)fullname, (LPCTSTR)GetErrorMessage(::GetLastError(), 1));
    return false;
}
```
**Why:** Without the prefix, `CreateFile` silently fails for paths ≥ 260 chars. The guard ensures that without the OS policy, an overlong path logs a clear warning and stops silently breaking uploads.

---

### 7.3 PartFileWriteThread.cpp — write handle for download

**Function:** `CPartFileWriteThread::AddFile`
**Current (lines 200–201):**
```cpp
const CString sPartFile(RemoveFileExtension(pFile->GetFullName()));
pFile->m_hWrite = ::CreateFile(sPartFile, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
```
**Replace with:**
```cpp
const CString sPartFile(RemoveFileExtension(pFile->GetFullName()));
const CString longPath = PreparePathForWin32LongPath(sPartFile);
pFile->m_hWrite = ::CreateFile(longPath, GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
```
**Why:** This is the IOCP write handle that receives every downloaded block. Failure to open it stops all writes for that partial file.

---

### 7.4 PartFile.cpp — part.met atomic write (SavePartFile)

The current code uses `_tremove` + `_trename` to atomically promote the `.tmp` file to the `.met` file. These CRT wrappers do not support `\\?\` paths. Replace with Win32 equivalents.

**Current (lines 1452–1467):**
```cpp
if (_tremove(m_fullname) != 0 && errno != ENOENT) {
    if (thePrefs.GetVerbose())
        DebugLogError(_T("Failed to remove \"%s\" - %s"), (LPCTSTR)m_fullname, _tcserror(errno));
}

if (_trename(strTmpFile, m_fullname) != 0) {
    int iErrno = errno;
    if (thePrefs.GetVerbose())
        DebugLogError(_T("Failed to move temporary part.met file \"%s\" to \"%s\" - %s"), (LPCTSTR)strTmpFile, (LPCTSTR)m_fullname, _tcserror(iErrno));
    // ... error path ...
    return false;
}
```
**Replace with:**
```cpp
const CString ldst = PreparePathForWin32LongPath(m_fullname);
::DeleteFile(ldst);
const CString lsrc = PreparePathForWin32LongPath(strTmpFile);

if (!MoveFileEx(lsrc, ldst, MOVEFILE_REPLACE_EXISTING)) {
    if (thePrefs.GetVerbose())
        DebugLogError(_T("Failed to move temporary part.met file \"%s\" to \"%s\" - %s"), (LPCTSTR)strTmpFile, (LPCTSTR)m_fullname, (LPCTSTR)GetErrorMessage(::GetLastError()));
    // ... error path ...
    return false;
}
```

**PartFile.cpp — part.met backup CopyFile (line 1470):**
```cpp
// Current:
if (!::CopyFile(m_fullname, m_fullname + PARTMET_BAK_EXT, static_cast<BOOL>(bDontOverrideBak)))

// Replace with:
if (!::CopyFile(PreparePathForWin32LongPath(m_fullname), PreparePathForWin32LongPath(m_fullname + PARTMET_BAK_EXT), static_cast<BOOL>(bDontOverrideBak)))
```

**PartFile.cpp — file completion (CompleteFile), category incoming path check (around line 2839):**
```cpp
// Current:
if (::PathFileExists(thePrefs.GetCategory(GetCategory())->strIncomingPath))
    indir = thePrefs.GetCategory(GetCategory())->strIncomingPath;

// Replace with (also guards against overlong incoming path):
const CString sCatIncoming(thePrefs.GetCategory(GetCategory())->strIncomingPath);
bool bUseCatIncoming = false;
if (!sCatIncoming.IsEmpty()) {
    if (IsWin32LongPathsEnabled() || sCatIncoming.GetLength() < MAX_PATH) {
        WIN32_FILE_ATTRIBUTE_DATA _wfdCat = { 0 };
        const CString _longCat = PreparePathForWin32LongPath(sCatIncoming);
        bUseCatIncoming = (::GetFileAttributesEx(_longCat, GetFileExInfoStandard, &_wfdCat) != 0) && (_wfdCat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    }
}
indir = bUseCatIncoming ? sCatIncoming : thePrefs.GetMuleDirectory(EMULE_INCOMINGDIR);
```

**PartFile.cpp — completion: collision check for existing file (lines 2855, 2891):**
```cpp
// Current:
bool renamed = ::PathFileExists(strNewname);
// ...
while (::PathFileExists(strTestName));

// Replace:
WIN32_FILE_ATTRIBUTE_DATA _wfdNew = { 0 };
bool renamed = (::GetFileAttributesEx(PreparePathForWin32LongPath(strNewname), GetFileExInfoStandard, &_wfdNew) != 0);
// ...
while (::GetFileAttributesEx(PreparePathForWin32LongPath(strTestName), GetFileExInfoStandard, &_wfdNew));
```

**PartFile.cpp — completion: MoveFileWithProgress (line 2898):**
```cpp
// Current:
if (::MoveFileWithProgress(strPartfilename, strNewname, CopyProgressRoutine, this, MOVEFILE_COPY_ALLOWED))

// Replace:
if (::MoveFileWithProgress(PreparePathForWin32LongPath(strPartfilename), PreparePathForWin32LongPath(strNewname), CopyProgressRoutine, this, MOVEFILE_COPY_ALLOWED))
```

**PartFile.cpp — completion: statUTC, DeleteFile for .met/.bak/.tmp (lines 2936, 2943, 2951, 2958):**
```cpp
// Current:
if (statUTC(strNewname, st) == 0)
if (!::DeleteFile(m_fullname))
if (!_taccess(BAKName, 0) && !::DeleteFile(BAKName))
if (!_taccess(TMPName, 0) && !::DeleteFile(TMPName))

// Replace:
if (statUTC(PreparePathForWin32LongPath(strNewname), st) == 0)
if (!::DeleteFile(PreparePathForWin32LongPath(m_fullname)))
if (!_taccess(BAKName, 0) && !::DeleteFile(PreparePathForWin32LongPath(BAKName)))
if (!_taccess(TMPName, 0) && !::DeleteFile(PreparePathForWin32LongPath(TMPName)))
```

---

### 7.5 SharedFileList.cpp — file sharing and directory enumeration

Six locations. These control which files appear in the shared files list and whether they can be hashed.

**Location 1 — Shared file path check (line 215):**
```cpp
// Current: no path length guard
const CString longResolved = PreparePathForWin32LongPath(resolved);
// ... FindFirstFile(longResolved)

// Add guard before the FindFirstFile call:
if (!IsWin32LongPathsEnabled() && resolved.GetLength() >= MAX_PATH)
    return false;
const CString longResolved = PreparePathForWin32LongPath(resolved);
```

**Location 2 — Directory enumeration pattern (line 821):**
```cpp
// Current:
CString pattern = cur + _T("*");

// Replace:
CString pattern = PreparePathForWin32LongPath(cur + _T("*"));
```

**Location 3 — Scan subdirectory search path (line 2265–2268):**
```cpp
// Current (skips overlong paths):
if (searchPath.GetLength() >= MAX_PATH)
    continue;
HANDLE hDir = ::FindFirstFile(searchPath + _T("*"), &wfd);

// Replace (no skip; prefix handles it):
// Do not skip overlong paths; PreparePathForWin32LongPath will add the needed prefix.
CString prepared = PreparePathForWin32LongPath(searchPath);
HANDLE hDir = ::FindFirstFile(prepared + _T("*"), &wfd);
```

**Location 4 — Subdirectory recursion (line 2378):**
```cpp
// Current:
HANDLE hFind = ::FindFirstFile(subDir + _T("*"), &wfd);

// Replace:
const CString longSubDir = PreparePathForWin32LongPath(subDir);
HANDLE hFind = ::FindFirstFile(longSubDir + _T("*"), &wfd);
```

**Location 5 — Import path check (line 2494–2499):**
```cpp
// Current: no guard
HANDLE h = ::FindFirstFile(raw + _T("*"), &wfd);

// Replace:
if (!IsWin32LongPathsEnabled() && raw.GetLength() >= MAX_PATH) {
    // log or skip
    return;
}
const CString longPath = PreparePathForWin32LongPath(raw);
HANDLE h = ::FindFirstFile(longPath + _T("*"), &wfd);
```

---

### 7.6 SharedDirsTreeCtrl.cpp — directory tree

**Location 1 — Directory scan (line 983):**
```cpp
// Current:
HANDLE hFind = ::FindFirstFile(search, &wfd);

// Replace:
CString searchLong = PreparePathForWin32LongPath(search);
HANDLE hFind = ::FindFirstFile(searchLong, &wfd);
```

**Location 2 — Directory existence check (line 1032):**
```cpp
// Current:
DWORD attr = ::GetFileAttributes(sDir);

// Replace:
CString longp = PreparePathForWin32LongPath(sDir);
DWORD attr = ::GetFileAttributes(longp);
```

---

### 7.7 SharedFilesCtrl.cpp — copy to clipboard / drag-drop

**Lines 1257–1258:**
```cpp
// Current:
::CopyFile(src, dst, FALSE);

// Replace:
const CString lsrc = PreparePathForWin32LongPath(src);
const CString ldst = PreparePathForWin32LongPath(dst);
::CopyFile(lsrc, ldst, FALSE);
```

---

### 7.8 Collection.cpp — collection file read/write

Four locations. Collections store ed2k link lists as files; paths within them may exceed MAX_PATH.

**Location 1 — Open collection file for reading (line 132–135):**
```cpp
// Current:
HANDLE hFile = ::CreateFile(sFilePath, GENERIC_READ, FILE_SHARE_READ | ..., NULL, OPEN_EXISTING, ...);

// Replace:
if (!IsWin32LongPathsEnabled() && sFilePath.GetLength() >= MAX_PATH)
    continue; // or skip/warn
const CString longPath = PreparePathForWin32LongPath(sFilePath);
HANDLE hFile = ::CreateFile(longPath, GENERIC_READ, FILE_SHARE_READ | ..., NULL, OPEN_EXISTING, ...);
```

**Locations 2, 3, 4 — Collection write/verify paths (lines 239, 319, 376):**
Each follows the pattern:
```cpp
// Current: unconditional operation on sFilePath
// Replace: guard + wrap
if (IsWin32LongPathsEnabled() || sFilePath.GetLength() < MAX_PATH) {
    const CString tLong = PreparePathForWin32LongPath(sFilePath);
    // ... original operation using tLong
}
```

---

### 7.9 ArchiveRecovery.cpp — archive preview extraction

Nine locations. All follow either a guard+wrap or a plain wrap pattern.

**Lines 132–137 — temp file path:**
```cpp
// Current: unconditional file open
// Replace:
if (!IsWin32LongPathsEnabled() && tempFileName.GetLength() >= MAX_PATH) {
    // log and return
}
CString tempOpenPath = PreparePathForWin32LongPath(tempFileName);
// use tempOpenPath for CreateFile / gzopen
```

**Lines 141–146 — source (part) file path:**
```cpp
if (!IsWin32LongPathsEnabled() && partFile->GetFilePath().GetLength() >= MAX_PATH) {
    // log and return
}
CString srcOpenPath = PreparePathForWin32LongPath(partFile->GetFilePath());
```

**Lines 176–183 — output file path:**
```cpp
if (!IsWin32LongPathsEnabled() && outputFileName.GetLength() >= MAX_PATH) {
    ::DeleteFile(PreparePathForWin32LongPath(tempFileName));
    // log and return
}
CString outOpenPath = PreparePathForWin32LongPath(outputFileName);
```

**Lines 180, 205, 226, 229 — cleanup DeleteFile calls:**
```cpp
// All become:
::DeleteFile(PreparePathForWin32LongPath(tempFileName));
::DeleteFile(PreparePathForWin32LongPath(outputFileName));
```

---

### 7.10 PartFileConvert.cpp — partial file import

Six locations. All are plain wraps of existing Win32 calls.

```cpp
// Line 313:
::DeleteFile(PreparePathForWin32LongPath(dataTarget));

// Line 317:
if (!GetFileAttributesEx(PreparePathForWin32LongPath(oldfile), GetFileExInfoStandard, &fad))

// Line 319:
HANDLE hFile = ::CreateFile(PreparePathForWin32LongPath(dataTarget), GENERIC_WRITE, ...);

// Line 336:
::DeleteFile(PreparePathForWin32LongPath(newfilename));

// Line 339:
MoveFile(PreparePathForWin32LongPath(sDir + partfile), PreparePathForWin32LongPath(newfilename));

// Line 341:
CopyFile(PreparePathForWin32LongPath(sDir + partfile), PreparePathForWin32LongPath(newfilename), FALSE);
```

---

### 7.11 Preview.cpp — preview file generation and reading

Four locations.

**Line 44 — CreateFile for preview:**
```cpp
CString prepared = PreparePathForWin32LongPath(path);
HANDLE h = ::CreateFile(prepared, ...);
```

**Lines 187–194 — preview temp file creation:**
```cpp
if (!IsWin32LongPathsEnabled() && strPreviewName.GetLength() >= MAX_PATH) {
    // log: preview impossible, path too long
    return false;
}
CString longPreviewName = PreparePathForWin32LongPath(strPreviewName);
// use longPreviewName for CreateFile
```

**Line 242 — DeleteFile for preview cleanup:**
```cpp
CString delPath = PreparePathForWin32LongPath(strPreviewName);
::DeleteFile(delPath);
```

**Line 293 — fopen replacement for preview reading:**
```cpp
// Current:
FILE* readFile = fopen(CT2A(strFilePath), "r");

// Replace:
FILE* readFile = OpenFileStreamSharedReadLongPath(strFilePath, true);
```

---

### 7.12 Preferences.cpp — incoming/temp directory scan

**Line 1556 — directory content scan:**
```cpp
// Current:
HANDLE hFind = ::FindFirstFile(cur + _T("*"), &wfd);

// Replace:
const CString pattern = PreparePathForWin32LongPath(cur + _T("*"));
HANDLE hFind = ::FindFirstFile(pattern, &wfd);
```

---

### 7.13 MediaInfo.cpp — media file metadata reading

Three locations. These affect the file info dialog for video/audio files.

**Line 640 — AVI RIFF header reading:**
```cpp
// Current:
int hAviFile = _open(CT2A(pszFileName), _O_RDONLY | _O_BINARY | _O_SEQUENTIAL);

// Replace:
int hAviFile = OpenCrtReadOnlyLongPath(pszFileName); // Long-path aware CRT fd open
```

**Line 1746 — general media file CreateFile:**
```cpp
// Current:
HANDLE hFile = ::CreateFile(pszFileName, dwDesiredAccess, dwShareMode, NULL, ...);

// Replace:
HANDLE hFile = ::CreateFile(PreparePathForWin32LongPath(pszFileName), dwDesiredAccess, dwShareMode, NULL, ...);
```

**Line 2759 — binary file open for media scan:**
```cpp
// Current:
int fd = _open(CT2A(pszFilePath), _O_RDONLY | _O_BINARY);

// Replace:
int fd = OpenCrtReadOnlyLongPath(pszFilePath); // Long-path aware fd open
```

---

### 7.14 HTRichEditCtrl.cpp — log/text file save

**Lines 582–587:**
```cpp
// Current: unconditional file save
// Replace: guard + wrap
if (!IsWin32LongPathsEnabled() && savePath.GetLength() >= MAX_PATH) {
    // log: cannot save, path too long
    return;
}
const CString longPath = PreparePathForWin32LongPath(savePath);
// use longPath for CreateFile / file.Open
```

---

### 7.15 ZIPFile.cpp — ZIP archive open and extract

**Line 62 — open ZIP for reading:**
```cpp
// Current:
HANDLE hFile = ::CreateFile(pszFile, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, ...);

// Replace:
const CString longPath = PreparePathForWin32LongPath(pszFile);
HANDLE hFile = ::CreateFile(longPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, ...);
```

**Line 354 — write extracted file:**
```cpp
// Current:
HANDLE hFile = ::CreateFile(pszFile, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, ...);

// Replace:
const CString outLong = PreparePathForWin32LongPath(pszFile);
HANDLE hFile = ::CreateFile(outLong, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, ...);
```

---

### 7.16 Emule.cpp — config directory backup

The config backup function (around line 2679) uses `GetFileAttributesEx`, `CreateDirectory`, `FindFirstFile`, `CopyFile` — all need wrapping. The function also iterates subdirectories recursively.

**Lines 2679–2756:**
```cpp
// Every Win32 path argument in this block becomes:
PreparePathForWin32LongPath(path)

// Specifically:
::GetFileAttributesEx(PreparePathForWin32LongPath(backupBase), ...)
::CreateDirectory(PreparePathForWin32LongPath(backupBase), NULL)
::FindFirstFile(PreparePathForWin32LongPath(pattern), &wfd)
::DeleteFile(PreparePathForWin32LongPath(oldestBackup))
::CreateDirectory(PreparePathForWin32LongPath(newBackupDir), NULL)
::FindFirstFile(PreparePathForWin32LongPath(pattern), &findData)
::CopyFile(PreparePathForWin32LongPath(src), PreparePathForWin32LongPath(dst), FALSE)
```

**Line 2795 — directory cleanup recursive scan:**
```cpp
// Current:
HANDLE hFind = FindFirstFile(szDirPath + _T("*"), &findFileData);

// Replace:
HANDLE hFind = FindFirstFile(PreparePathForWin32LongPath(szDirPath + _T("*")), &findFileData);
```

**Line 3325 — skin resource path:**
```cpp
// Current:
DWORD attr = ::GetFileAttributes(path);

// Replace:
const CString lpath = PreparePathForWin32LongPath(path);
DWORD attr = ::GetFileAttributes(lpath);
```

---

## 8. Protocol and .met file impact

### 8.1 .met file format — no format changes needed

Paths are stored in `.met` files (both `part.met` and `known.met`) as tag strings via `FT_FILEPATH`. The serialization uses `CFileDataIO::WriteString` / `ReadString`, which handle arbitrary-length Unicode strings. There is no length cap in the tag serialization code. A `.met` file written after enabling long paths contains the same structure as before, just with longer path strings in `FT_FILEPATH` tags.

**Backward compatibility:** A `known.met` or `part.met` written by a long-path-aware eMule can be read by an old eMule, but the old eMule will fail to open the actual file if the path exceeds 260 chars on that system. The `.met` file itself is not corrupted or unreadable.

### 8.2 ED2K and Kademlia protocol — not affected

The ED2K and Kademlia protocols exchange _filenames_ (not full paths). Filenames are communicated as arbitrary UTF-8 strings in search results and source exchange. There is no MAX_PATH constraint in the protocol wire format. The application-level logic that builds a full path from `incoming_dir + filename` can produce paths of any length; the changes in this document are precisely what handle those paths correctly.

### 8.3 Network-received filenames — no constraint added

eMule does not truncate filenames received from the network to fit within MAX_PATH. If a downloaded file would land at an overlong path and long path support is not enabled, the completion move (`MoveFileWithProgress`) will fail with a Win32 error, and eMule logs it. The changes in section 7.4 make the error message explicit when this happens and allow the move to succeed when the OS policy is enabled.

### 8.4 CRT path buffers — one residual risk

`SharedFileList.cpp` line 424 uses a fixed `TCHAR strFilePath[MAX_PATH]` buffer with `_tmakepathlimit`. This is a stack buffer that cannot hold paths longer than MAX_PATH. It is used only for the hash-thread logging message (not for the actual file open), but it is a potential truncation if the combined directory + filename exceeds 260 chars. eMuleAI does not fix this specific buffer; it is a known residual limitation.

Similarly, `SharedFileList.cpp` line 328 uses `_tmakepath(strFilePath.GetBuffer(MAX_PATH), ...)` in the part-import path. This is for display/logging only and does not gate the actual file open.

---

## 9. Limitations and known non-fixes

| Limitation | Detail |
|---|---|
| Windows 10 1607+ only | `longPathAware` manifest has no effect on earlier Windows. The `\\?\` prefix wrapping still works on older versions for Win32 calls, but there is no system-wide policy to enable. |
| CRT APIs without `\\?\` prefix | The `TCHAR strFilePath[MAX_PATH]` stack buffers in SharedFileList.cpp (lines 328, 424) cannot be fixed without refactoring the surrounding code. These affect display strings and logging only, not actual I/O. |
| `SHFileOperation` | Does not support the `\\?\` prefix. Any code using `SHFileOperation` for file moves/deletes (e.g. recycle bin operations) remains limited to MAX_PATH. eMuleAI notes this explicitly in `OtherFunctions.cpp` around the file-delete helper. |
| Shell dialogs (folder picker, file open dialog) | `CFileDialog` and `SHBrowseForFolder` have their own MAX_PATH constraints in some Windows versions. Users may not be able to browse to long paths via UI pickers even when the rest of the app handles them correctly. |
| `_tmakepath` / `PathCombine` | These Shell helper functions do not support `\\?\` prefixed paths. Calls to them must be replaced with manual string concatenation when long paths are involved. |

---

## 10. Change summary table

| File | Changes | Type |
|---|---|---|
| `OtherFunctions.h` | Add 5 function declarations | New API |
| `OtherFunctions.cpp` | Add `g_bWin32LongPathsEnabled`, 5 functions | New API |
| `SafeFile.h` | Add `OpenLongPath` declaration to `CSafeBufferedFile` | New method |
| `SafeFile.cpp` | Implement `CSafeBufferedFile::OpenLongPath` | New method |
| `Emule.cpp` | Call `DetectWin32LongPathsSupportAtStartup()` in `InitInstance` + 9 path wraps in backup code | Init + wrap |
| `res/emulex64.manifest` | Add `<ws2:longPathAware>true</ws2:longPathAware>` | Manifest |
| `res/emuleWin32.manifest` | Same | Manifest |
| `res/emuleARM64.manifest` | Same | Manifest |
| `KnownFile.cpp` | Replace `_tfsopen` with `OpenFileStreamSharedReadLongPath` | CRT → Win32 |
| `PartFile.cpp` | Replace `_tremove`/`_trename` with Win32 `DeleteFile`/`MoveFileEx`; wrap 9 other Win32 calls | CRT → Win32 + wraps |
| `PartFileWriteThread.cpp` | Wrap `CreateFile` with `PreparePathForWin32LongPath` | Wrap |
| `UploadDiskIOThread.cpp` | Add length guard + wrap `CreateFile` | Guard + wrap |
| `SharedFileList.cpp` | 6 locations: add guards and wrap `FindFirstFile` patterns | Guards + wraps |
| `SharedDirsTreeCtrl.cpp` | 2 locations: wrap `FindFirstFile`, `GetFileAttributes` | Wraps |
| `SharedFilesCtrl.cpp` | Wrap `CopyFile` src and dst | Wrap |
| `Collection.cpp` | 4 locations: add guards + wrap `CreateFile` | Guards + wraps |
| `ArchiveRecovery.cpp` | 9 locations: add guards + wrap `CreateFile`, `DeleteFile` | Guards + wraps |
| `PartFileConvert.cpp` | 6 locations: wrap `DeleteFile`, `GetFileAttributesEx`, `CreateFile`, `MoveFile`, `CopyFile` | Wraps |
| `Preview.cpp` | 4 locations: wrap `CreateFile`, `DeleteFile`; replace `fopen` | CRT → Win32 + wraps |
| `Preferences.cpp` | Wrap `FindFirstFile` pattern | Wrap |
| `MediaInfo.cpp` | 3 locations: replace `_open` with `OpenCrtReadOnlyLongPath`; wrap `CreateFile` | CRT → Win32 |
| `HTRichEditCtrl.cpp` | Add length guard + wrap file open | Guard + wrap |
| `ZIPFile.cpp` | 2 locations: wrap `CreateFile` | Wraps |

**Totals:** 23 files, ~55 individual change locations, 0 protocol changes, 0 .met format changes.

---

## Feature Identifier

### FEAT_031: Long Path Support Implementation

This document covers the Windows long path support (paths exceeding MAX_PATH / 260 characters) in eMulebb. Implementation involves:

- Enabling the long path aware manifest flag
- Using `\?\` prefix or wide-character APIs where needed
- Registry configuration guidance for enabling system-wide long path support on Windows 10+

**Status:** **[PARTIAL]** — Long path helpers implemented and active in critical paths (commits `f3781f8`, `ea009a3`, `85000bb`):
- `OpenCrtReadOnlyLongPath()` helper in use for file I/O (e.g., MediaInfo.cpp)
- Shared-file discovery and completion paths updated
- Manifest `LongPathAware` declaration not yet added
- Remaining: systematic audit of all `MAX_PATH`-dependent call sites
