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
#include "types.h"

/**
 * Returns the payload size encoded in an eD2k-style packet header while treating
 * zero-length packets as malformed instead of letting the subtraction underflow.
 */
inline uint32 GetPacketPayloadSize(uint32 packetlength)
{
	return packetlength > 0 ? packetlength - 1 : 0;
}

/**
 * Reports whether a TCP packet header can be materialized into the receive buffer
 * without overflowing the payload area after the opcode byte is removed.
 */
inline bool CanReadTcpPacketPayload(uint32 packetlength, size_t readBufferSize)
{
	return packetlength > 0 && static_cast<size_t>(packetlength - 1) <= readBufferSize;
}

/**
 * Rejects impossible or hostile tag counts before entering the per-tag parsing loop.
 */
inline bool HasSaneTagCount(ULONGLONG position, ULONGLONG length, uint32 tagcount, uint32 maxTagCount)
{
	return position <= length
		&& tagcount <= maxTagCount
		&& static_cast<ULONGLONG>(tagcount) <= (length - position);
}

/**
 * Reports whether a UDP payload contains the protocol byte and opcode byte.
 */
inline bool HasUdpPayloadHeader(UINT payloadLength)
{
	return payloadLength >= 2;
}

/**
 * Reports whether a compressed UDP payload contains the protocol and opcode bytes
 * plus at least one compressed byte for the inflater input buffer.
 */
inline bool HasCompressedUdpPayload(UINT payloadLength)
{
	return payloadLength > 2;
}

/**
 * Reports whether a blob payload can be read without underflowing the
 * remaining-byte calculation first.
 */
inline bool CanReadBlobPayload(ULONGLONG position, ULONGLONG length, uint32 blobSize)
{
	return position <= length
		&& static_cast<ULONGLONG>(blobSize) <= (length - position);
}

/**
 * Returns a bounded integer percentage and treats a zero denominator as 0%
 * instead of relying on ASSERT-only guards.
 */
inline uint32 CalculateProgressPercent(uint64 completed, uint64 total)
{
	if (total == 0)
		return 0;

	const uint64 boundedCompleted = completed <= total ? completed : total;
	return static_cast<uint32>((static_cast<double>(boundedCompleted) * 100.0) / static_cast<double>(total));
}

/**
 * Returns a bounded floating-point progress ratio while treating a zero or
 * negative denominator as an empty-progress state.
 */
inline float CalculateProgressRatio(float completed, float total)
{
	if (total <= 0.0f)
		return 0.0f;

	const float ratio = completed / total;
	if (ratio < 0.0f)
		return 0.0f;
	return ratio <= 1.0f ? ratio : 1.0f;
}

/**
 * Parses a dotted IPv4 literal without conflating the valid broadcast address
 * 255.255.255.255 with an invalid parse result.
 */
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
