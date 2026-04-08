#pragma once

#include <tchar.h>

#include <vector>

#include "ProtocolGuards.h"
#include "types.h"

namespace PartStatusOwnershipSeams
{
template <typename T>
inline T *GetRawStatusView(std::vector<T> &values)
{
	return values.empty() ? NULL : values.data();
}

template <typename T>
inline const T *GetRawStatusView(const std::vector<T> &values)
{
	return values.empty() ? NULL : values.data();
}

inline void AssignPartStatus(std::vector<uint8> &status, const uint16 nPartCount, const uint8 nValue)
{
	status.assign(nPartCount, nValue);
}

inline void ClearPartStatus(std::vector<uint8> &status, uint16 &rnPartCount)
{
	status.clear();
	rnPartCount = 0;
}

inline bool TryBuildPendingPartOverlay(const size_t nPartCount, std::vector<char> &overlay)
{
	size_t nOverlaySize = 0;
	if (!TryAddSize(nPartCount, 0u, &nOverlaySize))
		return false;

	overlay.assign(nOverlaySize, 0);
	return true;
}

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
