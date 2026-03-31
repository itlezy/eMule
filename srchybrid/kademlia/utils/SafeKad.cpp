#include "srchybrid/Opcodes.h"
#include "SafeKad.h"

namespace Kademlia
{
	CSafeKad safeKad;

	CSafeKad::CSafeKad() throw()
		: m_tLastCleanup(time(NULL))
	{
	}

	void CSafeKad::TrackNode(uint32 uIP, uint16 uPort, const CUInt128 &uID, bool bIDVerified, bool bBanOnVerifiedIdFlip) throw()
	{
		if (IsBanned(uIP))
			return;

		const time_t tNow = time(NULL);
		if (tNow - m_tLastCleanup > 10)
			Cleanup();

		const NodeAddress address(uIP, uPort);
		TrackedNode &tracked = m_mapTrackedNodes[address];
		if (tracked.m_tLastReferenced == 0 && m_mapTrackedNodes.size() > s_uMaxTrackedNodes) {
			m_mapTrackedNodes.erase(address);
			return;
		}

		if (tracked.m_tLastReferenced == 0) {
			tracked.m_uLastID = uID;
			tracked.m_tLastIDChange = tNow;
			tracked.m_bIDVerified = bIDVerified;
		} else if (uID != tracked.m_uLastID && !(tracked.m_bIDVerified && !bIDVerified)) {
			if (bBanOnVerifiedIdFlip && bIDVerified && tNow - tracked.m_tLastIDChange < s_tMinimumIDChangeInterval) {
				BanIP(uIP);
				m_mapTrackedNodes.erase(address);
				return;
			}
			tracked.m_uLastID = uID;
			tracked.m_tLastIDChange = tNow;
		}

		tracked.m_tLastReferenced = tNow;
		if (!tracked.m_bIDVerified)
			tracked.m_bIDVerified = bIDVerified;
	}

	void CSafeKad::TrackProblematicNode(uint32 uIP, uint16 uPort) throw()
	{
		if (IsBanned(uIP))
			return;

		const time_t tNow = time(NULL);
		if (tNow - m_tLastCleanup > 10)
			Cleanup();

		const NodeAddress address(uIP, uPort);
		ProblematicNode &problematic = m_mapProblematicNodes[address];
		if (problematic.m_tLastReferenced == 0 && m_mapProblematicNodes.size() > s_uMaxProblematicNodes) {
			m_mapProblematicNodes.erase(address);
			return;
		}

		if (problematic.m_tFailed == 0)
			problematic.m_tFailed = tNow;
		problematic.m_tLastReferenced = tNow;
	}

	void CSafeKad::BanIP(uint32 uIP) throw()
	{
		const time_t tNow = time(NULL);
		if (tNow - m_tLastCleanup > 10)
			Cleanup();

		BannedIP &banned = m_mapBannedIPs[uIP];
		if (banned.m_tLastReferenced == 0 && m_mapBannedIPs.size() > s_uMaxBannedIPs) {
			m_mapBannedIPs.erase(uIP);
			return;
		}

		banned.m_tBanned = tNow;
		banned.m_tLastReferenced = tNow;
	}

	bool CSafeKad::IsBadNode(uint32 uIP, uint16 uPort, const CUInt128 &uID, uint8 uKadVersion, bool bIDVerified, bool bOnlyOneNodePerIP, bool bBanOnVerifiedIdFlip) throw()
	{
		const time_t tNow = time(NULL);
		if (tNow - m_tLastCleanup > 10)
			Cleanup();

		if (IsBanned(uIP))
			return true;

		const NodeAddress address(uIP, uPort);
		std::map<NodeAddress, TrackedNode>::iterator itTracked = m_mapTrackedNodes.find(address);
		if (itTracked != m_mapTrackedNodes.end()) {
			itTracked->second.m_tLastReferenced = tNow;
			if (itTracked->second.m_uLastID != uID) {
				if ((itTracked->second.m_bIDVerified || uKadVersion < KADEMLIA_VERSION8_49b) && !bIDVerified)
					return true;
				TrackNode(uIP, uPort, uID, bIDVerified, bBanOnVerifiedIdFlip);
				return IsBanned(uIP);
			}
			return false;
		}

		if (bOnlyOneNodePerIP) {
			for (std::map<NodeAddress, TrackedNode>::const_iterator itNode = m_mapTrackedNodes.begin(); itNode != m_mapTrackedNodes.end(); ++itNode) {
				if (itNode->first.m_uIP == uIP)
					return true;
			}
		}

		TrackNode(uIP, uPort, uID, bIDVerified, bBanOnVerifiedIdFlip);
		return IsBanned(uIP);
	}

	bool CSafeKad::IsBanned(uint32 uIP) throw()
	{
		const time_t tNow = time(NULL);
		std::map<uint32, BannedIP>::iterator itBanned = m_mapBannedIPs.find(uIP);
		if (itBanned == m_mapBannedIPs.end())
			return false;

		if (tNow - itBanned->second.m_tBanned > s_tMaximumBanTime) {
			m_mapBannedIPs.erase(itBanned);
			return false;
		}

		itBanned->second.m_tLastReferenced = tNow;
		return true;
	}

	bool CSafeKad::IsProblematic(uint32 uIP, uint16 uPort) throw()
	{
		const time_t tNow = time(NULL);
		const NodeAddress address(uIP, uPort);
		std::map<NodeAddress, ProblematicNode>::iterator itProblematic = m_mapProblematicNodes.find(address);
		if (itProblematic != m_mapProblematicNodes.end()) {
			if (tNow - itProblematic->second.m_tFailed > s_tMaximumProblematicTime) {
				m_mapProblematicNodes.erase(itProblematic);
			} else {
				itProblematic->second.m_tLastReferenced = tNow;
				return true;
			}
		}
		return IsBanned(uIP);
	}

	void CSafeKad::ShutdownCleanup() throw()
	{
		m_mapTrackedNodes.clear();
		m_mapProblematicNodes.clear();
		m_mapBannedIPs.clear();
		m_tLastCleanup = time(NULL);
	}

	void CSafeKad::Cleanup() throw()
	{
		const time_t tNow = time(NULL);
		for (std::map<NodeAddress, TrackedNode>::iterator itTracked = m_mapTrackedNodes.begin(); itTracked != m_mapTrackedNodes.end();) {
			if (tNow - itTracked->second.m_tLastReferenced > s_tMaximumTrackedRefAge)
				itTracked = m_mapTrackedNodes.erase(itTracked);
			else
				++itTracked;
		}
		for (std::map<NodeAddress, ProblematicNode>::iterator itProblematic = m_mapProblematicNodes.begin(); itProblematic != m_mapProblematicNodes.end();) {
			if (tNow - itProblematic->second.m_tLastReferenced > s_tMaximumProblematicTime)
				itProblematic = m_mapProblematicNodes.erase(itProblematic);
			else
				++itProblematic;
		}
		for (std::map<uint32, BannedIP>::iterator itBanned = m_mapBannedIPs.begin(); itBanned != m_mapBannedIPs.end();) {
			if (tNow - itBanned->second.m_tLastReferenced > s_tMaximumBannedRefAge || tNow - itBanned->second.m_tBanned > s_tMaximumBanTime)
				itBanned = m_mapBannedIPs.erase(itBanned);
			else
				++itBanned;
		}
		m_tLastCleanup = tNow;
	}
}
