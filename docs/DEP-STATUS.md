# Dependency Status

Reviewed on 2026-03-23.

This note summarizes the current dependency set on `v0.72a`, the pinned version used by this workspace, the visible GitHub maintenance status, and a practical recommendation for this build workspace.

## Summary

| Dependency | Workspace pin | GitHub status | Maintained? | Recommendation |
|---|---:|---|---|---|
| Crypto++ | 8.9.0 | Latest release `8.9.0`; active commits continue | Yes | Keep |
| id3lib | ~~3.9.1~~ | **REMOVED** (commit `907e675`) | N/A | **[DONE]** Replaced by MediaInfo |
| miniupnp / miniupnpc | 2.3.3 | Latest `miniupnpc_2_3_3`; active commits continue | Yes | Keep |
| ResizableLib | `master` | Latest release `v1.5.3`; small amount of recent activity | Lightly maintained | Keep, low priority |
| zlib | 1.3.2 | Latest release `1.3.2`; current upstream activity | Yes | Keep |
| Mbed TLS | 4.0.0 | Latest release `4.0.0`; active development | Yes | Keep, watch API churn |
| TF-PSA-Crypto | 1.0.0 | Latest release `1.0.0`; active development | Yes | Keep, coupled to Mbed TLS |

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

### Mbed TLS

- Workspace pin: `4.0.0`
- GitHub:
  - Latest release `4.0.0`
  - Release page shows October 15, 2025
  - `development` branch still has active commits in 2026
- Assessment:
  - Actively maintained
  - Main risk is API churn and the 4.x restructuring, not abandonment
  - This dependency is more operationally complex than most others in the workspace
- Recommendation:
  - Keep
  - Watch upstream migration notes and security fixes carefully
  - Prefer deliberate upgrades, not casual ones

Sources:
- https://github.com/Mbed-TLS/mbedtls/releases
- https://github.com/Mbed-TLS/mbedtls/commits/development

### TF-PSA-Crypto

- Workspace pin: `1.0.0`
- GitHub:
  - Latest release `1.0.0`
  - Release page shows October 15, 2025
  - `development` branch still has active commits in 2026
  - Release notes explicitly say Mbed TLS 4.0.0 and TF-PSA-Crypto 1.0.0 are coupled
- Assessment:
  - Actively maintained
  - Operationally tied to Mbed TLS rather than independently chosen
- Recommendation:
  - Keep
  - Evaluate together with Mbed TLS, never in isolation

Sources:
- https://github.com/Mbed-TLS/TF-PSA-Crypto/releases
- https://github.com/Mbed-TLS/TF-PSA-Crypto/commits/development

## Overall Recommendation

- Keep as normal maintained deps:
  - Crypto++
  - miniupnp
  - zlib
  - Mbed TLS
  - TF-PSA-Crypto
- Keep but low priority:
  - ResizableLib
- **Removed:**
  - id3lib (replaced by MediaInfo, commit `907e675`)
