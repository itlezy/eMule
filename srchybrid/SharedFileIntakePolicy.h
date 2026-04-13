#pragma once

#include <atlstr.h>
#include <tchar.h>
#include <vector>

namespace SharedFileIntakePolicy
{
enum RuleMatchKind
{
	RuleMatchExact,
	RuleMatchPrefix,
	RuleMatchSuffix
};

struct IgnoreRule
{
	RuleMatchKind eMatchKind;
	CString strPattern;
};

inline const std::vector<IgnoreRule> &GetUserRules();
inline void ReplaceUserRules(const std::vector<IgnoreRule> &rRules);

class ScopedUserRuleOverride
{
public:
	ScopedUserRuleOverride()
		: m_savedRules(GetUserRules())
	{
	}

	~ScopedUserRuleOverride()
	{
		ReplaceUserRules(m_savedRules);
	}

private:
	std::vector<IgnoreRule> m_savedRules;
};

inline bool MatchesPrefixNoCase(const CString &rstrValue, const CString &rstrPrefix)
{
	return rstrValue.GetLength() >= rstrPrefix.GetLength()
		&& rstrValue.Left(rstrPrefix.GetLength()).CompareNoCase(rstrPrefix) == 0;
}

inline bool MatchesSuffixNoCase(const CString &rstrValue, const CString &rstrSuffix)
{
	return rstrValue.GetLength() >= rstrSuffix.GetLength()
		&& rstrValue.Right(rstrSuffix.GetLength()).CompareNoCase(rstrSuffix) == 0;
}

inline CString GetLeafName(const CString &rstrPath)
{
	if (rstrPath.IsEmpty())
		return CString();

	int iEnd = rstrPath.GetLength();
	while (iEnd > 0 && (rstrPath[iEnd - 1] == _T('\\') || rstrPath[iEnd - 1] == _T('/')))
		--iEnd;

	int iLeafStart = iEnd;
	while (iLeafStart > 0 && rstrPath[iLeafStart - 1] != _T('\\') && rstrPath[iLeafStart - 1] != _T('/'))
		--iLeafStart;

	return rstrPath.Mid(iLeafStart, iEnd - iLeafStart);
}

inline bool TryParseUserRule(const CString &rstrLine, IgnoreRule &rRule)
{
	if (rstrLine.IsEmpty())
		return false;

	const int iFirstWildcard = rstrLine.Find(_T('*'));
	if (iFirstWildcard < 0) {
		rRule.eMatchKind = RuleMatchExact;
		rRule.strPattern = rstrLine;
		return !rRule.strPattern.IsEmpty();
	}

	if (rstrLine.Find(_T('*'), iFirstWildcard + 1) >= 0)
		return false;

	if (rstrLine.GetLength() == 1)
		return false;

	if (iFirstWildcard == 0) {
		rRule.eMatchKind = RuleMatchSuffix;
		rRule.strPattern = rstrLine.Mid(1);
		return !rRule.strPattern.IsEmpty();
	}

	if (iFirstWildcard == rstrLine.GetLength() - 1) {
		rRule.eMatchKind = RuleMatchPrefix;
		rRule.strPattern = rstrLine.Left(rstrLine.GetLength() - 1);
		return !rRule.strPattern.IsEmpty();
	}

	return false;
}

inline bool MatchesRule(const IgnoreRule &rRule, const CString &rstrLeafName)
{
	switch (rRule.eMatchKind) {
	case RuleMatchExact:
		return rstrLeafName.CompareNoCase(rRule.strPattern) == 0;
	case RuleMatchPrefix:
		return MatchesPrefixNoCase(rstrLeafName, rRule.strPattern);
	case RuleMatchSuffix:
		return MatchesSuffixNoCase(rstrLeafName, rRule.strPattern);
	default:
		return false;
	}
}

inline bool MatchesAnyRule(const CString &rstrLeafName, const std::vector<IgnoreRule> &rRules)
{
	for (size_t i = 0; i < rRules.size(); ++i) {
		if (MatchesRule(rRules[i], rstrLeafName))
			return true;
	}
	return false;
}

inline bool MatchesAffixNoCase(const CString &rstrValue, LPCTSTR pszPrefix, LPCTSTR pszSuffix)
{
	const CString strPrefix(pszPrefix);
	const CString strSuffix(pszSuffix);
	return MatchesPrefixNoCase(rstrValue, strPrefix)
		&& MatchesSuffixNoCase(rstrValue, strSuffix)
		&& rstrValue.GetLength() >= strPrefix.GetLength() + strSuffix.GetLength();
}

inline std::vector<IgnoreRule> &MutableUserRules()
{
	static std::vector<IgnoreRule> s_userRules;
	return s_userRules;
}

inline const std::vector<IgnoreRule> &GetUserRules()
{
	return MutableUserRules();
}

inline void ReplaceUserRules(const std::vector<IgnoreRule> &rRules)
{
	MutableUserRules() = rRules;
}

inline void ClearUserRules()
{
	MutableUserRules().clear();
}

inline bool ShouldIgnoreFileByName(const CString &rstrFileName)
{
	if (rstrFileName.IsEmpty())
		return false;

	static const LPCTSTR s_apszIgnoredExactNames[] = {
		_T("ehthumbs.db"),
		_T("desktop.ini"),
		_T(".ds_store"),
		_T(".localized"),
		_T("Icon\r"),
		_T(".directory")
	};
	static const LPCTSTR s_apszIgnoredPrefixes[] = {
		_T("._"),
		_T("~$"),
		_T(".nfs"),
		_T(".sb-"),
		_T(".syncthing.")
	};
	static const LPCTSTR s_apszIgnoredSuffixes[] = {
		_T(".lnk"),
		_T(".part"),
		_T(".crdownload"),
		_T(".download"),
		_T(".tmp"),
		_T(".temp"),
		_T("~")
	};

	for (size_t i = 0; i < _countof(s_apszIgnoredExactNames); ++i) {
		if (rstrFileName.CompareNoCase(s_apszIgnoredExactNames[i]) == 0)
			return true;
	}
	for (size_t i = 0; i < _countof(s_apszIgnoredPrefixes); ++i) {
		if (MatchesPrefixNoCase(rstrFileName, s_apszIgnoredPrefixes[i]))
			return true;
	}
	for (size_t i = 0; i < _countof(s_apszIgnoredSuffixes); ++i) {
		if (MatchesSuffixNoCase(rstrFileName, s_apszIgnoredSuffixes[i]))
			return true;
	}
	if (MatchesAffixNoCase(rstrFileName, _T("~lock."), _T("#")))
		return true;

	return MatchesAnyRule(rstrFileName, GetUserRules());
}

inline bool ShouldIgnoreDirectoryByName(const CString &rstrDirectoryName)
{
	if (rstrDirectoryName.IsEmpty())
		return false;

	static const LPCTSTR s_apszIgnoredExactNames[] = {
		_T(".fseventsd"),
		_T(".spotlight-v100"),
		_T(".temporaryitems"),
		_T(".trashes"),
		_T(".git"),
		_T(".svn"),
		_T(".hg"),
		_T("CVS")
	};
	static const LPCTSTR s_apszIgnoredPrefixes[] = {
		_T("._"),
		_T(".nfs"),
		_T(".sb-"),
		_T(".syncthing.")
	};

	for (size_t i = 0; i < _countof(s_apszIgnoredExactNames); ++i) {
		if (rstrDirectoryName.CompareNoCase(s_apszIgnoredExactNames[i]) == 0)
			return true;
	}
	for (size_t i = 0; i < _countof(s_apszIgnoredPrefixes); ++i) {
		if (MatchesPrefixNoCase(rstrDirectoryName, s_apszIgnoredPrefixes[i]))
			return true;
	}

	return MatchesAnyRule(rstrDirectoryName, GetUserRules());
}

inline bool ShouldIgnoreDirectoryPath(const CString &rstrDirectoryPath)
{
	return ShouldIgnoreDirectoryByName(GetLeafName(rstrDirectoryPath));
}

inline bool ShouldIgnoreByName(const CString &rstrFileName)
{
	return ShouldIgnoreFileByName(rstrFileName);
}

template <typename IsThumbsDbFn>
inline bool ShouldIgnoreCandidate(const CString &rstrFilePath, const CString &rstrFileName, IsThumbsDbFn isThumbsDbFn)
{
	return ShouldIgnoreFileByName(rstrFileName) || isThumbsDbFn(rstrFilePath, rstrFileName);
}
}

bool ShouldIgnoreSharedFileCandidate(const CString &sFilePath, const CString &sFileName);
