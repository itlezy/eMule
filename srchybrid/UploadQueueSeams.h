#pragma once

enum UploadQueueEntryAccessState
{
	uploadQueueEntryMissing,
	uploadQueueEntryRetired,
	uploadQueueEntryLive
};

/**
 * @brief Classifies an upload entry snapshot while the upload-list read lock is held.
 */
inline UploadQueueEntryAccessState ClassifyUploadQueueEntryAccess(bool bFoundInActiveList, bool bRetired, bool bHasClient)
{
	if (!bFoundInActiveList)
		return uploadQueueEntryMissing;
	if (bRetired || !bHasClient)
		return uploadQueueEntryRetired;
	return uploadQueueEntryLive;
}

/**
 * @brief Reports whether a retired upload entry can be reclaimed safely.
 */
inline bool CanReclaimUploadQueueEntry(bool bRetired, int nPendingIOBlocks)
{
	return bRetired && nPendingIOBlocks == 0;
}
