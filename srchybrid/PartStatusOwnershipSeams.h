#pragma once

#include <tchar.h>

#include <vector>

#include "ProtocolGuards.h"
#include "types.h"

namespace PartStatusOwnershipSeams
{
/**
 * @brief Returns a raw view for legacy call sites while preserving the historical NULL-for-empty contract.
 */
template <typename T>
inline T *GetRawStatusView(std::vector<T> &values)
{
	return values.empty() ? NULL : values.data();
}

/**
 * @brief Returns a read-only raw view for legacy call sites while preserving the historical NULL-for-empty contract.
 */
template <typename T>
inline const T *GetRawStatusView(const std::vector<T> &values)
{
	return values.empty() ? NULL : values.data();
}

/**
 * @brief Replaces a part-status buffer with a fully initialized fixed value.
 */
inline void AssignPartStatus(std::vector<uint8> &status, const uint16 nPartCount, const uint8 nValue)
{
	status.assign(nPartCount, nValue);
}

/**
 * @brief Clears a part-status buffer and keeps the legacy count in sync.
 */
inline void ClearPartStatus(std::vector<uint8> &status, uint16 &rnPartCount)
{
	status.clear();
	rnPartCount = 0;
}

/**
 * @brief Allocates the temporary pending-block overlay length while rejecting overflowed counts.
 */
inline bool TryBuildPendingPartOverlay(const size_t nPartCount, std::vector<char> &overlay)
{
	size_t nOverlaySize = 0;
	if (!TryAddSize(nPartCount, 0u, &nOverlaySize))
		return false;

	overlay.assign(nOverlaySize, 0);
	return true;
}

/**
 * @brief Builds the debug display buffer for a part-status bitfield and keeps the trailing terminator intact.
 */
inline bool TryBuildPartStatusDisplay(const std::vector<uint8> &status, std::vector<TCHAR> &display)
{
	size_t nDisplayLength = 0;
	if (!TryAddSize(status.size(), 1u, &nDisplayLength))
		return false;

	display.assign(nDisplayLength, _T('\0'));
	for (size_t i = 0; i < status.size(); ++i)
		display[i] = status[i] ? _T('#') : _T('.');
	return true;
}
}
