#pragma once

#include "AICHMaintenanceSeams.h"

/**
 * @brief Reports whether AICH background hashing should wait for foreground hash work to finish.
 */
inline bool ShouldWaitForAICHSyncForegroundHashing(const AICHSyncForegroundHashState &state)
{
	const AICHSyncForegroundWaitAction action = AICHMaintenanceSeams::GetForegroundHashWaitAction(state);
	return !action.bShouldExit && action.dwSleepMilliseconds != 0u;
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
