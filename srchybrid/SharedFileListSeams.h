#pragma once

#include "AtomicStateSeams.h"

namespace SharedFileListSeams
{
struct AutoReloadScheduleState
{
	bool bAutoRescanEnabled;
	bool bAutoReloadPending;
	bool bAutoReloadInProgress;
	bool bReloadTickElapsed;
	bool bFallbackPolling;
	bool bDirty;
};

constexpr DWORD kImportPartProgressYieldMs = 100;

inline bool ShouldScheduleAutoReload(const AutoReloadScheduleState &rState)
{
	if (!rState.bAutoRescanEnabled
		|| rState.bAutoReloadPending
		|| rState.bAutoReloadInProgress
		|| !rState.bReloadTickElapsed)
	{
		return false;
	}

	return rState.bFallbackPolling || rState.bDirty;
}

inline bool ShouldYieldAfterImportProgress(const bool bAppRunning, const bool bImportedFullPart, const bool bImportStillActive)
{
	return bAppRunning && bImportedFullPart && bImportStillActive;
}

inline bool CanAddSharedFile(const bool bIsPartFile, const bool bSharedByDirectory, const bool bSharedByExactPath)
{
	return bIsPartFile || bSharedByDirectory || bSharedByExactPath;
}

inline void MarkAutoRescanDirtyFlag(std::atomic<LONG> &rDirtyFlag)
{
	SetAtomicLongFlag(rDirtyFlag);
}

inline bool IsAutoRescanDirtyFlagSet(const std::atomic<LONG> &rDirtyFlag)
{
	return IsAtomicLongFlagSet(rDirtyFlag);
}

inline void ClearAutoRescanDirtyFlag(std::atomic<LONG> &rDirtyFlag)
{
	ClearAtomicLongFlag(rDirtyFlag);
}
}
