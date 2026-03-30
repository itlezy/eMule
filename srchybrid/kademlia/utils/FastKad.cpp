#include "FastKad.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
	const uint64 FASTKAD_RECENT_SUCCESS_AGE = 7ull * 24ull * 60ull * 60ull;
	const uint64 FASTKAD_WARM_SUCCESS_AGE = 30ull * 24ull * 60ull * 60ull;
	const uint64 FASTKAD_STALE_SUCCESS_AGE = 90ull * 24ull * 60ull * 60ull;
}

namespace Kademlia
{
	CFastKad fastKad;

	CFastKad::NodeKey::NodeKey()
		: m_uUDPPort(0)
	{
		memset(m_abyID, 0, sizeof m_abyID);
	}

	CFastKad::NodeKey::NodeKey(const CUInt128 &uID, uint16 uUDPPort)
		: m_uUDPPort(uUDPPort)
	{
		memcpy(m_abyID, uID.GetData(), sizeof m_abyID);
	}

	bool CFastKad::NodeKey::operator<(const NodeKey &other) const
	{
		const int nCompare = memcmp(m_abyID, other.m_abyID, sizeof m_abyID);
		return nCompare < 0 || (nCompare == 0 && m_uUDPPort < other.m_uUDPPort);
	}

	CFastKad::NodeState::NodeState()
		: m_uLastSuccessTime(0)
		, m_uResponseTimeMs(0)
		, m_nHealthScore(0)
	{
	}

	CFastKad::CFastKad()
		: m_fResponseTimeMean(0.0)
		, m_fResponseTimeVariance(0.0)
		, m_clkEstResponseTime(0)
	{
		AddResponseTime(0, CLOCKS_PER_SEC);
	}

	void CFastKad::AddResponseTime(uint32 uIP, clock_t clkResponseTime)
	{
		const clock_t clkNow = clock();
		if (m_mapResponseTimes.find(uIP) == m_mapResponseTimes.end() && m_mapResponseTimes.size() >= s_uMaxResponseTimes) {
			clock_t clkOldestAge = 0;
			std::map<uint32, ResponseTimeEntry>::iterator itOldest = m_mapResponseTimes.end();
			for (std::map<uint32, ResponseTimeEntry>::iterator itEntry = m_mapResponseTimes.begin(); itEntry != m_mapResponseTimes.end(); ++itEntry) {
				const clock_t clkAge = clkNow - itEntry->second.m_clkLastReferenced;
				if (clkAge >= clkOldestAge && clkAge > (5 * 60 * CLOCKS_PER_SEC)) {
					clkOldestAge = clkAge;
					itOldest = itEntry;
				}
			}
			if (itOldest != m_mapResponseTimes.end())
				m_mapResponseTimes.erase(itOldest);
			else
				return;
		}

		ResponseTimeEntry &entry = m_mapResponseTimes[uIP];
		entry.m_clkResponseTime = clkResponseTime;
		entry.m_clkLastReferenced = clkNow;
		RecalculateResponseTime();
	}

	void CFastKad::TrackNodeResponse(const CUInt128 &uID, uint16 uUDPPort, uint32 uIP, clock_t clkResponseTime)
	{
		AddResponseTime(uIP, clkResponseTime);

		NodeState &state = GetOrCreateNodeState(uID, uUDPPort);
		state.m_uLastSuccessTime = static_cast<uint64>(time(NULL));
		state.m_uResponseTimeMs = static_cast<uint32>(std::min<ULONGLONG>(60000ull, (static_cast<ULONGLONG>(clkResponseTime) * 1000ull) / CLOCKS_PER_SEC));
		state.m_nHealthScore = static_cast<sint16>(state.m_nHealthScore + 4);
		ClampHealth(state);
	}

	void CFastKad::TrackNodeReachable(const CUInt128 &uID, uint16 uUDPPort)
	{
		NodeState &state = GetOrCreateNodeState(uID, uUDPPort);
		state.m_uLastSuccessTime = static_cast<uint64>(time(NULL));
		state.m_nHealthScore = static_cast<sint16>(state.m_nHealthScore + 2);
		ClampHealth(state);
	}

	void CFastKad::TrackNodeFailure(const CUInt128 &uID, uint16 uUDPPort)
	{
		NodeState &state = GetOrCreateNodeState(uID, uUDPPort);
		state.m_nHealthScore = static_cast<sint16>(state.m_nHealthScore - 8);
		ClampHealth(state);
	}

	void CFastKad::LoadNodesMetadata(LPCTSTR pszFilename)
	{
		m_mapNodeStates.clear();

		FILE *pFile = NULL;
		if (_wfopen_s(&pFile, pszFilename, L"rb") != 0 || pFile == NULL)
			return;

		uint32 uMagic = 0;
		uint32 uVersion = 0;
		uint32 uCount = 0;
		bool bValidFile = ReadExact(pFile, &uMagic, sizeof uMagic)
			&& ReadExact(pFile, &uVersion, sizeof uVersion)
			&& ReadExact(pFile, &uCount, sizeof uCount)
			&& uMagic == s_uSidecarMagic
			&& uVersion == s_uSidecarVersion;

		for (uint32 uIndex = 0; bValidFile && uIndex < uCount; ++uIndex) {
			SidecarEntry entry;
			bValidFile = ReadExact(pFile, entry.m_key.m_abyID, sizeof entry.m_key.m_abyID)
				&& ReadExact(pFile, &entry.m_key.m_uUDPPort, sizeof entry.m_key.m_uUDPPort)
				&& ReadExact(pFile, &entry.m_state.m_uLastSuccessTime, sizeof entry.m_state.m_uLastSuccessTime)
				&& ReadExact(pFile, &entry.m_state.m_uResponseTimeMs, sizeof entry.m_state.m_uResponseTimeMs)
				&& ReadExact(pFile, &entry.m_state.m_nHealthScore, sizeof entry.m_state.m_nHealthScore);
			if (bValidFile) {
				ClampHealth(entry.m_state);
				m_mapNodeStates[entry.m_key] = entry.m_state;
			}
		}

		fclose(pFile);
		if (!bValidFile)
			m_mapNodeStates.clear();
	}

	void CFastKad::SaveNodesMetadata(LPCTSTR pszFilename, const std::vector<NodeKey> &knownNodes) const
	{
		std::vector<SidecarEntry> records;
		records.reserve(knownNodes.size());

		for (std::vector<NodeKey>::const_iterator itKey = knownNodes.begin(); itKey != knownNodes.end(); ++itKey) {
			std::map<NodeKey, NodeState>::const_iterator itState = m_mapNodeStates.find(*itKey);
			if (itState == m_mapNodeStates.end())
				continue;
			if (itState->second.m_uLastSuccessTime == 0 && itState->second.m_uResponseTimeMs == 0 && itState->second.m_nHealthScore == 0)
				continue;

			SidecarEntry entry;
			entry.m_key = *itKey;
			entry.m_state = itState->second;
			records.push_back(entry);
		}

		if (records.empty()) {
			::DeleteFile(pszFilename);
			return;
		}

		FILE *pFile = NULL;
		if (_wfopen_s(&pFile, pszFilename, L"wb") != 0 || pFile == NULL)
			return;

		const uint32 uMagic = s_uSidecarMagic;
		const uint32 uVersion = s_uSidecarVersion;
		const uint32 uCount = static_cast<uint32>(records.size());
		bool bWritten = WriteExact(pFile, &uMagic, sizeof uMagic)
			&& WriteExact(pFile, &uVersion, sizeof uVersion)
			&& WriteExact(pFile, &uCount, sizeof uCount);

		for (std::vector<SidecarEntry>::const_iterator itEntry = records.begin(); bWritten && itEntry != records.end(); ++itEntry) {
			bWritten = WriteExact(pFile, itEntry->m_key.m_abyID, sizeof itEntry->m_key.m_abyID)
				&& WriteExact(pFile, &itEntry->m_key.m_uUDPPort, sizeof itEntry->m_key.m_uUDPPort)
				&& WriteExact(pFile, &itEntry->m_state.m_uLastSuccessTime, sizeof itEntry->m_state.m_uLastSuccessTime)
				&& WriteExact(pFile, &itEntry->m_state.m_uResponseTimeMs, sizeof itEntry->m_state.m_uResponseTimeMs)
				&& WriteExact(pFile, &itEntry->m_state.m_nHealthScore, sizeof itEntry->m_state.m_nHealthScore);
		}

		fclose(pFile);
		if (!bWritten)
			::DeleteFile(pszFilename);
	}

	uint64 CFastKad::GetBootstrapPriority(const CUInt128 &uID, uint16 uUDPPort) const
	{
		const NodeState *pState = FindNodeState(uID, uUDPPort);
		if (pState == NULL)
			return 0;

		const uint64 uNow = static_cast<uint64>(time(NULL));
		uint64 uRecencyBucket = 0;
		if (pState->m_uLastSuccessTime > 0 && pState->m_uLastSuccessTime <= uNow) {
			const uint64 uAge = uNow - pState->m_uLastSuccessTime;
			if (uAge <= FASTKAD_RECENT_SUCCESS_AGE)
				uRecencyBucket = 4;
			else if (uAge <= FASTKAD_WARM_SUCCESS_AGE)
				uRecencyBucket = 3;
			else if (uAge <= FASTKAD_STALE_SUCCESS_AGE)
				uRecencyBucket = 2;
			else
				uRecencyBucket = 1;
		}

		const sint32 nHealth = std::max<sint32>(-100, std::min<sint32>(100, pState->m_nHealthScore));
		const uint32 uHealthScore = static_cast<uint32>(nHealth + 100);
		const uint32 uLatencyScore = (pState->m_uResponseTimeMs == 0)
			? 30000u
			: 60000u - std::min<uint32>(60000u, pState->m_uResponseTimeMs);

		return (uRecencyBucket << 48) | (static_cast<uint64>(uHealthScore) << 24) | uLatencyScore;
	}

	void CFastKad::ShutdownCleanup()
	{
		m_mapResponseTimes.clear();
		m_mapNodeStates.clear();
		m_fResponseTimeMean = 0.0;
		m_fResponseTimeVariance = 0.0;
		m_clkEstResponseTime = 0;
		AddResponseTime(0, CLOCKS_PER_SEC);
	}

	void CFastKad::ClampHealth(NodeState &state) const
	{
		state.m_nHealthScore = static_cast<sint16>(std::max<sint32>(-100, std::min<sint32>(100, state.m_nHealthScore)));
	}

	CFastKad::NodeState &CFastKad::GetOrCreateNodeState(const CUInt128 &uID, uint16 uUDPPort)
	{
		return m_mapNodeStates[NodeKey(uID, uUDPPort)];
	}

	const CFastKad::NodeState *CFastKad::FindNodeState(const CUInt128 &uID, uint16 uUDPPort) const
	{
		std::map<NodeKey, NodeState>::const_iterator itState = m_mapNodeStates.find(NodeKey(uID, uUDPPort));
		return (itState != m_mapNodeStates.end()) ? &itState->second : NULL;
	}

	void CFastKad::RecalculateResponseTime()
	{
		double fResponseTimeSum = 0.0;
		const double fResponseTimeCount = static_cast<double>(m_mapResponseTimes.size());
		double fResponseTimeVarianceSum = 0.0;
		const double fResponseTimeCountMissing = (m_mapResponseTimes.size() < s_uMaxResponseTimes)
			? static_cast<double>(s_uMaxResponseTimes) - fResponseTimeCount
			: 0.0;

		for (std::map<uint32, ResponseTimeEntry>::const_iterator itEntry = m_mapResponseTimes.begin(); itEntry != m_mapResponseTimes.end(); ++itEntry)
			fResponseTimeSum += static_cast<double>(itEntry->second.m_clkResponseTime);
		fResponseTimeSum += fResponseTimeCountMissing * static_cast<double>(CLOCKS_PER_SEC);
		m_fResponseTimeMean = fResponseTimeSum / static_cast<double>(s_uMaxResponseTimes);

		if (m_mapResponseTimes.size() > 1) {
			for (std::map<uint32, ResponseTimeEntry>::const_iterator itEntry = m_mapResponseTimes.begin(); itEntry != m_mapResponseTimes.end(); ++itEntry)
				fResponseTimeVarianceSum += pow(static_cast<double>(itEntry->second.m_clkResponseTime) - m_fResponseTimeMean, 2.0);
		}
		fResponseTimeVarianceSum += fResponseTimeCountMissing * static_cast<double>(CLOCKS_PER_SEC);
		m_fResponseTimeVariance = fResponseTimeVarianceSum / static_cast<double>(s_uMaxResponseTimes - 1);

		const clock_t clkEstimate = static_cast<clock_t>(m_fResponseTimeMean + 2.0 * sqrt(m_fResponseTimeVariance)) + CLOCKS_PER_SEC / 10;
		m_clkEstResponseTime = std::min<clock_t>(3 * CLOCKS_PER_SEC, std::max<clock_t>(CLOCKS_PER_SEC / 2, clkEstimate));
	}

	bool CFastKad::ReadExact(FILE *pFile, void *pBuffer, size_t uBytes)
	{
		return fread(pBuffer, 1, uBytes, pFile) == uBytes;
	}

	bool CFastKad::WriteExact(FILE *pFile, const void *pBuffer, size_t uBytes)
	{
		return fwrite(pBuffer, 1, uBytes, pFile) == uBytes;
	}
}
