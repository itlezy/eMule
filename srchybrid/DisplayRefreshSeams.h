#pragma once

enum EDisplayRefreshMask : uint32
{
	DISPLAY_REFRESH_NONE = 0,
	DISPLAY_REFRESH_CLIENT_LIST = 1u << 0,
	DISPLAY_REFRESH_DOWNLOAD_LIST = 1u << 1,
	DISPLAY_REFRESH_DOWNLOAD_CLIENTS = 1u << 2,
	DISPLAY_REFRESH_UPLOAD_LIST = 1u << 3,
	DISPLAY_REFRESH_QUEUE_LIST = 1u << 4
};

struct CPartFileDisplayUpdateRequest
{
	unsigned char fileHash[16];
};

struct CClientDisplayUpdateRequest
{
	unsigned char userHash[16];
	DWORD connectIP;
	USHORT userPort;
	USHORT reserved;
};

/**
 * @brief Reports whether the caller must marshal a display refresh back to the main thread.
 */
inline bool ShouldQueueDisplayRefresh(UINT uCurrentThreadId, UINT uMainThreadId)
{
	return uMainThreadId != 0 && uCurrentThreadId != uMainThreadId;
}

/**
 * @brief Applies the existing randomized throttling window used by UI refresh helpers.
 */
inline bool ShouldRunDisplayRefresh(bool bForce, DWORD dwCurrentTick, DWORD dwLastRefreshTick, DWORD dwMinimumWait, DWORD dwRandomWait = 0)
{
	return bForce || dwCurrentTick >= dwLastRefreshTick + dwMinimumWait + dwRandomWait;
}

/**
 * @brief Atomically merges a refresh mask and returns the previous value.
 */
inline LONG AccumulatePendingDisplayMask(volatile LONG *pnPendingMask, LONG nMask)
{
	LONG nCurrent = *pnPendingMask;
	for (;;) {
		const LONG nUpdated = nCurrent | nMask;
		const LONG nObserved = InterlockedCompareExchange(pnPendingMask, nUpdated, nCurrent);
		if (nObserved == nCurrent)
			return nCurrent;
		nCurrent = nObserved;
	}
}
