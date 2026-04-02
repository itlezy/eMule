#pragma once

#include <limits>
#include "types.h"

struct CollectionSignatureLayout
{
	UINT nMessageLength;
	UINT nSignatureLength;
};

/**
 * @brief Builds the signed-message and trailing-signature spans from a serialized collection payload.
 */
inline bool TryBuildCollectionSignatureLayout(const ULONGLONG nSerializedLength, const ULONGLONG nSignedLength, CollectionSignatureLayout &layout)
{
	if (nSerializedLength <= nSignedLength)
		return false;
	if (nSerializedLength > static_cast<ULONGLONG>((std::numeric_limits<UINT>::max)()))
		return false;
	if (nSignedLength > static_cast<ULONGLONG>((std::numeric_limits<UINT>::max)()))
		return false;

	layout.nMessageLength = static_cast<UINT>(nSignedLength);
	layout.nSignatureLength = static_cast<UINT>(nSerializedLength - nSignedLength);
	return layout.nSignatureLength > 0;
}

/**
 * @brief Converts a serialized collection length into the on-disk 32-bit format when representable.
 */
inline bool TryConvertCollectionSerializedLength(const ULONGLONG nSerializedLength, uint32 &dwSerializedLength)
{
	if (nSerializedLength > static_cast<ULONGLONG>((std::numeric_limits<uint32>::max)()))
		return false;

	dwSerializedLength = static_cast<uint32>(nSerializedLength);
	return true;
}

/**
 * @brief Keeps collection import tolerant of malformed individual entries while continuing the outer file load.
 */
inline bool ShouldContinueAfterCollectionEntryFailure()
{
	return true;
}
