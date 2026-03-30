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
#include "opcodes.h"

enum
{
	PROTOCOL_PACKET_HEADER_SIZE = 6
};

/**
 * Carries the decoded metadata from a serialized eD2k packet header.
 */
struct ProtocolPacketHeader
{
	uint8 nProtocol;
	uint8 nOpcode;
	uint32 nPacketLength;
	uint32 nPayloadLength;
};

/**
 * Carries the decoded metadata from a serialized eD2k tag header.
 */
struct ProtocolTagHeader
{
	uint8 nRawType;
	uint8 nType;
	uint8 nNameId;
	uint16 nNameLength;
	size_t nHeaderSize;
	bool bUsesNameId;
};

/**
 * Carries the serialized span information for a single eD2k tag.
 */
struct ProtocolTagSpan
{
	ProtocolTagHeader Header;
	size_t nValueSize;
	size_t nTotalSize;
	uint32 nBlobSize;
};

/**
 * Reads a little-endian 16-bit value from serialized packet bytes.
 */
inline uint16 ReadLittleEndianUInt16(const BYTE *pBuffer)
{
	return static_cast<uint16>(pBuffer[0] | (static_cast<uint16>(pBuffer[1]) << 8));
}

/**
 * Reads a little-endian 32-bit value from serialized packet bytes.
 */
inline uint32 ReadLittleEndianUInt32(const BYTE *pBuffer)
{
	return static_cast<uint32>(pBuffer[0])
		| (static_cast<uint32>(pBuffer[1]) << 8)
		| (static_cast<uint32>(pBuffer[2]) << 16)
		| (static_cast<uint32>(pBuffer[3]) << 24);
}

/**
 * Reproduces the legacy remaining-byte logic without guarding underflow first.
 */
inline bool CanReadSerializedBytes(ULONGLONG position, ULONGLONG length, ULONGLONG byteCount)
{
	return byteCount <= (length - position);
}

/**
 * Reproduces the legacy bool-array payload logic without guarding underflow first.
>>>>>>> 19a1b7d (TEST: mirror legacy parser seam for live diff)
 */
inline bool CanReadBoolArrayPayload(ULONGLONG position, ULONGLONG length, uint16 bitCount)
{
	return CanReadSerializedBytes(position, length, (static_cast<ULONGLONG>(bitCount) / 8u) + 1u);
}

/**
 * Reproduces the legacy packet-header decode, including the zero-length underflow.
 */
inline bool TryParsePacketHeader(const BYTE *pBuffer, size_t nBufferSize, ProtocolPacketHeader *pHeader)
{
	if (pBuffer == NULL || pHeader == NULL || nBufferSize < PROTOCOL_PACKET_HEADER_SIZE)
		return false;

	pHeader->nProtocol = pBuffer[0];
	pHeader->nPacketLength = ReadLittleEndianUInt32(pBuffer + 1);
	pHeader->nPayloadLength = pHeader->nPacketLength - 1;
	pHeader->nOpcode = pBuffer[5];
	return true;
}

/**
 * Reproduces the legacy tag-header decode without rejecting truncated explicit names.
 */
inline bool TryParseTagHeader(const BYTE *pBuffer, size_t nBufferSize, ProtocolTagHeader *pHeader)
{
	if (pBuffer == NULL || pHeader == NULL || nBufferSize < 1)
		return false;

	pHeader->nRawType = pBuffer[0];
	pHeader->nType = static_cast<uint8>(pHeader->nRawType & ~0x80u);
	pHeader->nNameId = 0;
	pHeader->nNameLength = 0;
	pHeader->nHeaderSize = 0;
	pHeader->bUsesNameId = false;

	if ((pHeader->nRawType & 0x80u) != 0) {
		if (nBufferSize < 2)
			return false;

		pHeader->nNameId = pBuffer[1];
		pHeader->nNameLength = 1;
		pHeader->nHeaderSize = 2;
		pHeader->bUsesNameId = true;
		return true;
	}

	if (nBufferSize < 3)
		return false;

	const uint16 nNameLength = ReadLittleEndianUInt16(pBuffer + 1);
	if (nNameLength == 1) {
		if (nBufferSize >= 4)
			pHeader->nNameId = pBuffer[3];
		pHeader->nNameLength = 1;
		pHeader->nHeaderSize = 4;
		pHeader->bUsesNameId = true;
		return true;
	}

	pHeader->nNameLength = nNameLength;
	pHeader->nHeaderSize = 3u + static_cast<size_t>(nNameLength);
	return true;
}

/**
 * Reproduces the legacy tag-span decode without rejecting oversized values.
 */
inline bool TryParseTagSpan(const BYTE *pBuffer, size_t nBufferSize, ProtocolTagSpan *pSpan)
{
	if (pBuffer == NULL || pSpan == NULL)
		return false;

	if (!TryParseTagHeader(pBuffer, nBufferSize, &pSpan->Header))
		return false;

	size_t nValueSize = 0;
	uint32 nBlobSize = 0;
	const size_t nValueOffset = pSpan->Header.nHeaderSize;
	switch (pSpan->Header.nType) {
	case TAGTYPE_STRING:
		if (nBufferSize >= nValueOffset + 2)
			nValueSize = 2u + static_cast<size_t>(ReadLittleEndianUInt16(pBuffer + nValueOffset));
		else
			nValueSize = 2;
		break;
	case TAGTYPE_UINT32:
	case TAGTYPE_FLOAT32:
		nValueSize = 4;
		break;
	case TAGTYPE_UINT64:
		nValueSize = 8;
		break;
	case TAGTYPE_UINT16:
		nValueSize = 2;
		break;
	case TAGTYPE_UINT8:
	case TAGTYPE_BOOL:
		nValueSize = 1;
		break;
	case TAGTYPE_HASH:
		nValueSize = 16;
		break;
	case TAGTYPE_BOOLARRAY:
		if (nBufferSize >= nValueOffset + 2)
			nValueSize = 2u + (static_cast<size_t>(ReadLittleEndianUInt16(pBuffer + nValueOffset)) / 8u) + 1u;
		else
			nValueSize = 2;
		break;
	case TAGTYPE_BLOB:
		if (nBufferSize >= nValueOffset + 4)
			nBlobSize = ReadLittleEndianUInt32(pBuffer + nValueOffset);
		nValueSize = 4u + static_cast<size_t>(nBlobSize);
		break;
	default:
		if (pSpan->Header.nType >= TAGTYPE_STR1 && pSpan->Header.nType <= TAGTYPE_STR16)
			nValueSize = static_cast<size_t>(pSpan->Header.nType - TAGTYPE_STR1 + 1u);
		break;
	}

	pSpan->nValueSize = nValueSize;
	pSpan->nTotalSize = pSpan->Header.nHeaderSize + nValueSize;
	pSpan->nBlobSize = nBlobSize;
	return true;
}
