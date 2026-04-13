#pragma once

#include <cstdint>
#include <vector>
#include "LongPathSeams.h"

namespace SharedStartupCachePolicy
{
constexpr std::uint32_t kMagic = 0x43484853u; // 'SHHC'
constexpr std::uint16_t kVersion = 1;

inline const wchar_t* GetFileName() noexcept
{
	return L"sharedcache.dat";
}

struct FileRecord
{
	CString strLeafName;
	std::int64_t utcFileDate = -1;
	std::uint64_t ullFileSize = 0;
};

struct DirectoryRecord
{
	CString strDirectoryPath;
	LongPathSeams::FileSystemObjectIdentity identity = {};
	bool bHasIdentity = false;
	std::int64_t utcDirectoryDate = -1;
	std::uint32_t uCachedFileCount = 0;
	std::vector<FileRecord> files;
};

inline bool ShouldRejectWholeCacheOnMalformedBlock() noexcept
{
	return true;
}

inline bool ShouldRescanDirectoryOnCachedLookupMiss() noexcept
{
	return true;
}

inline bool CanPersistDirectorySnapshot(const bool bHasPendingHashForDirectory) noexcept
{
	return !bHasPendingHashForDirectory;
}

inline bool IsStructurallyValid(const DirectoryRecord &rRecord) noexcept
{
	return rRecord.uCachedFileCount == static_cast<std::uint32_t>(rRecord.files.size());
}

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
}
