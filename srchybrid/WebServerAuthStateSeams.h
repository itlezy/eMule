#pragma once

#include <cstdint>

namespace WebServerAuthStateSeams
{
constexpr int kMaxRecentBadLoginFaults = 4;

inline bool IsBadLoginExpired(const uint64_t ullCurrentTick, const uint64_t ullBadLoginTick, const uint64_t ullWindowMilliseconds)
{
	return ullCurrentTick >= ullBadLoginTick + ullWindowMilliseconds;
}

inline bool ShouldDenyForBadLoginFaults(const int nRecentFaults)
{
	return nRecentFaults > kMaxRecentBadLoginFaults;
}

inline bool IsSessionExpired(const __int64 llAgeSeconds, const int nTimeoutMinutes)
{
	return nTimeoutMinutes > 0 && llAgeSeconds >= static_cast<__int64>(nTimeoutMinutes) * 60;
}
}
