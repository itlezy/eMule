#include "FastKad.h"

#include <algorithm>
#include <cmath>

namespace Kademlia
{
	CFastKad fastKad;

	CFastKad::CFastKad()
		: m_fResponseTimeMean(0.0)
		, m_fResponseTimeVariance(0.0)
		, m_clkEstResponseTime(0)
	{
		AddResponseTime(0, CLOCKS_PER_SEC);
	}

	void CFastKad::ShutdownCleanup()
	{
		m_mapResponseTimes.clear();
		m_fResponseTimeMean = 0.0;
		m_fResponseTimeVariance = 0.0;
		m_clkEstResponseTime = 0;
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
}
