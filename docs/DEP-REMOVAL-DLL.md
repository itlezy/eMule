# eMule v0.72a — Dependency DLL Conversion Analysis

**Target platform: Windows 10+ x64 ONLY**
**Build: MSVC v143 (VS 2022), static MFC, `/MT` CRT**
**Source analysis: `c:\prj\p2p\eMule\analysis\bbclean7`**

---

## Table of Contents

- [Context](#context)
- [Dependency Inventory](#dependency-inventory)
- [1. Crypto++ 8.9.0](#1-crypto-890--dep_001) — Medium risk
- [2. id3lib 3.9.1](#2-id3lib-391--dep_002) — Do not convert
- [3. miniupnpc 2.3.3](#3-miniupnpc-233--dep_003) — **Good DLL candidate**
- [4. ResizableLib](#4-resizablelib-master--v153--dep_005) — Incompatible
- [5. zlib 1.3.2](#5-zlib-132--dep_004) — **Good DLL candidate**
- [6. MbedTLS + TF-PSA-Crypto](#6-mbedtls-400--tf-psa-crypto-100--dep_006-stale--removed) — **[STALE — REMOVED]**
- [Summary Table](#summary-table)
- [Recommended Action Plan](#recommended-action-plan)

---

## Context

All current dependencies are linked as **static `.lib`** files. This document evaluates whether each
dependency can be replaced with a DLL-linked variant, what that would require, and whether it is
advisable given the constraints.

Windows 10+ x64-only targeting eliminates several legacy concerns (XP CRT, x86 alignment, 32-bit
pointer truncation, old SDK API shims) and simplifies the feasibility analysis.

---

## Dependency Inventory

| # | Library | Version | Link type | Purpose | Removal ref |
|---|---------|---------|-----------|---------|-------------|
| 1 | **Crypto++** | 8.9.0 | Static `.lib` | RSA/AES/MD5/hash — client credits, crypto ops | `DEP_001` in DEP-REMOVAL.md |
| 2 | **id3lib** | 3.9.1 | Static `.lib` | ID3 tag read/write for media files | `DEP_002` in DEP-REMOVAL.md |
| 3 | **miniupnpc** | 2.3.3 | Static `.lib` | UPnP port mapping / NAT traversal | `DEP_003` in DEP-REMOVAL.md |
| 4 | **ResizableLib** | master | Static `.lib` | MFC resizable dialogs | `DEP_005` in DEP-REMOVAL.md |
| 5 | **zlib** | 1.3.2 | Static `.lib` (CMake) | Deflate/gzip compression | `DEP_004` in DEP-REMOVAL.md |
| 6 | ~~**MbedTLS**~~ | ~~4.0.0~~ | ~~Static `.lib`~~ | ~~TLS 1.3, X.509, WebSocket security~~ **[REMOVED]** | `DEP_006` in DEP-REMOVAL.md |
| 7 | ~~**TF-PSA-Crypto**~~ | ~~1.0.0~~ | ~~Static `.lib`~~ | ~~PSA Crypto API layer under MbedTLS~~ **[REMOVED]** | `DEP_006` in DEP-REMOVAL.md |

---

## 1. Crypto++ 8.9.0 — `DEP_001`

### Current setup
- Build: `eMule-cryptopp\cryptlib.vcxproj` (MSVC Utility project, v143)
- Output: `x64\Release\cryptlib.lib` / `x64\Debug\cryptlib.lib`
- CRT: `/MT` Release, `/MTd` Debug — patched to match eMule
- Includes: headers pulled directly (`rsa.h`, `md5.h`, etc.)

### DLL availability
Crypto++ ships a second project `cryptdll.vcxproj` in the same source tree producing
`cryptlib.dll` + an import `cryptlib.lib`. This is an official upstream distribution mode — not a
third-party hack.

### What DLL linking would require

1. Build `cryptdll.vcxproj` instead of (or alongside) `cryptlib.vcxproj`
2. The DLL must be compiled with `/MD` (it is a DLL, CRT must be shared at the DLL boundary)
3. eMule itself remains `/MT` but links the Crypto++ **import lib** — this is a valid mixed
   configuration on Windows 10+; the CRT boundary is respected as long as no heap objects cross
   the DLL/exe boundary (allocated in DLL, freed in exe)
4. Remove `CRYPTOPP_STATICLIB` preprocessor define in eMule (or swap to `CRYPTOPP_DLL`)
5. Deploy `cryptlib.dll` alongside `emule.exe`

### Risk assessment — MEDIUM

**What works well:**
- C API surface for symmetric ciphers, hashing, and RNG is stable across versions
- No heap-object ownership transfer for hash/cipher use (eMule creates objects, uses them,
  destroys them in the same module)
- Windows 10 x64 eliminates all `__cdecl` vs `__stdcall` concerns (x64 has one calling convention)

**What is risky:**
- Crypto++ exposes **C++ class hierarchies** in headers (`CryptoPP::HashTransformation`,
  `CryptoPP::SymmetricCipher`, etc.). The vtable layout must match exactly between the DLL and
  eMule's compile unit. Any rebuild of the DLL without rebuilding eMule can silently break this.
- Template-heavy codebase: many algorithms are header-inlined. These inline instantiations land in
  eMule's code segment, not the DLL. Mixing header-inlined and DLL-exported paths for the same
  algorithm is error-prone.
- Credit system (`ClientCredits.cpp`) uses RSA via inline templates — these would still be compiled
  into eMule, bypassing the DLL entirely for that code path

**Verdict:** Technically possible but fragile due to C++ class export surface. The security
sensitivity of the credit system makes ABI drift hard to test. **Keep static unless upstream
ships versioned ABI guarantees.**

---

## 2. id3lib 3.9.1 — `DEP_002`

### Current setup
- Build: `eMule-id3lib\libprj\id3lib.vcxproj` (v143)
- Output: `x64\Release\id3lib.lib`
- CRT: `/MT` Release, `/MTd` Debug
- Depends on: zlib (path patched to `../../eMule-zlib`)

### DLL availability
id3lib has a shared library build option, but:
- Upstream is effectively **dead** — last real release 2003, last meaningful commit ~2016
- The `itlezy/eMule-id3lib` fork used here has no releases and no DLL builds
- No pre-built DLLs exist from any reputable source

### What DLL linking would require
1. Add `__declspec(dllexport)` annotations to id3lib's public headers (none exist)
2. Decide how to handle zlib dependency: static-into-DLL or cascade DLL
3. Write a custom vcxproj DLL configuration from scratch
4. Ship `id3lib.dll` + `zlib1.dll` alongside `emule.exe`

### Risk assessment — VERY HIGH / NOT RECOMMENDED

- **Unmaintained**: no security patches will ever arrive for id3lib. DLL version pinning locks
  in whatever bugs exist at time of build.
- **C++ ABI**: id3lib exposes C++ objects (`ID3_Tag`, `ID3_Frame`). Vtable/layout mismatches
  between builds will crash at runtime, often non-deterministically.
- **No upstream DLL support**: adding dllexport annotations to a dead library's headers is pure
  maintenance burden with zero benefit.
- **Usage scope is narrow**: only invoked for media file tagging. The added complexity of a DLL
  for a minor feature is not worth it.

**Verdict: Keep static. Do not convert.**

---

## 3. miniupnpc 2.3.3 — `DEP_003`

### Current setup
- Build: `eMule-miniupnp\miniupnpc\msvc\miniupnpc.vcxproj` (v143)
- Output: `x64\Release\miniupnpc.lib`
- CRT: `/MT` Release (patched from `/MD`)
- Preprocessor: `MINIUPNP_STATICLIB` in eMule when linking static
- Upstream already has `Release Dll` and `Debug Dll` configurations in the same vcxproj

### DLL availability
**Yes, officially.** miniupnpc ships `Release Dll` / `Debug Dll` / `x64 Release Dll` / `x64 Debug
Dll` configurations in `miniupnpc.vcxproj`. The DLL exports a pure **C API** (`upnpDiscover`,
`UPNP_AddPortMapping`, etc.) — no C++ objects cross the boundary.

### What DLL linking would require

1. Build the `x64 Release Dll` configuration of `miniupnpc.vcxproj` — zero patching needed for
   this config (it already uses `/MD` by design, correct for a DLL)
2. In eMule's vcxproj:
   - Remove `MINIUPNP_STATICLIB` from `PreprocessorDefinitions`
   - Change `AdditionalDependencies` from `miniupnpc.lib` (static) to the import lib from the
     DLL build (same name, different artifact)
3. Deploy `miniupnpc.dll` alongside `emule.exe`
4. `ws2_32.lib` and `iphlpapi.lib` stay in eMule's linker input (they are already there)

### Risk assessment — LOW

**Why it is safe:**
- Pure C API — no C++ object ownership, no vtable, no template instantiation across boundary
- Windows 10 x64 single calling convention — zero `__cdecl`/`__stdcall` mismatch risk
- miniupnpc is actively maintained (v2.3.3, May 2025) with stable ABI across minor versions
- The upstream already validates the DLL build path; it is not experimental
- UPnP is an optional feature (if DLL missing, eMule can gracefully degrade — already handled
  in `Upnp.cpp` with error returns from `upnpDiscover`)

**Minor concerns:**
- `miniupnpc.dll` must be deployed; adds one file to distribution
- DLL uses `/MD` CRT; fine for a DLL, but eMule's heap and miniupnpc's heap are separate —
  do not pass allocated pointers across the boundary expecting the other side to free them
  (current eMule code does not do this; `freeUPNPDevlist`, `FreeUPNPUrls` are called in the
  same module that called `upnpDiscover`)

**Verdict: GOOD DLL CANDIDATE. Lowest-effort conversion with solid payoff.**

---

## 4. ResizableLib (master / v1.5.3+) — `DEP_005`

### Current setup
- Build: `eMule-ResizableLib\ResizableLib\ResizableLib.vcxproj` (v143)
- Output: `x64\Release\resizablelib.lib`
- CRT: `/MT`, MFC: Static — patched from inconsistent upstream configs
- Nature: C++ MFC extension library; deeply uses `CWnd`, `CDialog`, `CControlBar` internals

### DLL availability
No official DLL distribution. ResizableLib is designed as a static companion to MFC applications.
Its classes inherit directly from MFC dialog/window classes and inline into the application.

### What DLL linking would require
1. Create an **MFC Extension DLL** build (special MSVC project type)
2. Add `AFX_EXT_CLASS` to every public class declaration
3. Change eMule to dynamic MFC (`/MD` + dynamic MFC DLL)
   — this is **a fundamental rebuild change** that affects every MFC class in the entire
   application, not just ResizableLib
4. Or: keep static MFC and accept that an MFC Extension DLL linked against static MFC will
   produce duplicate MFC state → guaranteed crash

### Risk assessment — INCOMPATIBLE / DO NOT ATTEMPT

- eMule uses **static MFC** (`UseOfMfc=Static`). MFC Extension DLLs require **shared MFC**.
  These two models are mutually exclusive.
- Switching eMule to shared MFC would require validating every `CDynLinkLibrary`, `AFX_MODULE_STATE`,
  `AfxGetApp()`, resource handle, and global MFC state across the entire codebase. This is a
  multi-week refactoring effort for zero user-visible gain.
- ResizableLib is small (< 50 KB compiled). Static linking cost is negligible.

**Verdict: Do not convert. Structurally incompatible with eMule's MFC model.**

---

## 5. zlib 1.3.2 — `DEP_004`

### Current setup
- Build: CMake-generated MSBuild projects (CMake generator: VS 17 2022)
- Wrapper: `templates\zlib\zlib.vcxproj` (Utility project)
  - PreBuildEvent: `cmake -S ... -B eMule-zlib\cmake-build` (configure once)
  - PostBuildEvent: `cmake --build ... --target zlibstatic` → copies `zs.lib` → `zlib.lib`
- Output: `x64\Release\zlib.lib`
- CRT: `/MT` via CMake flag `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded[Debug]`

### DLL availability
**Yes, officially.** zlib's CMake build supports `BUILD_SHARED_LIBS=ON` producing `zlib1.dll` +
import lib `zdll.lib`. This is the most widely deployed DLL on Windows (ships with countless
applications and some Windows components).

### What DLL linking would require

1. Change CMake configure command in `zlib.vcxproj` PostBuildEvent:
   ```
   cmake --build ... --target zlib   (not zlibstatic)
   ```
   Or set `BUILD_SHARED_LIBS=ON` and rebuild
2. Copy resulting `zdll.lib` → `zlib.lib` and `zlib1.dll` to output directory
3. In eMule's vcxproj: no preprocessor changes needed (zlib API is identical static/shared)
4. Update `templates\zlib\zlib.vcxproj` PostBuildEvent to copy the DLL to the output alongside
   `emule.exe`
5. **id3lib also links zlib**: if zlib becomes a DLL, id3lib's static build must link the zlib
   import lib (not the static `.lib`), otherwise two copies of zlib exist (one in id3lib.lib,
   one as DLL) — harmless on Windows 10 x64 but wastes memory

### Risk assessment — VERY LOW

**Why it is safe:**
- Pure C API — one of the most stable ABIs on the planet
- `zlib1.dll` at version 1.3.x is ABI-compatible with any code compiled against 1.x headers
- Windows 10 x64 ships a system-level zlib-compatible DLL in some frameworks (not guaranteed
  for `zlib1.dll` specifically, but close)
- CMake change is a one-line flag

**Minor concerns:**
- Two zlib consumers (eMule + id3lib): need to ensure both link same zlib (import lib or DLL),
  not a static copy baked into id3lib.lib alongside a separate DLL
- `zlib1.dll` must be deployed with `emule.exe`
- CMake CRT override (`/MT`) applies to static targets — for DLL build, zlib must use `/MD`
  (correct for DLLs); the CMake flag would need to be adjusted for the DLL target only

**Verdict: GOOD DLL CANDIDATE. Trivial CMake change, excellent ABI stability.**

---

## 6. MbedTLS 4.0.0 + TF-PSA-Crypto 1.0.0 — `DEP_006` **[STALE — REMOVED]**

**Status:** Both MbedTLS and TF-PSA-Crypto were fully removed in commit `6a1c440` as part of the SMTP + embedded web-server purge. This entire section is kept for historical reference only.

### Current setup (historical)
Six static libraries combined via `lib.exe /OUT:` into one aggregate `mbedtls.lib`:

| Sub-library | Source | Role |
|------------|--------|------|
| `mbedtls.lib` | `library\` | Core TLS 1.2/1.3 stack |
| `mbedx509.lib` | `library\` | X.509 certificate processing |
| `tfpsacrypto.lib` | `tf-psa-crypto\core\` | PSA Crypto core |
| `builtin.lib` | `tf-psa-crypto\drivers\builtin\` | Algorithm implementations |
| `everest.lib` | `tf-psa-crypto\drivers\everest\` | Everest ECC driver |
| `p256m.lib` | `tf-psa-crypto\drivers\p256-m\` | P-256 curve driver |

- Build: CMake → VS2017-named dirs; `templates\mbedtls\mbedTLS.vcxproj` drives all 6
- Special: `workspace.ps1` post-processes all 6 generated `.vcxproj` files to force `/MT`/`/MTd`
- Special: `threading_alt.h` patch enables `MBEDTLS_THREADING_C` + `MBEDTLS_THREADING_ALT`
  using Windows `CRITICAL_SECTION` for thread safety
- eMule preprocessor: `MBEDTLS_ALLOW_PRIVATE_ACCESS` (accesses `private/sha1.h` internals)
- Extra: eMule links `bcrypt.lib` explicitly (MbedTLS calls `BCryptGenRandom` for entropy)

### DLL availability
MbedTLS CMake supports `USE_SHARED_MBEDTLS_LIBRARY=ON` for DLL builds. However:
- TF-PSA-Crypto v1.0.0 is new (bundled with MbedTLS 4.0); its DLL mode is less tested
- The 6-library aggregate is non-standard; DLL mode would produce 6 separate DLLs or one large
  combined DLL (requires custom CMake work)

### What DLL linking would require

1. CMake: `USE_SHARED_MBEDTLS_LIBRARY=ON`, `USE_STATIC_MBEDTLS_LIBRARY=OFF`
2. Remove `/MT` post-processing in `workspace.ps1` for MbedTLS (DLLs must use `/MD`)
3. Rewrite the `lib.exe` aggregate PostBuildEvent — no longer valid for DLL output
4. Replace with copy of 6 (or more) DLLs to output directory
5. Remove `MBEDTLS_ALLOW_PRIVATE_ACCESS` from eMule — **this accesses struct internals that
   are opaque in DLL builds**. The code using `private/sha1.h` must be rewritten to use the
   public PSA API only. See also `GAP_005` in AUDIT-CODEREVIEW.md.
6. Validate `threading_alt.h` is visible to the DLL (not just eMule) — currently the patch
   adds the file to the source tree; must be in the DLL's include path too
7. Ensure `bcrypt.dll` is present (always present on Windows 10+ — no deployment concern)
8. Update eMule linker: replace one aggregate `mbedtls.lib` with up to 6 import libs

### Risk assessment — VERY HIGH / NOT RECOMMENDED

**Hard blockers:**

- **`MBEDTLS_ALLOW_PRIVATE_ACCESS`**: eMule currently reads `mbedtls_sha1_context` internals
  directly. In a DLL, these structs are opaque. This is not a linker error — it will compile
  and then access wrong memory offsets at runtime. A full rewrite of the affected code paths
  to use PSA Crypto API is required before even attempting DLL conversion.

- **6-library interdependency**: The sub-libraries have circular symbol dependencies resolved
  only because `lib.exe` merges them into one archive. As separate DLLs, each must be compiled
  in the correct order and each DLL must explicitly link the others. Getting CMake to express
  this correctly for MSVC DLL targets is non-trivial.

- **`threading_alt.h`**: The custom threading backend using `CRITICAL_SECTION` must be
  consistent between eMule and the DLL. If the DLL is ever replaced (updated), it must be
  recompiled with the same `threading_alt.h`. There is no standard packaging for this.

- **TF-PSA-Crypto 1.0.0 / MbedTLS 4.0.0 version lock**: These two must be exactly paired.
  Any DLL versioning scheme that allows independent updates of one without the other is a
  latent crash. DLL deployment normally assumes components can be updated independently.

- **Security ABI drift**: TLS handshake state (`mbedtls_ssl_context`) is a large stack-allocated
  struct. Its layout is compiled into eMule. Any DLL rebuild that changes struct layout (e.g.
  a security patch) without rebuilding eMule produces silent memory corruption.

**Verdict: Do not convert. The private API access alone is a hard blocker. Keep as static
aggregate until the sha1 private access is refactored to the public PSA API — at which point
the risk is reduced from VERY HIGH to HIGH (threading and version lock concerns remain).**

---

## Summary Table

| Library | Version | DLL Available | Feasibility | Effort | Verdict | Removal ref |
|---------|---------|--------------|-------------|--------|---------|-------------|
| **miniupnpc** | 2.3.3 | Yes (official) | High | Low | **DO IT** | `DEP_003` |
| **zlib** | 1.3.2 | Yes (official) | High | Low | **DO IT** | `DEP_004` |
| **Crypto++** | 8.9.0 | Yes (official) | Medium | Medium | Consider — test ABI | `DEP_001` |
| **id3lib** | 3.9.1 | No (dead upstream) | Low | Very High | **Keep static** | `DEP_002` |
| **ResizableLib** | master | No | None | N/A | **Incompatible** | `DEP_005` |
| ~~**MbedTLS + TF-PSA**~~ | ~~4.0.0 / 1.0.0~~ | N/A | N/A | N/A | **[REMOVED]** | `DEP_006` |

---

## Recommended Action Plan

### Phase 1 — Easy wins (low risk, low effort)

#### miniupnpc → DLL (`DEP_003`)

1. In `eMule-miniupnp\miniupnpc\msvc\miniupnpc.vcxproj`, build configuration
   `x64 Release Dll` (already exists upstream, no patching needed)
2. Collect output: `miniupnpc.dll` + import lib
3. In `emule.vcxproj`:
   - Remove `MINIUPNP_STATICLIB` from `PreprocessorDefinitions`
   - Update `AdditionalDependencies` to use the import lib
4. Add `miniupnpc.dll` to distribution / install target
5. Test: UPnP discovery, port mapping, error fallback when UPnP unavailable

#### zlib → DLL (`DEP_004`)

1. In `templates\zlib\zlib.vcxproj` PostBuildEvent, change target from `zlibstatic` to `zlib`
   and add `-DBUILD_SHARED_LIBS=ON` to the CMake configure step
2. Update CRT cmake flag: for shared build use `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded` is
   still valid but the DLL target will correctly use `/MD` — verify cmake output
3. Collect: `zlib1.dll` + `zdll.lib` → rename/copy to `zlib.lib` for linker
4. Decide id3lib: link id3lib against zlib import lib (not static zlib archive)
5. Add `zlib1.dll` to distribution
6. Test: download/upload compression, file preview, any gzip-encoded HTTP response

### Phase 2 — Optional (evaluate after Phase 1)

#### Crypto++ → DLL (conditional) (`DEP_001`)

Only pursue if the Crypto++ version needs frequent updates (security patches) and the rebuild
cost of a full static relink is unacceptable. Otherwise the static build is fine.

Preconditions:
- Audit all eMule code that uses Crypto++ headers for template-inlined algorithms
- Ensure no `CryptoPP::` heap objects are allocated in one module and freed in another
- Test RSA credit verification end-to-end against a known-good peer after conversion

### ~~Phase 3 — Future~~ **[STALE — REMOVED]**

~~#### MbedTLS → DLL (blocked) (`DEP_006`)~~

MbedTLS and TF-PSA-Crypto were fully removed in commit `6a1c440`. This phase is no longer applicable.

---

## Windows 10+ x64 Notes

The following legacy concerns are **not applicable** and can be ignored:

- `__stdcall` vs `__cdecl` calling convention mismatch → x64 has one convention, irrelevant
- `_declspec(dllimport)` on 32-bit thunks → x64 linker handles this transparently
- XP-era `LoadLibrary` path length limits → Windows 10 long paths enabled
- `/arch:SSE2` alignment for DLL data → x64 guarantees 16-byte stack alignment at call sites
- WoW64 / 32-bit DLL in 64-bit process → not possible, not a concern
- Windows 7 SDK `LoadLibraryEx` flags → `LOAD_LIBRARY_SEARCH_*` flags fully available on Win10

The Windows 10 x64 ABI is stable, well-specified, and makes DLL interop for **pure C APIs**
(miniupnpc, zlib) essentially risk-free.
