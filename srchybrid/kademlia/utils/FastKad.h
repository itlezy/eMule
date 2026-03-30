#pragma once

#include <cstdio>
#include <ctime>
#include <map>
#include <vector>

#include "KadSupport.h"
#include "UInt128.h"

namespace Kademlia
{
	/**
	 * Learns recent Kad response times, persists bootstrap-quality hints, and exposes a conservative pending-response timeout.
	 */
	class CFastKad
	{
	public:
		struct NodeKey
		{
			NodeKey();
			NodeKey(const CUInt128 &uID, uint16 uUDPPort);

			bool operator<(const NodeKey &other) const;

			byte m_abyID[16];
			uint16 m_uUDPPort;
		};

		CFastKad();

		void AddResponseTime(uint32 uIP, clock_t clkResponseTime);
		void TrackNodeResponse(const CUInt128 &uID, uint16 uUDPPort, uint32 uIP, clock_t clkResponseTime);
		void TrackNodeReachable(const CUInt128 &uID, uint16 uUDPPort);
		void TrackNodeFailure(const CUInt128 &uID, uint16 uUDPPort);
		void LoadNodesMetadata(LPCTSTR pszFilename);
		void SaveNodesMetadata(LPCTSTR pszFilename, const std::vector<NodeKey> &knownNodes) const;
		uint64 GetBootstrapPriority(const CUInt128 &uID, uint16 uUDPPort) const;
		clock_t GetEstMaxResponseTime() const { return m_clkEstResponseTime; }
		void ShutdownCleanup();

	private:
		struct ResponseTimeEntry
		{
			clock_t m_clkResponseTime;
			clock_t m_clkLastReferenced;
		};

		struct NodeState
		{
			NodeState();

			uint64 m_uLastSuccessTime;
			uint32 m_uResponseTimeMs;
			sint16 m_nHealthScore;
		};

		struct SidecarEntry
		{
			NodeKey m_key;
			NodeState m_state;
		};

		void ClampHealth(NodeState &state) const;
		NodeState &GetOrCreateNodeState(const CUInt128 &uID, uint16 uUDPPort);
		const NodeState *FindNodeState(const CUInt128 &uID, uint16 uUDPPort) const;
		void RecalculateResponseTime();
		static bool ReadExact(FILE *pFile, void *pBuffer, size_t uBytes);
		static bool WriteExact(FILE *pFile, const void *pBuffer, size_t uBytes);

		enum
		{
			s_uMaxResponseTimes = 100,
			s_uSidecarMagic = 0x3146444Bu, // 'KDF1'
			s_uSidecarVersion = 1
		};

		std::map<uint32, ResponseTimeEntry> m_mapResponseTimes;
		std::map<NodeKey, NodeState> m_mapNodeStates;
		double m_fResponseTimeMean;
		double m_fResponseTimeVariance;
		clock_t m_clkEstResponseTime;
	};

	extern CFastKad fastKad;
}
