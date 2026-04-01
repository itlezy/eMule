#pragma once

namespace SharedFileListSeams
{
/**
 * Returns whether a file may enter the shared-file list when it is either
 * shared by directory/category rules or by an explicit single-file share.
 */
inline bool CanAddSharedFile(const bool bSharedByDirectory, const bool bSharedByExactPath)
{
	return bSharedByDirectory || bSharedByExactPath;
}
}
