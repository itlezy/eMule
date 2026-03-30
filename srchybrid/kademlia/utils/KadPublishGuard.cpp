#include "KadPublishGuard.h"

namespace Kademlia
{
	bool IsLowIdPublishSourceType(uint8 uSourceType)
	{
		return uSourceType == 3 || uSourceType == 5;
	}

	bool IsAcceptedPublishSourceType(uint8 uSourceType)
	{
		switch (uSourceType) {
		case 1:
		case 3:
		case 4:
		case 5:
		case 6:
			return true;
		}
		return false;
	}

	bool ValidatePublishSourceMetadata(const PublishSourceMetadata &metadata)
	{
		if (!metadata.m_bHasSourceType || !IsAcceptedPublishSourceType(metadata.m_uSourceType))
			return false;
		if (!metadata.m_bHasSourcePort || metadata.m_uSourcePort == 0)
			return false;
		if (IsLowIdPublishSourceType(metadata.m_uSourceType)) {
			if (!metadata.m_bHasBuddyIP || metadata.m_uBuddyIP == 0)
				return false;
			if (!metadata.m_bHasBuddyPort || metadata.m_uBuddyPort == 0)
				return false;
			if (!metadata.m_bHasBuddyHash)
				return false;
		}
		return true;
	}

	CKadPublishSourceThrottle::CKadPublishSourceThrottle()
		: m_dwLastCleanup(0)
	{
	}

	EKadPublishThrottleDecision CKadPublishSourceThrottle::TrackRequest(uint32 uIP, uint32 dwNow, uint32 uThresholdPerMinute)
	{
		if (uThresholdPerMinute == 0)
			return KPUBLISH_ALLOW;

		if (dwNow >= m_dwLastCleanup + 60000)
			Cleanup(dwNow);

		ThrottleEntry &entry = m_mapEntries[uIP];
		if (entry.m_dwWindowStart == 0 || dwNow - entry.m_dwWindowStart >= 60000) {
			entry.m_dwWindowStart = dwNow;
			entry.m_uCount = 0;
		}
		entry.m_dwLastSeen = dwNow;
		++entry.m_uCount;

		if (entry.m_uCount > uThresholdPerMinute * 3)
			return KPUBLISH_BAN;
		if (entry.m_uCount > uThresholdPerMinute)
			return KPUBLISH_DROP;
		return KPUBLISH_ALLOW;
	}

	void CKadPublishSourceThrottle::Reset()
	{
		m_mapEntries.clear();
		m_dwLastCleanup = 0;
	}

	void CKadPublishSourceThrottle::Cleanup(uint32 dwNow)
	{
		for (std::map<uint32, ThrottleEntry>::iterator itEntry = m_mapEntries.begin(); itEntry != m_mapEntries.end();) {
			if (dwNow - itEntry->second.m_dwLastSeen >= 10 * 60000)
				itEntry = m_mapEntries.erase(itEntry);
			else
				++itEntry;
		}
		m_dwLastCleanup = dwNow;
	}
}
