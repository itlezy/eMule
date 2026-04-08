//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#pragma once

#include <cstddef>
#include <limits>
#include "types.h"

inline uint32 GetPacketPayloadSize(uint32 packetlength)
{
	return packetlength > 0 ? packetlength - 1 : 0;
}

inline bool CanReadTcpPacketPayload(uint32 packetlength, size_t readBufferSize)
{
	return packetlength > 0 && static_cast<size_t>(packetlength - 1) <= readBufferSize;
}

inline bool CanReadPacketSpan(size_t packetSize, size_t offset, size_t spanSize)
{
	return offset <= packetSize && spanSize <= packetSize - offset;
}

inline bool CanRestoreTcpPendingHeader(size_t pendingHeaderSize, size_t packetHeaderSize, size_t readBufferSize)
{
	return pendingHeaderSize <= packetHeaderSize && pendingHeaderSize <= readBufferSize;
}

inline bool CanStoreTcpPendingHeader(size_t trailingByteCount, size_t packetHeaderSize)
{
	return trailingByteCount < packetHeaderSize;
}

inline bool CanContinuePacketAssembly(uint32 packetSize, uint32 bufferedByteCount)
{
	return bufferedByteCount <= packetSize;
}

inline bool TryAddSize(size_t baseSize, size_t extraSize, size_t *pnCombinedSize)
{
	if (pnCombinedSize == NULL || baseSize > (std::numeric_limits<size_t>::max)() - extraSize)
		return false;

	*pnCombinedSize = baseSize + extraSize;
	return true;
}

inline bool TryMultiplyAddSize(size_t baseSize, size_t multiplier, size_t addend, size_t *pnExpandedSize)
{
	if (pnExpandedSize == NULL)
		return false;

	if (multiplier != 0 && baseSize > ((std::numeric_limits<size_t>::max)() - addend) / multiplier)
		return false;

	*pnExpandedSize = baseSize * multiplier + addend;
	return true;
}

inline bool HasSaneTagCount(ULONGLONG position, ULONGLONG length, uint32 tagcount, uint32 maxTagCount)
{
	return position <= length
		&& tagcount <= maxTagCount
		&& static_cast<ULONGLONG>(tagcount) <= (length - position);
}

inline bool HasUdpPayloadHeader(UINT payloadLength)
{
	return payloadLength >= 2;
}

inline bool HasCompressedUdpPayload(UINT payloadLength)
{
	return payloadLength > 2;
}

inline bool HasUdpCallbackPayload(size_t payloadLength)
{
	return payloadLength >= 17 && CanReadPacketSpan(payloadLength, 10u, 6u);
}

inline uint32 GetDownloadBlockPacketHeaderSize(const bool bPacked, const bool bI64Offsets)
{
	const uint32 nFileHashSize = 16;
	const uint32 nOffsetFieldSize = bI64Offsets ? 8u : 4u;
	return nFileHashSize + nOffsetFieldSize + (bPacked ? 4u : nOffsetFieldSize);
}

inline bool HasDownloadBlockPacketHeader(size_t packetSize, const bool bPacked, const bool bI64Offsets)
{
	return packetSize >= GetDownloadBlockPacketHeaderSize(bPacked, bI64Offsets);
}

inline bool CanReadBlobPayload(ULONGLONG position, ULONGLONG length, uint32 blobSize)
{
	return position <= length
		&& static_cast<ULONGLONG>(blobSize) <= (length - position);
}

inline uint32 CalculateProgressPercent(uint64 completed, uint64 total)
{
	if (total == 0)
		return 0;

	const uint64 boundedCompleted = completed <= total ? completed : total;
	return static_cast<uint32>((static_cast<double>(boundedCompleted) * 100.0) / static_cast<double>(total));
}

inline float CalculateProgressRatio(float completed, float total)
{
	if (total <= 0.0f)
		return 0.0f;

	const float ratio = completed / total;
	if (ratio < 0.0f)
		return 0.0f;
	return ratio <= 1.0f ? ratio : 1.0f;
}

inline bool TryParseDottedIPv4Literal(const char *pszAddress, uint32 *pnAddress)
{
	if (pszAddress == NULL || pnAddress == NULL)
		return false;

	unsigned int octets[4] = {};
	const char *pszCursor = pszAddress;
	const size_t nOctetCount = sizeof(octets) / sizeof(octets[0]);
	for (size_t index = 0; index < nOctetCount; ++index) {
		if (*pszCursor < '0' || *pszCursor > '9')
			return false;

		unsigned int nOctet = 0;
		do {
			nOctet = nOctet * 10 + static_cast<unsigned int>(*pszCursor - '0');
			if (nOctet > 255)
				return false;
			++pszCursor;
		} while (*pszCursor >= '0' && *pszCursor <= '9');

		octets[index] = nOctet;
		if (index + 1 == nOctetCount)
			break;

		if (*pszCursor != '.')
			return false;
		++pszCursor;
	}

	if (*pszCursor != '\0')
		return false;

	uint8 *pBytes = reinterpret_cast<uint8*>(pnAddress);
	pBytes[0] = static_cast<uint8>(octets[0]);
	pBytes[1] = static_cast<uint8>(octets[1]);
	pBytes[2] = static_cast<uint8>(octets[2]);
	pBytes[3] = static_cast<uint8>(octets[3]);
	return true;
}
