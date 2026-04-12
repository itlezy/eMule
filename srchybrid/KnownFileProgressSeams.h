#pragma once

#include <cstdint>

struct CKnownFileProgressTargetSnapshot
{
	unsigned char fileHash[16];
	uint64_t fileSize;
	bool isPartFile;
};

/**
 * @brief Reports whether a worker-side progress owner still matches the file currently being hashed.
 */
inline bool IsCompatibleKnownFileProgressOwner(bool bIsKnownFileOwner, uint64_t nProgressOwnerFileSize, uint64_t nCurrentFileSize)
{
	return bIsKnownFileOwner && nProgressOwnerFileSize == nCurrentFileSize;
}
