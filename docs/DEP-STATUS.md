# Dependency Status

Reviewed on 2026-03-23. Staleness review 2026-03-31.

This note summarizes the current dependency set on `v0.72a`, the pinned version used by this workspace, the visible GitHub maintenance status, and a practical recommendation for this build workspace.

## Summary

| Dependency | Workspace pin | GitHub status | Maintained? | Recommendation |
|---|---:|---|---|---|
| Crypto++ | 8.9.0 | Latest release `8.9.0`; active commits continue | Yes | Keep |
| id3lib | ~~3.9.1~~ | **REMOVED** (commit `907e675`) | N/A | **[DONE]** Replaced by MediaInfo |
| miniupnp / miniupnpc | 2.3.3 | Latest `miniupnpc_2_3_3`; active commits continue | Yes | Keep |
| ResizableLib | `master` | Latest release `v1.5.3`; small amount of recent activity | Lightly maintained | Keep, low priority |
| zlib | 1.3.2 | Latest release `1.3.2`; current upstream activity | Yes | Keep |
| Mbed TLS | ~~4.0.0~~ | **REMOVED** (commit `6a1c440`) | N/A | **[DONE]** Removed with web server + SMTP |
| TF-PSA-Crypto | ~~1.0.0~~ | **REMOVED** (commit `6a1c440`) | N/A | **[DONE]** Removed with Mbed TLS |

## Per Dependency

### Crypto++

- Workspace pin: `CRYPTOPP_8_9_0`
- GitHub:
  - Latest release is `Crypto++ 8.9 release`
  - Release page states it was released on October 1, 2023
  - Commit activity on `master` continues into 2026
- Assessment:
  - This is an active upstream with a stable Windows/VS story
  - Release cadence is slower than commit activity, but it is clearly maintained
- Recommendation:
  - Keep the dependency model as-is
  - Revisit only when you want to pick up upstream fixes beyond 8.9.0

Sources:
- https://github.com/weidai11/cryptopp/releases
- https://github.com/weidai11/cryptopp/commits/master

### id3lib [DONE — REMOVED]

- **Status:** Fully removed in commit `907e675` ("WIP: remove id3lib and unify MP3 metadata on MediaInfo")
- Previous workspace pin: `v3.9.1`
- MP3 metadata extraction now handled entirely by MediaInfo
- Binary dependency removed from `emule.vcxproj` (no `ID3LIB_LINKOPTION` or lib paths)
- UI option `IDS_META_DATA_ID3LIB` retained as legacy selector label (maps to MediaInfo internally)

### miniupnp / miniupnpc

- Workspace pin: `miniupnpc_2_3_3`
- GitHub:
  - Latest `miniupnpc_2_3_3` release on May 26, 2025
  - Commit activity on `master` continues in 2026
- Assessment:
  - Healthy upstream
  - Good candidate to stay on the normal patch-and-pin model
- Recommendation:
  - Keep
  - Upgrade when there is a concrete reason, not just for freshness

Sources:
- https://github.com/miniupnp/miniupnp/releases
- https://github.com/miniupnp/miniupnp/commits/master

### ResizableLib

- Workspace pin: `master`
- GitHub:
  - Latest release `v1.5.3`
  - Release page shows the latest release from June 30, 2020
  - There is still recent repository activity on `master`
- Assessment:
  - Not dead, but clearly niche and low-velocity
  - This is old MFC-era infrastructure, so low churn is expected
- Recommendation:
  - Keep
  - Do not spend effort here unless it becomes a build blocker or you want to reduce MFC-era baggage

Sources:
- https://github.com/ppescher/resizablelib/releases
- https://github.com/ppescher/resizablelib/commits/master

### zlib

- Workspace pin: `1.3.2`
- GitHub:
  - Latest release `1.3.2`
  - Release date shown as February 17, 2026
  - Commit activity matches current upstream maintenance
- Assessment:
  - Very healthy upstream
  - Minimal strategic risk
- Recommendation:
  - Keep
  - No special handling needed beyond the existing workspace wrapper/configure logic

Sources:
- https://github.com/madler/zlib/releases
- https://github.com/madler/zlib/commits/master

### Mbed TLS [DONE — REMOVED]

- **Status:** Fully removed in commit `6a1c440` as part of the SMTP + embedded web-server purge.
- Previous workspace pin: `4.0.0`
- Core P2P behavior unchanged — Mbed TLS was only used by the optional SMTP notifier and the embedded web server.

### TF-PSA-Crypto [DONE — REMOVED]

- **Status:** Fully removed in commit `6a1c440` together with Mbed TLS.
- Previous workspace pin: `1.0.0`

## Overall Recommendation

- Keep as normal maintained deps:
  - Crypto++
  - miniupnp
  - zlib
- Keep but low priority:
  - ResizableLib
- **Removed:**
  - id3lib (replaced by MediaInfo, commit `907e675`)
  - Mbed TLS (removed with web server + SMTP, commit `6a1c440`)
  - TF-PSA-Crypto (removed with Mbed TLS, commit `6a1c440`)
