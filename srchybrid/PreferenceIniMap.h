#pragma once

#include <tchar.h>

/**
 * Documents the non-stock preferences.ini sections and key names owned by the
 * broadband branch. Stock/community keys intentionally remain at their
 * established call sites.
 */
namespace PreferenceIniMap
{
namespace Sections
{
inline constexpr const TCHAR* FileCompletion = _T("FileCompletion");
inline constexpr const TCHAR* UploadPolicy = _T("UploadPolicy");
inline constexpr const TCHAR* eMule = _T("eMule");
}

namespace FileCompletionKeys
{
inline constexpr const TCHAR* RunCommandOnFileCompletion = _T("RunCommandOnFileCompletion");
inline constexpr const TCHAR* Program = _T("FileCompletionProgram");
inline constexpr const TCHAR* Arguments = _T("FileCompletionArguments");
}

namespace UploadPolicyKeys
{
inline constexpr const TCHAR* MaxUploadClientsAllowed = _T("MaxUploadClientsAllowed");
inline constexpr const TCHAR* SlowUploadThresholdFactor = _T("SlowUploadThresholdFactor");
inline constexpr const TCHAR* SlowUploadGraceSeconds = _T("SlowUploadGraceSeconds");
inline constexpr const TCHAR* SlowUploadWarmupSeconds = _T("SlowUploadWarmupSeconds");
inline constexpr const TCHAR* ZeroUploadRateGraceSeconds = _T("ZeroUploadRateGraceSeconds");
inline constexpr const TCHAR* SlowUploadCooldownSeconds = _T("SlowUploadCooldownSeconds");
inline constexpr const TCHAR* LowRatioBoostEnabled = _T("LowRatioBoostEnabled");
inline constexpr const TCHAR* LowRatioThreshold = _T("LowRatioThreshold");
inline constexpr const TCHAR* LowRatioScoreBonus = _T("LowRatioScoreBonus");
inline constexpr const TCHAR* LowIDScoreDivisor = _T("LowIDScoreDivisor");
inline constexpr const TCHAR* SessionTransferLimitMode = _T("SessionTransferLimitMode");
inline constexpr const TCHAR* SessionTransferLimitValue = _T("SessionTransferLimitValue");
inline constexpr const TCHAR* SessionTimeLimitSeconds = _T("SessionTimeLimitSeconds");
}
}
