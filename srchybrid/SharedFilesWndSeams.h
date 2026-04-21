#pragma once

namespace SharedFilesWndSeams
{
inline constexpr int kMinTreeWidth = 100;
inline constexpr int kMinRightPaneWidth = 160;
inline constexpr int kSplitterWidth = 4;

/**
 * @brief Captures the reload work deferred while shared-file hashing is active.
 */
struct ReloadDeferralState
{
	bool bFullTreeReload = false;
	bool bSharedFilesReload = false;
};

/**
 * @brief Returns the maximum splitter-left position for the Shared Files pane.
 */
inline int GetSplitterRangeMax(const int iClientWidth)
{
	const int iComputedMax = iClientWidth - kMinRightPaneWidth - kSplitterWidth;
	return iComputedMax < kMinTreeWidth ? kMinTreeWidth : iComputedMax;
}

/**
 * @brief Clamps a persisted or requested splitter-left position to the current client width.
 */
inline int ClampSplitterPosition(const int iSplitterLeft, const int iClientWidth)
{
	const int iRangeMax = GetSplitterRangeMax(iClientWidth);
	if (iSplitterLeft < kMinTreeWidth)
		return kMinTreeWidth;
	if (iSplitterLeft > iRangeMax)
		return iRangeMax;
	return iSplitterLeft;
}

/**
 * @brief Reports whether a reload request should be deferred to avoid active shared hashing.
 */
inline bool ShouldDeferReloadForSharedHashing(const bool bSharedHashingActive)
{
	return bSharedHashingActive;
}

/**
 * @brief Reports whether a coalesced Shared Files list reload can run now.
 */
inline bool ShouldRunStartupDeferredListReload(const bool bDeferredReloadPending, const bool bSharedHashingActive)
{
	return bDeferredReloadPending && !bSharedHashingActive;
}

/**
 * @brief Adds one deferred reload request, with a full tree reload superseding shared-file-only work.
 */
inline ReloadDeferralState AddDeferredReloadRequest(const ReloadDeferralState &rState, const bool bForceTreeReload)
{
	ReloadDeferralState result = rState;
	if (bForceTreeReload) {
		result.bFullTreeReload = true;
		result.bSharedFilesReload = false;
	} else if (!result.bFullTreeReload)
		result.bSharedFilesReload = true;
	return result;
}

/**
 * @brief Reports whether any deferred shared-files reload work remains.
 */
inline bool HasDeferredReload(const ReloadDeferralState &rState)
{
	return rState.bFullTreeReload || rState.bSharedFilesReload;
}
}
