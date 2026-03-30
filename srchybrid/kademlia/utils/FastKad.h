#pragma once

#include <ctime>
#include <map>

#include "KadSupport.h"

namespace Kademlia
{
	/**
	 * Learns recent Kad response times and exposes a conservative pending-response timeout.
	 */
	class CFastKad
	{
	public:
		CFastKad();

		void AddResponseTime(uint32 uIP, clock_t clkResponseTime);
		clock_t GetEstMaxResponseTime() const { return m_clkEstResponseTime; }
		void ShutdownCleanup();

	private:
		struct ResponseTimeEntry
		{
			clock_t m_clkResponseTime;
			clock_t m_clkLastReferenced;
		};

		void RecalculateResponseTime();

		enum { s_uMaxResponseTimes = 100 };

		std::map<uint32, ResponseTimeEntry> m_mapResponseTimes;
		double m_fResponseTimeMean;
		double m_fResponseTimeVariance;
		clock_t m_clkEstResponseTime;
	};

	extern CFastKad fastKad;
}
