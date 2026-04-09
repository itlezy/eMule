#pragma once

#include <cstdint>
#include <string>

#include "PartFilePersistenceSeams.h"

namespace DownloadQueueDiskSpaceSeams
{
enum class FileDiskSpaceStatus : uint8_t
{
	Active = 0,
	Paused,
	Insufficient,
	Error,
	Completing,
	Complete
};

struct VolumeKey
{
	int DriveNumber;
	std::wstring ShareName;
};

struct FileDiskSpaceState
{
	FileDiskSpaceStatus Status;
	VolumeKey TempVolumeKey;
	bool IsNormalFile;
	uint64_t NeededBytes;
};

struct VolumeResumeBudget
{
	VolumeKey TempVolumeKey;
	uint64_t FreeBytes;
	uint64_t ResumeHeadroomBytes;
};

inline bool IsSameVolumeKey(const VolumeKey &rLeft, const VolumeKey &rRight)
{
	return rLeft.DriveNumber == rRight.DriveNumber
		&& (rLeft.DriveNumber >= 0 || rLeft.ShareName == rRight.ShareName);
}

inline bool IsPauseCandidate(const FileDiskSpaceStatus eStatus)
{
	return eStatus == FileDiskSpaceStatus::Active;
}

inline bool ShouldPauseForDiskSpace(const FileDiskSpaceState &rState, const uint64_t nFreeBytes, const uint64_t nMinimumFreeBytes)
{
	if (!IsPauseCandidate(rState.Status))
		return false;

	if (nMinimumFreeBytes == 0)
		return rState.NeededBytes > nFreeBytes;

	if (nFreeBytes >= nMinimumFreeBytes)
		return false;

	return !rState.IsNormalFile || rState.NeededBytes > 0;
}

inline bool IsResumeCandidate(const FileDiskSpaceStatus eStatus)
{
	return eStatus == FileDiskSpaceStatus::Insufficient;
}

inline void AccumulateResumeHeadroom(VolumeResumeBudget *pBudget, const FileDiskSpaceState &rState)
{
	if (pBudget == NULL || !IsResumeCandidate(rState.Status))
		return;

	pBudget->ResumeHeadroomBytes = PartFilePersistenceSeams::AddInsufficientResumeHeadroomBytes(
		pBudget->ResumeHeadroomBytes, rState.NeededBytes);
}

inline bool ShouldResumeForDiskSpace(const FileDiskSpaceState &rState, const VolumeResumeBudget &rBudget, const uint64_t nMinimumFreeBytes)
{
	return IsResumeCandidate(rState.Status)
		&& IsSameVolumeKey(rState.TempVolumeKey, rBudget.TempVolumeKey)
		&& nMinimumFreeBytes == 0
		&& rState.NeededBytes <= rBudget.FreeBytes;
}
}
