#pragma once

#include <ctime>
#include <map>

#include "KadSupport.h"
#include "kademlia/kademlia/Defines.h"
#include "kademlia/utils/UInt128.h"

namespace Kademlia
{
	/**
	 * Tracks Kad node identity changes and short-lived bad-node state for safer routing decisions.
	 */
	class CSafeKad
	{
	public:
		CSafeKad() throw();

		void TrackNode(uint32 uIP, uint16 uPort, const CUInt128 &uID, bool bIDVerified = false, bool bBanOnVerifiedIdFlip = false) throw();
		void TrackProblematicNode(uint32 uIP, uint16 uPort) throw();
		void BanIP(uint32 uIP) throw();
		bool IsBadNode(uint32 uIP, uint16 uPort, const CUInt128 &uID, uint8 uKadVersion, bool bIDVerified = false, bool bOnlyOneNodePerIP = true, bool bBanOnVerifiedIdFlip = false) throw();
		bool IsBanned(uint32 uIP) throw();
		bool IsProblematic(uint32 uIP, uint16 uPort) throw();
		void ShutdownCleanup() throw();

	private:
		struct NodeAddress
		{
			NodeAddress() throw()
				: m_uIP(0)
				, m_uPort(0)
			{
			}

			NodeAddress(uint32 uIP, uint16 uPort) throw()
				: m_uIP(uIP)
				, m_uPort(uPort)
			{
			}

			bool operator<(const NodeAddress &other) const throw()
			{
				return (m_uIP < other.m_uIP) || (m_uIP == other.m_uIP && m_uPort < other.m_uPort);
			}

			uint32 m_uIP;
			uint16 m_uPort;
		};

		struct TrackedNode
		{
			CUInt128 m_uLastID;
			time_t m_tLastIDChange;
			time_t m_tLastReferenced;
			bool m_bIDVerified;
		};

		struct ProblematicNode
		{
			time_t m_tFailed;
			time_t m_tLastReferenced;
		};

		struct BannedIP
		{
			time_t m_tBanned;
			time_t m_tLastReferenced;
		};

		void Cleanup() throw();

		enum
		{
			s_uMaxTrackedNodes = 10000,
			s_uMaxProblematicNodes = 10000,
			s_uMaxBannedIPs = 1000
		};

		static const time_t s_tMinimumIDChangeInterval = 3600;
		static const time_t s_tMaximumBanTime = 4 * 3600;
		static const time_t s_tMaximumProblematicTime = 5 * 60;
		static const time_t s_tMaximumTrackedRefAge = 3600;
		static const time_t s_tMaximumBannedRefAge = 3600;

		std::map<NodeAddress, TrackedNode> m_mapTrackedNodes;
		std::map<NodeAddress, ProblematicNode> m_mapProblematicNodes;
		std::map<uint32, BannedIP> m_mapBannedIPs;
		time_t m_tLastCleanup;
	};

	extern CSafeKad safeKad;
}
