# Dependency Removal Analysis

**Branch:** `v0.72a`
**Date:** 2026-03-28
**Scope:** All six third-party dependencies linked into `emule.exe` on this branch.

This document covers each dependency in full: what it is, where it is used, what breaks on removal, and what realistic alternatives exist.

---

## Contents

1. [Crypto++ (cryptopp)](#1-crypto-cryptopp) — **`DEP_001`**
2. [Mbed TLS (mbedtls)](#2-mbed-tls-mbedtls--dep_006-done--removed) — **`DEP_006` [DONE - REMOVED]**
3. [id3lib](#3-id3lib) — **`DEP_002`**
4. [miniupnpc (miniupnp)](#4-miniupnpc-miniupnp) — **`DEP_003`**
5. [zlib](#5-zlib) — **`DEP_004`**
6. [ResizableLib](#6-resizablelib) — **`DEP_005`**
7. [Removal Impact Matrix](#7-removal-impact-matrix)

---

## 1. Crypto++ (cryptopp) — `DEP_001`

**Status: Keep, critical**

### What it is

Crypto++ (`weidai11/cryptopp`, pinned at `CRYPTOPP_8_9_0`) is a comprehensive C++ cryptographic library. It provides hash functions, big-integer arithmetic, asymmetric cryptography primitives, and a cryptographically secure random number generator. The workspace carries a local `emule-build-v0.72a` build branch that normalizes the VS output path and forces the `v143` toolset.

### Usage in eMule

| Source file | Crypto++ API used | Purpose |
|---|---|---|
| `MD4.h` | `CryptoPP::Weak::MD4` | ED2K file hashing (protocol standard) |
| `MD5Sum.h/.cpp` | `CryptoPP::Weak::MD5` | Key derivation in connection encryption handshake |
| `SHA.h` | `CryptoPP::SHA1` | AICH (Advanced Integrity Check Hash) file verification |
| `EncryptedStreamSocket.h/.cpp` | `CryptoPP::Integer`, `CryptoPP::a_exp_b_mod_c`, `CryptoPP::AutoSeededRandomPool` | Diffie-Hellman 768-bit key agreement for obfuscated TCP connections |
| `EncryptedDatagramSocket.cpp` | `CryptoPP::AutoSeededRandomPool` | Random key material for UDP encryption |
| `ClientCredits.h/.cpp` | `CryptoPP::RSASSA_PKCS1v15_SHA_Signer`, `CryptoPP::AutoSeededRandomPool` | RSA-based client credit signature generation and verification |
| `Collection.h/.cpp` | `CryptoPP::RSASSA_PKCS1v15_SHA_Signer`, `CryptoPP::AutoSeededRandomPool` | RSA signing of collection files |
| `Preferences.cpp` | `CryptoPP::AutoSeededRandomPool` | Secure key generation for user crypto credentials |
| `kademlia/utils/UInt128.cpp` | `CryptoPP::AutoSeededRandomPool` | Random Kademlia node ID initialization |

#### Crypto++ usage depth

The dependency is embedded across all three cryptographic layers of the protocol:

- **Hashing layer** — `MD4` is the `ed2k:` link hash, making it literally the file identity in the protocol. `MD5` is used for handshake key material derivation. `SHA1` is used for AICH subtree hashing across 9.5 MB blocks.
- **Key agreement layer** — Diffie-Hellman with a fixed 768-bit prime is implemented manually using `CryptoPP::Integer` and `a_exp_b_mod_c`. This is the obfuscation encryption negotiation for both TCP and UDP connections (`OP_EDONKEYPROT` 0xD4 packets).
- **Asymmetric signature layer** — `RSASSA_PKCS1v15_SHA_Signer` is the RSA signer used for the client credit system. Each peer generates a 384-bit RSA keypair on first run. Upload credits are signed with RSA, and each client verifies before accepting credits.

### Impact of removal

Removing Crypto++ would break all four of the above areas simultaneously:

- ED2K file identification collapses (no MD4 → no `ed2k://` hash)
- AICH integrity verification fails (no SHA1 → no subtree hash)
- All obfuscated connections fail to negotiate (no DH integer math)
- The credit system loses its signature capability entirely

The application would not be buildable at all without a source-level replacement for every call site, because all usages are direct API calls with CryptoPP types in headers (not hidden behind an abstraction layer). The types `CryptoPP::Integer`, `CryptoPP::RSASSA_PKCS1v15_SHA_Signer`, and `CryptoPP::AutoSeededRandomPool` appear directly in class member declarations in `.h` files.

**Removal verdict: Not feasible without a full rewrite of the cryptographic layer.**

### Alternatives

| Alternative | Notes |
|---|---|
| **OpenSSL / libssl** | Provides MD4, MD5, SHA1, DH, RSA, and a CSPRNG. Mature, widely used, actively maintained. However, it ships dynamic libraries by default and the eMule build requires static `/MT`. A static OpenSSL build on Windows is non-trivial. The API surface is C-only and would require substantial rewrites at all call sites, since eMule uses CryptoPP's C++ object model heavily. |
| **Botan** | C++ cryptographic library like Crypto++, with a similar object-oriented API. Provides all needed primitives. Less Windows-native build story than Crypto++; would require similar vcxproj plumbing work. Not an obvious improvement over Crypto++ for this project. |
| **Windows CNG (BCrypt API)** | Natively available on Windows 7+. eMule still links `bcrypt.lib`, so the platform RNG is available without adding another third-party crypto dependency. CNG provides MD4, MD5, SHA1, RSA, and a CSPRNG, but its API is low-level C and would require writing substantial wrapper code to replace the C++ object model. It also ties the build to Windows exclusively, which may not be a concern here but is a trade-off. |

**Recommended path if Crypto++ must be removed:** Either migrate to Windows CNG or adopt a new library such as OpenSSL or Botan. Any option still requires rewriting `MD4.h`, `MD5Sum.h`, `SHA.h`, `EncryptedStreamSocket`, `ClientCredits`, `Collection`, and `Preferences`. Estimated effort: high.

---

## 2. Mbed TLS (mbedtls) -- `DEP_006` [DONE - REMOVED]

**Status: Removed in commit `6a1c440` as part of the SMTP + embedded web-server purge**

Mbed TLS was only used by the optional SMTP notifier and the embedded web server. Those features, their UI, their settings, `TLSthreading`, and the workspace wrapper/build plumbing were removed on 2026-03-30.

### Removal outcome

- `emule.exe` no longer links `mbedtls.lib`
- `workspace.ps1` and `deps.psd1` no longer configure or build `eMule-mbedtls`
- `WebSocket.cpp`, `WebServer.cpp`, `PPgWebServer.cpp`, `SendMail.cpp`, and `TLSthreading.cpp` are gone
- Core P2P behavior is unchanged because the dependency was never used by ED2K/Kad traffic

**Removal verdict: Completed with no protocol impact.**

---

## 3. id3lib — `DEP_002` [DONE — REMOVED]

**Status: Removed in commit `907e675` — MP3 metadata unified on MediaInfo**

### What it is

id3lib (`itlezy/eMule-id3lib`, pinned at `v3.9.1`) is a C library for reading and writing ID3v1 and ID3v2 metadata tags in MP3 files. The library is effectively frozen — the last commit on the workspace fork is from February 2019 and there are no upstream releases. The workspace patch retargets the zlib include path from `../zlib` to `../eMule-zlib` and upgrades the vcxproj from `v142` to `v143`.

The library is the weakest maintenance point in the workspace: it is old, has no active upstream, and is the most likely to accumulate build friction over time as MSVC evolves.

### Usage in eMule

| Source file | id3lib API used | Purpose |
|---|---|---|
| `FileInfoDialog.cpp` | `ID3_Tag`, `ID3_FrameID`, `ID3_GetStringW`, `ID3FN_TEXT`, `ID3FN_DESCRIPTION`, `ID3FN_URL`, `ID3FN_LANGUAGE`, `ID3FN_MIMETYPE`, `ID3FN_IMAGEFORMAT`, `ID3FN_FILENAME`, `ID3FN_OWNER` | Displays full ID3v1/v2 tag contents in the file detail dialog for MP3 files |
| `KnownFile.cpp` | `ID3_Tag`, `id3/tag.h`, `id3/misc_support.h` | Extracts artist/title/album for file metadata indexing |

#### id3lib usage depth

Usage is confined to exactly two files with no transitive headers pulling in id3lib elsewhere. The two usage sites are:

1. **`FileInfoDialog.cpp`** — A large block (~180 lines) that iterates all ID3 frames and appends tag data into a display list view. This is purely cosmetic: it populates a tab in the file properties dialog when the user inspects an MP3. It is guarded by a check for the `.mp3` extension. Removing this block leaves a stub that shows no tag data for MP3s in the UI, but does not affect any P2P functionality.

2. **`KnownFile.cpp`** — A smaller block that populates the `strTitle` and `strArtist` metadata fields from ID3 tags. These fields feed the search index and the file comment/metadata display. Removing this block means MP3 metadata is not indexed or shown, but file transfer works normally.

### Impact of removal

- MP3 metadata (artist, title, album, etc.) is no longer shown in the file detail dialog
- File search by MP3 metadata fields stops working
- No P2P protocol impact whatsoever
- Build simplification: removes the most unmaintained dependency in the workspace

**Removal verdict: Fully feasible with minimal source changes. Lowest risk of all removals.**

### Alternatives

| Alternative | Notes |
|---|---|
| **taglib** (`taglib/taglib`) | The de facto successor to id3lib. Actively maintained, supports ID3v1, ID3v2.3, ID3v2.4, and many other formats (FLAC, Ogg, MP4). Has a well-maintained Windows/MSVC build. C++ API. Would be a natural drop-in replacement for the two usage sites and would also add support for non-MP3 formats. The main cost is workspace integration work (submodule, patch, vcxproj). |
| **libid3tag** (MAD project) | C library, part of the MAD MP3 decoder project. Lighter than taglib but covers ID3v1/v2. Less actively maintained than taglib. |
| **Windows Shell IPropertyStore** | The Windows Shell property system can read MP3 metadata via `IPropertyStore` / `PSGetPropertyValue`. No additional library required. Limited to what the Windows Media metadata handler exposes, but covers title/artist/album. Tying the feature to Windows-only APIs is acceptable given this is a Windows-only build. |
| **Remove the feature entirely** | Remove the two id3lib-dependent blocks and show no MP3 tag data. Simplest option. No library needed. |

**Recommended path:** Replace with **taglib** if MP3 metadata display is considered important. Otherwise, remove the feature entirely and drop the dependency. Either path is straightforward compared to the other dependency changes.

---

## 4. miniupnpc (miniupnp) — `DEP_003`

**Status: Keep, easy to DLL-ify**

### What it is

miniupnpc (`miniupnp/miniupnp`, pinned at `miniupnpc_2_3_3`) is a small C library that implements the UPnP Internet Gateway Device (IGD) protocol for automatic port mapping. The workspace carries a local `emule-build-v0.72a` branch that adds x64 static configs, switches the `PreBuildEvent` to `cscript //nologo`, sets `/MT`/`/MTd` CRT, fixes the output directory layout, and replaces the deprecated `_memicmp` with `_strnicmp`.

### Usage in eMule

| Source file | miniupnpc API used | Purpose |
|---|---|---|
| `UPnPImplMiniLib.h/.cpp` | `miniupnpc.h`, `upnpcommands.h`, `UPNPUrls`, `IGDdatas`, `UPNP_AddPortMapping`, `UPNP_GetExternalIPAddress`, `FreeUPNPUrls`, `DiscoverDevices` | Concrete UPnP IGD implementation via miniupnpc |

The UPnP subsystem has a clean three-layer abstraction:

```
UPnPImplWrapper   — public interface used by the rest of eMule (~72 files reference UPnP settings)
UPnPImpl          — abstract base class
UPnPImplMiniLib   — miniupnpc concrete implementation  ← only file that touches miniupnpc
UPnPImplWinServ   — Windows native UPnP implementation (uses <upnp.h>, no miniupnpc)
```

Only `UPnPImplMiniLib.cpp` includes miniupnpc headers. The rest of eMule uses the `UPnPImplWrapper` interface and never sees the library directly.

#### miniupnpc usage depth

The library is used for:

- **Router discovery** — `DiscoverDevices` / `upnpDiscover` to find UPnP-capable gateways on the LAN
- **Port mapping** — `UPNP_AddPortMapping` to open TCP (ED2K TCP port) and UDP (ED2K UDP port and Kad UDP port) mappings
- **External IP** — `UPNP_GetExternalIPAddress` to learn the WAN IP seen by the router
- **Cleanup** — `UPNP_DeletePortMapping` and `FreeUPNPUrls` for teardown

All of this runs in a dedicated `CStartDiscoveryThread` thread and is effectively optional at runtime — the user can disable UPnP in preferences.

### Impact of removal

- Automatic port forwarding no longer works via miniupnpc
- Users must configure port forwarding manually in their router
- External IP detection via UPnP is unavailable
- No protocol impact: ED2K and Kademlia connections still work as long as the router is configured manually or the Windows UPnP implementation (`UPnPImplWinServ`) is used instead

Because of the clean abstraction, removing `UPnPImplMiniLib` without removing the UPnP feature entirely is straightforward: only the concrete implementation class needs to be removed from the build and the fallback `UPnPImplWinServ` (Windows native) left in place.

**Removal verdict: Fully feasible. The abstraction layer makes this the cleanest removal in the codebase.**

### Alternatives

| Alternative | Notes |
|---|---|
| **UPnPImplWinServ (already in eMule)** | `UPnPImplWinServ` is a second concrete implementation that uses the Windows built-in UPnP API (`<upnp.h>`, `iphlpapi.lib`). It is already present in the codebase and already linked in the project. Switching the `UPnPImplWrapper` to use `UPnPImplWinServ` instead of `UPnPImplMiniLib` as the default eliminates the miniupnpc dependency entirely with a one-line change in `UPnPImplWrapper`. |
| **libupnp (pupnp)** | `pupnp/pupnp` is a larger UPnP SDK. More capable than miniupnpc but significantly heavier. Not a good trade for this use case. |
| **Remove UPnP entirely** | Remove both `UPnPImplMiniLib` and `UPnPImplWrapper`; stub out the UPnP init call. Users configure port forwarding manually. Simplest outcome. |

**Recommended path:** Switch `UPnPImplWrapper` to use `UPnPImplWinServ` as the default. Zero source impact outside the wrapper. The Windows-native implementation already exists and handles the same port mapping use case without any external library. Drop `UPnPImplMiniLib.cpp/.h` from the project file and remove the miniupnpc submodule.

---

## 5. zlib — `DEP_004`

**Status: Keep**

### What it is

zlib (`madler/zlib`, pinned at `v1.3.2`) is the standard data compression library. Upstream 1.3.x removed the `contrib/vstudio/` Visual Studio project files, so the workspace `setup` step materializes a workspace-owned cmake-generated VS project that builds `zlibstatic` and copies the output. CMake on `PATH` is a hard prerequisite.

### Usage in eMule

| Source file | zlib API used | Purpose |
|---|---|---|
| `Packets.cpp` | `compress2`, `uncompress`, `Z_BEST_COMPRESSION`, `Z_OK`, `Z_BUF_ERROR` | Compresses and decompresses ED2K protocol packets (OP_PACKEDPROT `0xD4`) |
| `DownloadClient.cpp` | `z_stream`, `inflate`, `inflateInit`, `inflateEnd`, `Z_SYNC_FLUSH`, `Z_OK`, `Z_STREAM_END` | Per-block streaming decompression of compressed file data received from a client |
| `ClientUDPSocket.cpp` | `uncompress` | Decompresses Kademlia UDP packets that have the compression flag set |
| `GZipFile.cpp` | `gzopen`, `gzread`, `gzclose`, `gzdirect` | Reading `.gz` compressed update/server-list files (e.g., `server.met.gz`) |
| `ZIPFile.cpp` | `z_stream`, `inflateInit2`, `inflate`, `inflateEnd` | ZIP archive extraction (for skin/mod ZIP files) |
| `HttpDownloadDlg.cpp` | `z_stream`, `inflate*` | Transparent decompression of gzip-encoded HTTP responses from servers |
| `ArchiveRecovery.cpp` | `z_stream`, `inflate*`, `gzopen`, `gzread`, `gzclose` | Recovering and reading gzip-compressed archives for preview functionality |

#### zlib usage depth

zlib is used at three distinct levels:

1. **Protocol level (critical)** — ED2K compressed packets (`OP_PACKEDPROT`) use zlib `compress2`/`uncompress` to reduce packet size. Both sending and receiving peers must agree to this compression. This is negotiated via the `EM_SUPPORTS_ZLIB` capability flag. If decompression fails, the packet is dropped and the connection may be terminated. This is the only usage that is core-protocol-critical.

2. **File transfer level (important)** — `DownloadClient.cpp` uses streaming `inflate` to decompress compressed file data blocks. This is the zlib-per-block download compression negotiated between clients. Without it, downloads from clients that negotiate compression would fail or need to fall back to uncompressed blocks.

3. **Utility level (non-critical)** — GZip file reading, ZIP archive extraction, HTTP response decompression, and archive recovery. These are quality-of-life features that can be stubbed without breaking file transfers.

### Impact of removal

- Compressed ED2K protocol packets cannot be handled — eMule would need to not advertise `EM_SUPPORTS_ZLIB` and drop any compressed packet from peers that send them without negotiation. This is a protocol degradation.
- Compressed block downloads fail — fall back to uncompressed exchange or drop compressed blocks.
- `.gz` server list files cannot be read — users must use uncompressed server lists.
- ZIP skins/mods cannot be loaded.
zlib is a deeply integrated transport-layer dependency. Unlike the removed SMTP/web features, the ED2K protocol compression is exercised on every active connection to a modern eMule peer.

**Removal verdict: Technically feasible with significant protocol degradation. Not recommended.**

### Alternatives

| Alternative | Notes |
|---|---|
| **Windows API zlib (zlib1.dll)** | Windows 8+ includes `zlib1.dll` as part of Cabinet.dll's deflate support accessible via `CreateDecompressor`/`Decompress` (Cabinet API). Not a compatible API; would require new wrapper code for all eight usage sites. Ties the feature to specific Windows versions. |
| **miniz** | Single-header, MIT-licensed, API-compatible with zlib. Drop-in replacement for most zlib call sites. Actively maintained. No separate build step required — copy `miniz.h` and `miniz.c` into the source tree. The `gzopen`/`gzread` high-level interface is not included in miniz's default build but is in `miniz_tdef`/`miniz_tinfl`. Most eMule usage would be directly compatible; `GZipFile.cpp` uses gzip high-level APIs that need verification. |
| **lz4** | Much faster than zlib but uses a different format and is not compatible with zlib's stream format. Cannot replace zlib transparently for the protocol. Would require protocol-level changes. |
| **libdeflate** | High-performance zlib/gzip-compatible implementation. Faster decompression than stock zlib. Compatible API for `compress`/`decompress`. Does not provide the streaming `inflate`/`deflate` API used by `DownloadClient.cpp` and `ZIPFile.cpp`. |

**Recommended path if zlib must be removed:** Replace with **miniz**. It is the only API-compatible option with zero additional library build overhead. The call sites in `Packets.cpp`, `ClientUDPSocket.cpp`, `DownloadClient.cpp`, and `HttpDownloadDlg.cpp` are directly compatible. `GZipFile.cpp` and `ArchiveRecovery.cpp` use the `gz*` high-level gzip functions which need minor adaptation. Estimated effort: low to medium.

---

## 6. ResizableLib — `DEP_005`

**Status: Keep, low priority**

### What it is

ResizableLib (`ppescher/resizablelib`, tracked at `master`) is an MFC extension library that provides base classes for dynamically resizable dialog windows, property sheets, and form views. The workspace patch moves the project off `v141_xp`/SDK 8.1 to `v143`/SDK 10.0 and adds the x64 Unicode static-MFC configurations eMule requires.

The library is lightly maintained (last formal release `v1.5.3` in June 2020), but is stable legacy code covering a well-understood problem space. Low churn is expected and acceptable.

### Usage in eMule

ResizableLib affects 31 source/header files spread across the entire dialog/window layer:

| Class used | Include | Files |
|---|---|---|
| `CResizableDialog` | `ResizableLib/ResizableDialog.h` | `AddSourceDlg`, `ChatWnd`, `CollectionCreateDialog`, `CollectionViewDialog`, `DialogMinTrayBtn`, `DirectDownloadDlg`, `IPFilterDlg`, `IrcWnd`, `KademliaWnd`, `NetworkInfoDlg`, `PartFileConvert`, `ServerWnd`, `SharedFilesWnd`, `StatisticsDlg`, `TrayDialog`, `TransferWnd` |
| `CResizablePage` | `ResizableLib/ResizablePage.h` | `ArchivePreviewDlg`, `ClientDetailDialog`, `CommentDialog`, `CommentDialogLst`, `ED2kLinkDlg`, `FileDetailDialogInfo`, `FileDetailDialogName`, `FileDetailDlgStatistics`, `FileInfoDialog`, `MetaDataDlg`, `PreviewDlg` |
| `CResizableSheet` | `ResizableLib/ResizableSheet.h` | `ClientDetailDialog`, `ListViewWalkerPropertySheet`, `SearchListCtrl`, `SharedFilesCtrl` |
| `CResizableFormView` | `ResizableLib/ResizableFormView.h` | `SearchResultsWnd` |

#### ResizableLib usage depth

Every dialog and property sheet in eMule inherits from a ResizableLib base class. The library provides the resizing/layout logic; eMule's dialogs provide nothing equivalent themselves. The dependency is at the base-class level in `.h` files, meaning it is compiled into every translation unit that includes a dialog header.

Despite being in 31 files, all usages are inheritance-level only. No ResizableLib-specific layout methods (such as `AddAnchor`) appear to be heavily used beyond what the base constructors wire up automatically. The library is effectively an invisible substrate for the entire UI layer.

### Impact of removal

- All dialog windows lose resizability — they become fixed-size, as MFC dialogs are by default
- No protocol impact
- No file transfer impact
- The build requires replacing all `CResizableDialog`, `CResizablePage`, `CResizableSheet`, and `CResizableFormView` base classes with plain MFC equivalents (`CDialog`, `CPropertyPage`, `CPropertySheet`, `CFormView`) in all 31 files
- The `AddAnchor` and `ArrangeLayout` calls in dialog `OnInitDialog` handlers must be removed

**Removal verdict: Feasible but high source-file churn. Purely a UI quality-of-life trade-off.**

### Alternatives

| Alternative | Notes |
|---|---|
| **Plain MFC base classes** | `CDialog`, `CPropertyPage`, `CPropertySheet`, `CFormView`. No external library. Dialogs become non-resizable. This is how pre-ResizableLib eMule worked. Removing ResizableLib and returning to plain MFC base classes eliminates the dependency at the cost of a regression in UI ergonomics across 31 dialogs. |
| **In-source fork** | Since ResizableLib's maintenance is light, the needed classes (`ResizableDialog.h/.cpp`, `ResizablePage.h/.cpp`, `ResizableSheet.h/.cpp`) could be copied directly into the eMule source tree. This eliminates the submodule and patch overhead while preserving the feature. The library is MIT-licensed, making this straightforward. The four files needed total around 1,500 LOC. |
| **MFC Layout Manager (WTL)** | WTL (Windows Template Library) provides similar dialog layout functionality. Not a drop-in for MFC; would require interface-level changes. |
| **Custom MFC anchor-based resizing** | Re-implementing the anchor model that ResizableLib uses is feasible but is reinventing the wheel. The in-source fork is strictly better than this option. |

**Recommended path:** If the submodule overhead is the concern, **copy the required ResizableLib classes directly into the eMule source tree** (in-source inclusion). The library is MIT-licensed, small, and stable. This eliminates the submodule, the patch, and the build project while preserving all functionality with zero source changes to the dialog files. If resizable dialogs are acceptable to drop, return to plain MFC bases across all 31 files.

---

## 7. Removal Impact Matrix

| Dependency | Core P2P protocol | File transfer | Kademlia DHT | Web interface | Email notify | GUI dialogs | MP3 metadata | UPnP NAT |
|---|:---:|:---:|:---:|:---:|:---:|:---:|:---:|:---:|
| **Crypto++** (`DEP_001`) | BREAKS | BREAKS | BREAKS | — | — | — | — | — |
| **Mbed TLS** (`DEP_006`) | — | — | — | removed | removed | — | — | — |
| **id3lib** (`DEP_002`) | — | — | — | — | — | — | removed | — |
| **miniupnpc** (`DEP_003`) | — | — | — | — | — | — | — | manual only |
| **zlib** (`DEP_004`) | degrades | degrades | degrades | degrades | — | — | — | — |
| **ResizableLib** (`DEP_005`) | — | — | — | — | — | non-resizable | — | — |

### Removal difficulty

| Dependency | ID | Effort | Risk | Recommended action |
|---|---|---|---|---|
| **id3lib** | `DEP_002` | Low | Very low | Remove or replace with taglib / Windows Shell IPropertyStore |
| **miniupnpc** | `DEP_003` | Very low | Very low | Switch to `UPnPImplWinServ` (already in codebase) |
| **ResizableLib** | `DEP_005` | Medium (31 files) | Low | Inline into source tree, or drop resizable dialogs |
| **Mbed TLS** | `DEP_006` | Done | Low | Removed together with SMTP and the embedded web server |
| **zlib** | `DEP_004` | Medium | Medium | Keep; or replace with miniz for same API |
| **Crypto++** | `DEP_001` | Very high | Critical | Keep; or plan a dedicated migration to a new crypto backend |

### Priority order for reduction

If the goal is reducing the dependency count with the least disruption:

1. **miniupnpc** (`DEP_003`) — one-line switch to the Windows-native UPnP fallback that already exists
2. **id3lib** (`DEP_002`) — remove two isolated code blocks; MP3 metadata display disappears gracefully
3. **ResizableLib** (`DEP_005`) — inline the library source into the tree; zero runtime change, submodule gone
4. **zlib** (`DEP_004`) — replace with miniz; API-compatible, no separate build needed
5. **Crypto++** (`DEP_001`) — only as part of a multi-month effort to migrate all crypto call sites
