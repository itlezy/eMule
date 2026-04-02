#pragma once

#include <cstddef>
#include <tchar.h>

#include "ProtocolGuards.h"

/**
 * @brief Duplicates a TCHAR string through the supplied allocator while reporting allocation failure explicitly.
 *
 * The seam keeps hello-packet parsing from silently accepting a null duplicated username when the allocator
 * fails under memory pressure.
 */
template <typename TDuplicateFn>
inline bool TryDuplicateCString(const TCHAR *pszSource, TCHAR **ppszDuplicate, TDuplicateFn pfnDuplicate)
{
	if (pszSource == NULL || ppszDuplicate == NULL)
		return false;

	*ppszDuplicate = pfnDuplicate(pszSource);
	return *ppszDuplicate != NULL;
}

/**
 * @brief Reports how many packed bytes are needed to represent the given part-status bitfield.
 */
inline bool TryGetPartStatusPackedByteCount(size_t nPartCount, size_t *pnPackedByteCount)
{
	if (pnPackedByteCount == NULL)
		return false;

	return TryAddSize(nPartCount / 8u, (nPartCount % 8u) != 0 ? 1u : 0u, pnPackedByteCount);
}

/**
 * @brief Reports whether the remaining serialized bytes are sufficient for the packed part-status bitfield.
 */
inline bool HasPackedPartStatusBytes(size_t nPartCount, size_t nRemainingBytes)
{
	size_t nPackedByteCount = 0;
	return TryGetPartStatusPackedByteCount(nPartCount, &nPackedByteCount)
		&& nPackedByteCount <= nRemainingBytes;
}

/**
 * @brief Reports how many TCHAR slots are needed for the debug part-status string including the terminator.
 */
inline bool TryGetPartStatusDisplayLength(size_t nPartCount, size_t *pnDisplayLength)
{
	return TryAddSize(nPartCount, 1u, pnDisplayLength);
}

/**
 * @brief Decodes a packed part-status bitfield into the destination status array without overrunning either span.
 */
inline bool TryDecodePartStatusBits(uint8 *pPartStatus, size_t nPartCount, const BYTE *pPackedBytes, size_t nPackedByteCount)
{
	size_t nRequiredPackedBytes = 0;
	if (pPartStatus == NULL || pPackedBytes == NULL
		|| !TryGetPartStatusPackedByteCount(nPartCount, &nRequiredPackedBytes)
		|| nRequiredPackedBytes > nPackedByteCount)
	{
		return false;
	}

	size_t nDoneParts = 0;
	for (size_t nByteIndex = 0; nByteIndex < nRequiredPackedBytes; ++nByteIndex) {
		const BYTE nPackedByte = pPackedBytes[nByteIndex];
		for (size_t nBitIndex = 0; nBitIndex < 8u && nDoneParts < nPartCount; ++nBitIndex)
			pPartStatus[nDoneParts++] = static_cast<uint8>((nPackedByte >> nBitIndex) & 0x01u);
	}

	return nDoneParts == nPartCount;
}
