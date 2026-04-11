#pragma once

#include <atlstr.h>
#include <cstdint>

namespace UploadScoreSeams
{
enum UploadScoreAvailability
{
	uploadScoreUnavailable,
	uploadScoreAvailable,
	uploadScoreFriendSlot,
	uploadScoreCooldown
};

struct UploadScoreInputs
{
	std::uint64_t uBaseValueMs;
	float fCreditRatio;
	int iFilePrioNumber;
	bool bUseCreditSystem;
	bool bApplyPriority;
	bool bApplyLowRatioBonus;
	std::uint32_t uLowRatioBonus;
	bool bApplyLowIdDivisor;
	std::uint32_t uLowIdDivisor;
	bool bApplyOldClientPenalty;
	bool bCooldownSuppressed;
};

struct UploadScoreBreakdown
{
	UploadScoreAvailability eAvailability;
	float fCoreScoreFloat;
	float fEffectiveScoreFloat;
	float fCreditRatio;
	int iFilePrioNumber;
	std::uint32_t uLowRatioBonus;
	std::uint32_t uLowIdDivisor;
	bool bLowRatioApplied;
	bool bLowIdPenaltyApplied;
	bool bOldClientPenaltyApplied;
	std::uint32_t uBaseScore;
	std::uint32_t uEffectiveScore;
};

inline UploadScoreBreakdown BuildUploadScoreBreakdown(const UploadScoreInputs &rInputs)
{
	UploadScoreBreakdown breakdown = {};
	breakdown.eAvailability = rInputs.bCooldownSuppressed ? uploadScoreCooldown : uploadScoreAvailable;
	breakdown.fCreditRatio = rInputs.fCreditRatio;
	breakdown.iFilePrioNumber = rInputs.iFilePrioNumber;
	breakdown.uLowRatioBonus = rInputs.uLowRatioBonus;
	breakdown.uLowIdDivisor = rInputs.uLowIdDivisor;

	float fWorkingScore = static_cast<float>(rInputs.uBaseValueMs / 1000.0);
	if (rInputs.bUseCreditSystem)
		fWorkingScore *= rInputs.fCreditRatio;
	if (rInputs.bApplyPriority)
		fWorkingScore *= static_cast<float>(rInputs.iFilePrioNumber) / 10.0f;

	float fBaseScore = fWorkingScore;
	if (rInputs.bApplyOldClientPenalty)
		fBaseScore *= 0.5f;
	breakdown.fCoreScoreFloat = fBaseScore;
	breakdown.uBaseScore = static_cast<std::uint32_t>(fBaseScore);

	if (rInputs.bApplyLowRatioBonus && rInputs.uLowRatioBonus > 0) {
		fWorkingScore += static_cast<float>(rInputs.uLowRatioBonus);
		breakdown.bLowRatioApplied = true;
	}

	if (rInputs.bApplyLowIdDivisor && rInputs.uLowIdDivisor > 1) {
		fWorkingScore /= static_cast<float>(rInputs.uLowIdDivisor);
		breakdown.bLowIdPenaltyApplied = true;
	}

	if (rInputs.bApplyOldClientPenalty) {
		fWorkingScore *= 0.5f;
		breakdown.bOldClientPenaltyApplied = true;
	}

	breakdown.fEffectiveScoreFloat = fWorkingScore;
	breakdown.uEffectiveScore = rInputs.bCooldownSuppressed ? 0u : static_cast<std::uint32_t>(fWorkingScore);
	return breakdown;
}

inline float ComputeCombinedFilePrioAndCredit(float fCreditRatio, int iFilePrioNumber)
{
	return 10.0f * fCreditRatio * static_cast<float>(iFilePrioNumber);
}

inline std::uint32_t BuildUploadScoreModifierMask(const UploadScoreBreakdown &rBreakdown)
{
	return (rBreakdown.eAvailability == uploadScoreCooldown ? 4u : 0u)
		| (rBreakdown.bLowRatioApplied ? 2u : 0u)
		| (rBreakdown.bLowIdPenaltyApplied ? 1u : 0u);
}

inline std::uint32_t BuildUploadScoreModifierSortKey(const UploadScoreBreakdown &rBreakdown)
{
	return (BuildUploadScoreModifierMask(rBreakdown) << 24)
		| ((rBreakdown.uLowRatioBonus & 0xFFFFu) << 8)
		| (rBreakdown.uLowIdDivisor & 0xFFu);
}

inline CString FormatUploadScoreModifiers(const UploadScoreBreakdown &rBreakdown, LPCTSTR pszLowRatioLabel, LPCTSTR pszLowIdLabel, LPCTSTR pszCooldownLabel)
{
	if (rBreakdown.eAvailability == uploadScoreFriendSlot || rBreakdown.eAvailability == uploadScoreUnavailable)
		return CString(_T("-"));

	CString strModifiers;
	if (rBreakdown.eAvailability == uploadScoreCooldown)
		strModifiers = pszCooldownLabel;

	if (rBreakdown.bLowRatioApplied) {
		CString strPart;
		strPart.Format(_T("%s +%u"), pszLowRatioLabel, rBreakdown.uLowRatioBonus);
		if (!strModifiers.IsEmpty())
			strModifiers += _T(", ");
		strModifiers += strPart;
	}

	if (rBreakdown.bLowIdPenaltyApplied) {
		CString strPart;
		strPart.Format(_T("%s /%u"), pszLowIdLabel, rBreakdown.uLowIdDivisor);
		if (!strModifiers.IsEmpty())
			strModifiers += _T(", ");
		strModifiers += strPart;
	}

	return strModifiers.IsEmpty() ? CString(_T("-")) : strModifiers;
}

inline CString FormatBaseUploadScore(const UploadScoreBreakdown &rBreakdown, LPCTSTR pszFriendSlotLabel, LPCTSTR pszUnavailableLabel)
{
	switch (rBreakdown.eAvailability) {
	case uploadScoreFriendSlot:
		return pszFriendSlotLabel;
	case uploadScoreUnavailable:
		return pszUnavailableLabel;
	default:
		break;
	}

	CString strScore;
	strScore.Format(_T("%u"), rBreakdown.uBaseScore);
	return strScore;
}

inline CString FormatEffectiveUploadScoreValue(const UploadScoreBreakdown &rBreakdown, LPCTSTR pszFriendSlotLabel, LPCTSTR pszUnavailableLabel)
{
	switch (rBreakdown.eAvailability) {
	case uploadScoreFriendSlot:
		return pszFriendSlotLabel;
	case uploadScoreUnavailable:
		return pszUnavailableLabel;
	default:
		break;
	}

	CString strScore;
	strScore.Format(_T("%u"), rBreakdown.uEffectiveScore);
	return strScore;
}

inline CString FormatUploadScoreCompact(const UploadScoreBreakdown &rBreakdown, LPCTSTR pszLowRatioLabel, LPCTSTR pszLowIdLabel, LPCTSTR pszCooldownLabel, LPCTSTR pszFriendSlotLabel, LPCTSTR pszUnavailableLabel)
{
	CString strScore = FormatEffectiveUploadScoreValue(rBreakdown, pszFriendSlotLabel, pszUnavailableLabel);
	const CString strModifiers = FormatUploadScoreModifiers(rBreakdown, pszLowRatioLabel, pszLowIdLabel, pszCooldownLabel);
	if (strModifiers != _T("-"))
		strScore.AppendFormat(_T(" (%s)"), (LPCTSTR)strModifiers);
	return strScore;
}

inline CString FormatEffectiveUploadScore(const UploadScoreBreakdown &rBreakdown, LPCTSTR pszLowRatioLabel, LPCTSTR pszLowIdLabel, LPCTSTR pszCooldownLabel, LPCTSTR pszFriendSlotLabel, LPCTSTR pszUnavailableLabel)
{
	return FormatUploadScoreCompact(rBreakdown, pszLowRatioLabel, pszLowIdLabel, pszCooldownLabel, pszFriendSlotLabel, pszUnavailableLabel);
}

inline CString FormatUploadScoreLowRatioDetail(const UploadScoreBreakdown &rBreakdown, LPCTSTR pszInactiveLabel)
{
	if (!rBreakdown.bLowRatioApplied)
		return pszInactiveLabel;

	CString strValue;
	strValue.Format(_T("+%u"), rBreakdown.uLowRatioBonus);
	return strValue;
}

inline CString FormatUploadScoreLowIdDetail(const UploadScoreBreakdown &rBreakdown, LPCTSTR pszInactiveLabel)
{
	if (!rBreakdown.bLowIdPenaltyApplied)
		return pszInactiveLabel;

	CString strValue;
	strValue.Format(_T("/%u"), rBreakdown.uLowIdDivisor);
	return strValue;
}

inline CString FormatUploadScoreCooldownDetail(const UploadScoreBreakdown &rBreakdown, std::uint64_t uRemainingMs, LPCTSTR pszInactiveLabel)
{
	if (rBreakdown.eAvailability == uploadScoreUnavailable || rBreakdown.eAvailability == uploadScoreFriendSlot || uRemainingMs == 0)
		return pszInactiveLabel;

	CString strValue;
	strValue.Format(_T("%us"), static_cast<UINT>((uRemainingMs + 999u) / 1000u));
	return strValue;
}
}
