#pragma once

/**
 * @brief Determines whether a stored AICH hashset should be purged from known2.met.
 *
 * A hashset should be purged when no current known-file entry owns that master hash any more,
 * or when the owning known file is old enough to be partially purged.
 */
inline bool ShouldPurgeKnownAICHHashset(bool bHasKnownFileEntry, bool bShouldPartiallyPurgeFile) noexcept
{
	return !bHasKnownFileEntry || bShouldPartiallyPurgeFile;
}
