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
