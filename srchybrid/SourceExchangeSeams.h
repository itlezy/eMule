#pragma once

#include <cstdint>

#include "Opcodes.h"

namespace SourceExchangeSeams
{
struct ResponsePlan
{
	bool bShouldSend = false;
	uint8_t byUsedVersion = 0;
	uint8_t byAnswerOpcode = 0;
	uint8_t nCountSeekOffset = 0;
};

inline bool ShouldAllowSourceExchangeRequest(const bool bExtProtocolAvailable, const bool bSupportsSourceExchange2) noexcept
{
	return bExtProtocolAvailable && bSupportsSourceExchange2;
}

inline bool IsValidSourceExchange2Request(const uint8_t byRequestedVersion) noexcept
{
	return byRequestedVersion > 0;
}

inline ResponsePlan ResolveSourceExchangeResponsePlan(const bool bSupportsSourceExchange2, const uint8_t byRequestedVersion) noexcept
{
	ResponsePlan plan;
	if (!bSupportsSourceExchange2 || !IsValidSourceExchange2Request(byRequestedVersion))
		return plan;

	plan.bShouldSend = true;
	plan.byUsedVersion = (byRequestedVersion < SOURCEEXCHANGE2_VERSION) ? byRequestedVersion : static_cast<uint8_t>(SOURCEEXCHANGE2_VERSION);
	plan.byAnswerOpcode = OP_ANSWERSOURCES2;
	plan.nCountSeekOffset = 17;
	return plan;
}
}
