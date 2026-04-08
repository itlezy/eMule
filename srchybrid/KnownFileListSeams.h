#pragma once

inline bool ShouldPurgeKnownAICHHashset(bool bHasKnownFileEntry, bool bShouldPartiallyPurgeFile) noexcept
{
	return !bHasKnownFileEntry || bShouldPartiallyPurgeFile;
}
