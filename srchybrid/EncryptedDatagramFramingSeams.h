//this file is part of eMule
//Copyright (C)2002-2026 Merkur ( strEmail.Format("%s@%s", "devteam", "emule-project.net") / https://www.emule-project.net )
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.

#pragma once

#include "opcodes.h"
#include "types.h"

enum class EEncryptedDatagramMarkerKind
{
	PlainProtocol,
	Ed2kCandidate,
	KadNodeIdCandidate,
	KadReceiverKeyCandidate,
	ServerCandidate
};

struct EncryptedDatagramFrameSnapshot
{
	EEncryptedDatagramMarkerKind eMarkerKind;
	bool bMarkerAllowed;
	bool bHeaderLongEnough;
	bool bRequiresVerifyKeys;
	uint32 nExpectedOverhead;
};

inline bool IsReservedPlainUdpProtocolMarker(const uint8 byProtocol)
{
	switch (byProtocol) {
	case OP_EMULEPROT:
	case OP_KADEMLIAPACKEDPROT:
	case OP_KADEMLIAHEADER:
	case OP_UDPRESERVEDPROT1:
	case OP_UDPRESERVEDPROT2:
	case OP_PACKEDPROT:
		return true;
	default:
		return false;
	}
}

inline EEncryptedDatagramMarkerKind ClassifyEncryptedDatagramMarker(const uint8 byProtocol, const bool bServerPacket)
{
	if (bServerPacket)
		return IsReservedPlainUdpProtocolMarker(byProtocol) ? EEncryptedDatagramMarkerKind::PlainProtocol : EEncryptedDatagramMarkerKind::ServerCandidate;
	if (IsReservedPlainUdpProtocolMarker(byProtocol))
		return EEncryptedDatagramMarkerKind::PlainProtocol;
	if ((byProtocol & 0x01u) != 0)
		return EEncryptedDatagramMarkerKind::Ed2kCandidate;
	return (byProtocol & 0x02u) != 0 ? EEncryptedDatagramMarkerKind::KadReceiverKeyCandidate : EEncryptedDatagramMarkerKind::KadNodeIdCandidate;
}

inline uint32 GetEncryptedDatagramOverhead(const bool bKadPacket)
{
	return bKadPacket ? 16u : 8u;
}

inline EncryptedDatagramFrameSnapshot InspectEncryptedDatagramFrame(const uint8 byProtocol, const size_t nDatagramLength, const bool bServerPacket)
{
	EncryptedDatagramFrameSnapshot snapshot = {};
	snapshot.eMarkerKind = ClassifyEncryptedDatagramMarker(byProtocol, bServerPacket);
	snapshot.bMarkerAllowed = !IsReservedPlainUdpProtocolMarker(byProtocol);
	snapshot.bRequiresVerifyKeys = !bServerPacket && (snapshot.eMarkerKind == EEncryptedDatagramMarkerKind::KadNodeIdCandidate || snapshot.eMarkerKind == EEncryptedDatagramMarkerKind::KadReceiverKeyCandidate);
	snapshot.nExpectedOverhead = GetEncryptedDatagramOverhead(snapshot.bRequiresVerifyKeys);
	snapshot.bHeaderLongEnough = nDatagramLength >= snapshot.nExpectedOverhead;
	return snapshot;
}
