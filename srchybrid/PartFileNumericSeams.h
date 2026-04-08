#pragma once

#include <cstddef>
#include <limits>

#include "ProtocolGuards.h"
#include "types.h"

namespace PartFileNumericSeams
{
constexpr uint16 kChunkCompletionPercentMax = 100u;

inline uint16 ClampUInt32ToUInt16(const uint32 nValue)
{
	return nValue > static_cast<uint32>((std::numeric_limits<uint16>::max)())
		? (std::numeric_limits<uint16>::max)()
		: static_cast<uint16>(nValue);
}

inline uint16 ClampUInt64ToUInt16(const uint64 nValue)
{
	return nValue > static_cast<uint64>((std::numeric_limits<uint16>::max)())
		? (std::numeric_limits<uint16>::max)()
		: static_cast<uint16>(nValue);
}

inline uint16 ClampCountToUInt16(const INT_PTR nValue)
{
	return nValue <= 0 ? 0u : ClampUInt64ToUInt16(static_cast<uint64>(nValue));
}

inline bool TryDeriveAICHHashSetSize(const size_t nHashSize, const uint32 nPartHashCount, uint32 *pnHashSetSize)
{
	if (pnHashSetSize == NULL)
		return false;

	size_t nHashCountWithMaster = 0;
	size_t nSerializedSize = 0;
	if (!TryAddSize(static_cast<size_t>(nPartHashCount), 1u, &nHashCountWithMaster)
		|| !TryMultiplyAddSize(nHashCountWithMaster, nHashSize, 2u, &nSerializedSize)
		|| nSerializedSize > static_cast<size_t>((std::numeric_limits<uint32>::max)()))
	{
		return false;
	}

	*pnHashSetSize = static_cast<uint32>(nSerializedSize);
	return true;
}

inline uint16 CalculateRareChunkSourceLimit(const size_t nSourceCount)
{
	size_t nRoundedSourceBucket = 0;
	if (!TryAddSize(nSourceCount, 9u, &nRoundedSourceBucket))
		nRoundedSourceBucket = (std::numeric_limits<size_t>::max)();
	else
		nRoundedSourceBucket /= 10u;

	if (nRoundedSourceBucket < 3u)
		nRoundedSourceBucket = 3u;

	return nRoundedSourceBucket > static_cast<size_t>((std::numeric_limits<uint16>::max)())
		? (std::numeric_limits<uint16>::max)()
		: static_cast<uint16>(nRoundedSourceBucket);
}

inline uint16 CalculateChunkCompletionPercent(const uint64 nCompletedBytes, const uint64 nFullPartSize)
{
	if (nFullPartSize == 0)
		return 0u;

	const uint64 nBoundedCompletedBytes = nCompletedBytes <= nFullPartSize ? nCompletedBytes : nFullPartSize;
	if (nBoundedCompletedBytes == 0)
		return 0u;
	if (nBoundedCompletedBytes == nFullPartSize)
		return kChunkCompletionPercentMax;

	const uint64 nRoundingBias = nFullPartSize - 1u;
	if (nBoundedCompletedBytes > (((std::numeric_limits<uint64>::max)() - nRoundingBias) / kChunkCompletionPercentMax))
		return kChunkCompletionPercentMax;

	return ClampUInt64ToUInt16(((nBoundedCompletedBytes * kChunkCompletionPercentMax) + nRoundingBias) / nFullPartSize);
}
}
