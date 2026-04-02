#pragma once

#include <Windows.h>

#include <cstddef>
#include <limits>

#include "ProtocolGuards.h"
#include "types.h"

/**
 * @brief Captures the current foreground hashing state that can block background AICH sync work.
 */
struct AICHSyncForegroundHashState
{
	bool bIsClosing;
	INT_PTR nSharedFileHashingCount;
	bool bHasPendingPartFileHashing;
};

/**
 * @brief Describes whether the background AICH sync loop should exit, continue, or yield briefly.
 */
struct AICHSyncForegroundWaitAction
{
	bool bShouldExit;
	DWORD dwSleepMilliseconds;
};

namespace AICHMaintenanceSeams
{
/**
 * @brief Bounded fallback delay used when the AICH sync thread yields to foreground hashing.
 */
constexpr DWORD kForegroundHashYieldDelayMs = 1u;

/**
 * @brief Derives the raw byte span for a sequence of serialized AICH hashes while rejecting overflow.
 */
inline bool TryDeriveAICHHashPayloadSize(const size_t nHashSize, const uint32 nHashCount, uint32 *pnPayloadSize)
{
	if (pnPayloadSize == NULL)
		return false;

	size_t nPayloadSize = 0;
	if (!TryMultiplyAddSize(nHashSize, static_cast<size_t>(nHashCount), 0u, &nPayloadSize)
		|| nPayloadSize > static_cast<size_t>((std::numeric_limits<uint32>::max)()))
	{
		return false;
	}

	*pnPayloadSize = static_cast<uint32>(nPayloadSize);
	return true;
}

/**
 * @brief Computes the bounded cooperative-wait action for background AICH sync work.
 */
inline AICHSyncForegroundWaitAction GetForegroundHashWaitAction(const AICHSyncForegroundHashState &state)
{
	if (state.bIsClosing)
		return {true, 0u};

	if (state.nSharedFileHashingCount > 0 || state.bHasPendingPartFileHashing)
		return {false, kForegroundHashYieldDelayMs};

	return {false, 0u};
}
}
