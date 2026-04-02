#pragma once

#include "AtomicStateSeams.h"

namespace SharedFileListSeams
{
/**
 * Returns whether a file may enter the shared-file list when it is either a part file,
 * shared by directory/category rules, or shared by an explicit single-file share.
 */
inline bool CanAddSharedFile(const bool bIsPartFile, const bool bSharedByDirectory, const bool bSharedByExactPath)
{
	return bIsPartFile || bSharedByDirectory || bSharedByExactPath;
}

/**
 * @brief Marks the shared-file auto-rescan state as dirty for the next main-thread reload pass.
 */
inline void MarkAutoRescanDirtyFlag(std::atomic<LONG> &rDirtyFlag)
{
	SetAtomicLongFlag(rDirtyFlag);
}

/**
 * @brief Reports whether the shared-file auto-rescan state still has pending work.
 */
inline bool IsAutoRescanDirtyFlagSet(const std::atomic<LONG> &rDirtyFlag)
{
	return IsAtomicLongFlagSet(rDirtyFlag);
}

/**
 * @brief Clears the shared-file auto-rescan dirty marker after the reload pass drains it.
 */
inline void ClearAutoRescanDirtyFlag(std::atomic<LONG> &rDirtyFlag)
{
	ClearAtomicLongFlag(rDirtyFlag);
}
}
