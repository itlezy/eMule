#pragma once

#include <windows.h>

struct AICHSyncForegroundHashState
{
	bool bIsClosing;
	INT_PTR nSharedFileHashingCount;
	bool bHasPendingPartFileHashing;
};

/**
 * @brief Reports whether AICH background hashing should wait for foreground hash work to finish.
 */
inline bool ShouldWaitForAICHSyncForegroundHashing(const AICHSyncForegroundHashState &state)
{
	return !state.bIsClosing && (state.nSharedFileHashingCount > 0 || state.bHasPendingPartFileHashing);
}

/**
 * @brief Reports whether the current AICH sync candidate should still be hashed.
 */
inline bool ShouldCreateAICHSyncHash(bool bIsClosing, bool bIsStillShared)
{
	return !bIsClosing && bIsStillShared;
}

/**
 * @brief Validates the queued AICH sync progress count before it is forwarded to the UI thread.
 */
inline bool HasValidAICHSyncProgressCount(INT_PTR nRemainingHashes)
{
	return nRemainingHashes >= 0;
}
