#pragma once

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
}
