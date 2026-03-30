#pragma once

#include <map>

#include "KadSupport.h"

namespace Kademlia
{
	/**
	 * Describes the source-specific fields extracted from a Kad publish-source request.
	 */
	struct PublishSourceMetadata
	{
		PublishSourceMetadata()
			: m_bHasSourceType(false)
			, m_uSourceType(0)
			, m_bHasSourcePort(false)
			, m_uSourcePort(0)
			, m_bHasBuddyIP(false)
			, m_uBuddyIP(0)
			, m_bHasBuddyPort(false)
			, m_uBuddyPort(0)
			, m_bHasBuddyHash(false)
		{
		}

		bool m_bHasSourceType;
		uint8 m_uSourceType;
		bool m_bHasSourcePort;
		uint16 m_uSourcePort;
		bool m_bHasBuddyIP;
		uint32 m_uBuddyIP;
		bool m_bHasBuddyPort;
		uint16 m_uBuddyPort;
		bool m_bHasBuddyHash;
	};

	enum EKadPublishThrottleDecision
	{
		KPUBLISH_ALLOW,
		KPUBLISH_DROP,
		KPUBLISH_BAN
	};

	bool IsLowIdPublishSourceType(uint8 uSourceType);
	bool IsAcceptedPublishSourceType(uint8 uSourceType);
	bool ValidatePublishSourceMetadata(const PublishSourceMetadata &metadata);

	/**
	 * Applies a dedicated per-IP publish-source rate limit on top of generic Kad flood tracking.
	 */
	class CKadPublishSourceThrottle
	{
	public:
		CKadPublishSourceThrottle();

		EKadPublishThrottleDecision TrackRequest(uint32 uIP, uint32 dwNow, uint32 uThresholdPerMinute);
		void Reset();

	private:
		struct ThrottleEntry
		{
			uint32 m_dwWindowStart;
			uint32 m_uCount;
			uint32 m_dwLastSeen;
		};

		void Cleanup(uint32 dwNow);

		std::map<uint32, ThrottleEntry> m_mapEntries;
		uint32 m_dwLastCleanup;
	};
}
