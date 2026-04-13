#pragma once

#include <cstdint>
#include <vector>
#include "LongPathSeams.h"

#define EMULE_SHARED_STARTUP_CACHE_POLICY_HAS_NTFS_FAST_PATH 1

/**
 * \brief Policy and wire-format helpers for the shared startup warm cache.
 *
 * The cache is intentionally disposable. Any structural or verification doubt
 * falls back to the normal startup rescan path.
 */
namespace SharedStartupCachePolicy
{
constexpr std::uint32_t kMagic = 0x43484853u; // 'SHHC'
constexpr std::uint16_t kVersion = 3;

/**
 * \brief Returns the config-directory sidecar name for the shared startup cache.
 */
inline const wchar_t* GetFileName() noexcept
{
	return L"sharedcache.dat";
}

/**
 * \brief Cached file inventory entry relative to one shared directory block.
 */
struct FileRecord
{
	CString strLeafName;
	std::int64_t utcFileDate = -1;
	std::uint64_t ullFileSize = 0;
};

/**
 * \brief Selects how one cached shared-directory block is validated at startup.
 */
enum class ValidationMode : std::uint8_t
{
	GenericFileVerification = 0,
	LocalNtfsJournalFastPath = 1
};

/**
 * \brief Per-volume guard data for local NTFS startup fast-path validation.
 */
struct VolumeRecord
{
	CString strVolumeKey;
	std::uint64_t ullVolumeSerialNumber = 0;
	std::uint64_t ullUsnJournalId = 0;
	std::int64_t llJournalCheckpointUsn = 0;
};

/**
 * \brief Cached snapshot for one explicit shared directory.
 */
struct DirectoryRecord
{
	CString strDirectoryPath;
	LongPathSeams::FileSystemObjectIdentity identity = {};
	bool bHasIdentity = false;
	std::int64_t utcDirectoryDate = -1;
	ValidationMode eValidationMode = ValidationMode::GenericFileVerification;
	VolumeRecord volumeRecord = {};
	std::uint64_t ullDirectoryFileReferenceNumber = 0;
	std::uint32_t uCachedFileCount = 0;
	std::vector<FileRecord> files;
};

/**
 * \brief Indicates whether one malformed directory block invalidates the whole file.
 */
inline bool ShouldRejectWholeCacheOnMalformedBlock() noexcept
{
	return true;
}

/**
 * \brief Indicates whether one cached-file lookup miss should trigger a rescan.
 */
inline bool ShouldRescanDirectoryOnCachedLookupMiss() noexcept
{
	return true;
}

/**
 * \brief Determines whether a directory snapshot is stable enough to persist.
 */
inline bool CanPersistDirectorySnapshot(const bool bHasPendingHashForDirectory) noexcept
{
	return !bHasPendingHashForDirectory;
}

/**
 * \brief Checks cached block self-consistency before filesystem validation.
 */
inline bool IsStructurallyValid(const DirectoryRecord &rRecord) noexcept
{
	if (rRecord.uCachedFileCount != static_cast<std::uint32_t>(rRecord.files.size()))
		return false;
	if (rRecord.eValidationMode != ValidationMode::GenericFileVerification
		&& rRecord.eValidationMode != ValidationMode::LocalNtfsJournalFastPath)
	{
		return false;
	}

	if (rRecord.eValidationMode == ValidationMode::LocalNtfsJournalFastPath) {
		return !rRecord.volumeRecord.strVolumeKey.IsEmpty()
			&& rRecord.volumeRecord.ullVolumeSerialNumber != 0
			&& rRecord.volumeRecord.ullUsnJournalId != 0
			&& rRecord.volumeRecord.llJournalCheckpointUsn > 0
			&& rRecord.ullDirectoryFileReferenceNumber != 0;
	}

	return true;
}

/**
 * \brief Checks whether a cached directory block still matches the current directory.
 */
inline bool MatchesVerifiedDirectoryState(const DirectoryRecord &rRecord, const bool bIdentityMatches, const bool bHaveCurrentDirectoryDate, const std::int64_t utcCurrentDirectoryDate) noexcept
{
	if (!IsStructurallyValid(rRecord))
		return false;
	if (rRecord.bHasIdentity && !bIdentityMatches)
		return false;
	if (!bHaveCurrentDirectoryDate)
		return false;
	return rRecord.utcDirectoryDate == utcCurrentDirectoryDate;
}

/**
 * \brief Returns true when one cached directory can use the NTFS journal fast path.
 */
inline bool UsesTrustedNtfsFastPath(const DirectoryRecord &rRecord) noexcept
{
	return rRecord.eValidationMode == ValidationMode::LocalNtfsJournalFastPath
		&& IsStructurallyValid(rRecord);
}

/**
 * \brief Checks whether the cached NTFS volume guard still matches the current volume.
 */
inline bool MatchesTrustedNtfsVolumeGuard(const VolumeRecord &rCachedRecord, const bool bHaveCurrentVolume, const CString &strCurrentVolumeKey, const std::uint64_t ullCurrentVolumeSerialNumber, const std::uint64_t ullCurrentUsnJournalId, const std::int64_t llCurrentLowestValidUsn, const std::int64_t llCurrentNextUsn) noexcept
{
	if (!bHaveCurrentVolume)
		return false;
	if (rCachedRecord.strVolumeKey.IsEmpty() || rCachedRecord.ullVolumeSerialNumber == 0 || rCachedRecord.ullUsnJournalId == 0 || rCachedRecord.llJournalCheckpointUsn <= 0)
		return false;
	if (rCachedRecord.strVolumeKey.CompareNoCase(strCurrentVolumeKey) != 0)
		return false;
	if (rCachedRecord.ullVolumeSerialNumber != ullCurrentVolumeSerialNumber || rCachedRecord.ullUsnJournalId != ullCurrentUsnJournalId)
		return false;
	if (llCurrentLowestValidUsn > 0 && rCachedRecord.llJournalCheckpointUsn < llCurrentLowestValidUsn)
		return false;
	if (llCurrentNextUsn > 0 && rCachedRecord.llJournalCheckpointUsn > llCurrentNextUsn)
		return false;
	return true;
}

/**
 * \brief Checks whether one cached file record still matches the current file entry.
 */
inline bool MatchesVerifiedFileState(const FileRecord &rRecord, const bool bHaveCurrentFileDate, const std::int64_t utcCurrentFileDate, const std::uint64_t ullCurrentFileSize) noexcept
{
	if (!bHaveCurrentFileDate)
		return false;
	return rRecord.utcFileDate == utcCurrentFileDate && rRecord.ullFileSize == ullCurrentFileSize;
}
}
