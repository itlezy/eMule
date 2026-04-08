#pragma once

#include <cstdint>

namespace PartFilePersistenceSeams
{
constexpr uint64_t kMinPartMetWriteFreeBytes = 4ull * 1024ull * 1024ull;

inline bool CanWritePartMetWithFreeSpace(const uint64_t nFreeBytes, const uint64_t nRequiredBytes = kMinPartMetWriteFreeBytes)
{
	return nFreeBytes >= nRequiredBytes;
}

inline bool ShouldReusePartMetWriteCache(const bool bHasCachedResult, const bool bForceRefresh)
{
	return bHasCachedResult && !bForceRefresh;
}

inline bool ShouldFlushPartFileOnDestroy(const bool bIsClosing, const bool bHasWriteThread, const bool bIsWriteThreadRunning)
{
	return !bIsClosing || (bHasWriteThread && bIsWriteThreadRunning);
}
}

#define EMULE_TEST_HAVE_PART_FILE_PERSISTENCE_SEAMS 1
