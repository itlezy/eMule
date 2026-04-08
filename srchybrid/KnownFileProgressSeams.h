#pragma once

#include <cstdint>

inline bool IsCompatibleKnownFileProgressOwner(bool bIsKnownFileOwner, uint64_t nProgressOwnerFileSize, uint64_t nCurrentFileSize)
{
	return bIsKnownFileOwner && nProgressOwnerFileSize == nCurrentFileSize;
}
