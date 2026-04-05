#pragma once

#include <cstring>
#include <limits>
#include <memory>
#include <vector>

/**
 * @brief Derives an initial zlib work-buffer size and caps it to the caller's maximum.
 */
inline bool TryDeriveZlibBufferSize(size_t nSourceSize, size_t nMultiplier, size_t nBias, size_t nMaximumSize, size_t *pnBufferSize)
{
	if (pnBufferSize == nullptr || nMaximumSize == 0)
		return false;

	size_t nDerivedSize = 0;
	if (nMultiplier != 0 && nSourceSize > ((std::numeric_limits<size_t>::max)() - nBias) / nMultiplier)
		nDerivedSize = nMaximumSize;
	else
		nDerivedSize = nSourceSize * nMultiplier + nBias;

	if (nDerivedSize == 0 || nDerivedSize > nMaximumSize)
		nDerivedSize = nMaximumSize;

	*pnBufferSize = nDerivedSize;
	return true;
}

/**
 * @brief Calculates the next bounded zlib retry-buffer size for `Z_BUF_ERROR` growth loops.
 */
inline bool TryGrowZlibBufferSize(size_t nCurrentSize, size_t nMaximumSize, size_t *pnNextSize)
{
	if (pnNextSize == nullptr || nCurrentSize == 0 || nCurrentSize >= nMaximumSize)
		return false;

	*pnNextSize = (nCurrentSize > nMaximumSize / 2u) ? nMaximumSize : nCurrentSize * 2u;
	return *pnNextSize > nCurrentSize;
}

/**
 * @brief Copies vector-backed temporary data into a legacy `new[]` buffer for ownership handoff.
 */
inline std::unique_ptr<unsigned char[]> MakeOwnedByteBufferCopy(const std::vector<unsigned char> &rSource, size_t nCopiedSize)
{
	if (nCopiedSize == 0 || nCopiedSize > rSource.size())
		return std::unique_ptr<unsigned char[]>();

	std::unique_ptr<unsigned char[]> pCopy(new unsigned char[nCopiedSize]);
	std::memcpy(pCopy.get(), rSource.data(), nCopiedSize);
	return pCopy;
}
