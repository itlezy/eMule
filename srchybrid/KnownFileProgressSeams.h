#pragma once

#include <cstdint>

/**
 * @brief Reports whether a worker-side progress owner still matches the file currently being hashed.
 */
inline bool IsCompatibleKnownFileProgressOwner(bool bIsKnownFileOwner, uint64_t nProgressOwnerFileSize, uint64_t nCurrentFileSize)
{
	return bIsKnownFileOwner && nProgressOwnerFileSize == nCurrentFileSize;
}
