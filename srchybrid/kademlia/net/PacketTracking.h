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
#include "kademlia/utils/UInt128.h"

namespace Kademlia
{
	struct TrackPackets_Struct
	{
		ULONGLONG dwInserted;
		uint32	dwIP;
		uint8	byOpcode;
	};

	struct TrackChallenge_Struct
	{
		ULONGLONG dwInserted;
		uint32	uIP;
		CUInt128 uContactID;
		CUInt128 uChallenge;
		uint8	byOpcode;
	};

	struct TrackPacketsIn_Struct
	{
		struct TrackedRequestIn_Struct
		{
			ULONGLONG m_dwLatest;
			int		m_tokens; //tokens are equal to milliseconds in this Token Bucket algorithm implementation
			uint8	m_byOpcode;
			bool	m_bDbgLogged;
		};

		TrackPacketsIn_Struct()
			: m_dwLastExpire()
			, m_uIP()
		{
		}

		ULONGLONG m_dwLastExpire;
		uint32	m_uIP;
		CArray<TrackedRequestIn_Struct> m_aTrackedRequests;
	};

	class CPacketTracking
	{
	public:
		CPacketTracking();
		virtual	~CPacketTracking();

	protected:
		void AddTrackedOutPacket(uint32 dwIP, uint8 byOpcode);
		bool IsOnOutTrackList(uint32 dwIP, uint8 byOpcode, bool bDontRemove = false);
		int InTrackListIsAllowedPacket(uint32 uIP, uint8 byOpcode, bool bValidReceiverkey);
		void InTrackListCleanup();
		void AddLegacyChallenge(const CUInt128 &uContactID, const CUInt128 &uChallengeID, uint32 uIP, uint8 byOpcode);
		bool IsLegacyChallenge(const CUInt128 &uChallengeID, uint32 uIP, uint8 byOpcode, CUInt128 &ruContactID);
		bool HasActiveLegacyChallenge(uint32 uIP) const;

	private:
		static bool IsTrackedOutListRequestPacket(uint8 byOpcode);
		CList<TrackPackets_Struct> listTrackedRequests;
		CList<TrackChallenge_Struct> listChallengeRequests;
		CTypedPtrList<CPtrList, TrackPacketsIn_Struct*>	m_liTrackPacketsIn;
		CMap<int, int, TrackPacketsIn_Struct*, TrackPacketsIn_Struct*> m_mapTrackPacketsIn;
		ULONGLONG dwLastTrackInCleanup;
	};
}
