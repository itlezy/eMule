#pragma once

#include <cstddef>
#include <cstring>

/**
 * @brief Copies an ANSI clipboard payload into the caller-owned buffer only when the full payload fits.
 *
 * The seam keeps clipboard export writes bounded by the actual global-buffer size instead of relying on
 * a raw C string copy into a fixed allocation.
 */
inline bool CopyClipboardAnsiPayload(char *pDestination, size_t nDestinationBytes, const char *pszSource, size_t nSourceBytes)
{
	if (pDestination == NULL || pszSource == NULL || nDestinationBytes < nSourceBytes + 1u)
		return false;

	memcpy(pDestination, pszSource, nSourceBytes);
	pDestination[nSourceBytes] = '\0';
	return true;
}
