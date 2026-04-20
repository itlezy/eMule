#pragma once

#include <atlstr.h>
#include <vector>
#include <windows.h>

#include "LongPathSeams.h"
#include "PathHelpers.h"

namespace Ini2Helpers
{
template <typename CStringType, typename ReadFn>
inline CStringType ReadProfileStringDynamic(ReadFn readFn)
{
	DWORD dwCapacity = 256u;
	CStringType strValue;
	for (;;) {
		const DWORD dwCopied = readFn(strValue.GetBuffer(dwCapacity), dwCapacity);
		if (dwCopied < dwCapacity - 1 || dwCapacity >= PathHelpers::kMaxDynamicPathChars) {
			strValue.ReleaseBuffer(static_cast<int>(dwCopied < (dwCapacity - 1) ? dwCopied : (dwCapacity - 1)));
			return strValue;
		}

		strValue.ReleaseBuffer(dwCapacity - 1);
		const DWORD dwNextCapacity = dwCapacity * 2u;
		dwCapacity = (dwNextCapacity < PathHelpers::kMaxDynamicPathChars)
			? dwNextCapacity
			: static_cast<DWORD>(PathHelpers::kMaxDynamicPathChars);
	}
}

inline bool NeedsBaseDirectoryPrefix(const CString &rstrFileName)
{
	if (rstrFileName.IsEmpty())
		return true;

	if (rstrFileName.GetLength() >= 2 && rstrFileName[1] == _T(':'))
		return false;
	if (rstrFileName.GetLength() >= 2
		&& (rstrFileName[0] == _T('\\') || rstrFileName[0] == _T('/'))
		&& (rstrFileName[1] == _T('\\') || rstrFileName[1] == _T('/')))
		return false;
	if (rstrFileName[0] == _T('\\') || rstrFileName[0] == _T('/'))
		return false;
	if (rstrFileName.Left(4).CompareNoCase(_T("\\\\?\\")) == 0)
		return false;
	return true;
}

inline CString ExtractBaseNameWithoutExtension(const CString &rstrPath)
{
	int iSeparator = rstrPath.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrPath.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;

	const int iNameStart = iSeparator + 1;
	int iNameEnd = rstrPath.GetLength();
	const int iDot = rstrPath.ReverseFind(_T('.'));
	if (iDot > iSeparator)
		iNameEnd = iDot;
	return rstrPath.Mid(iNameStart, iNameEnd - iNameStart);
}

inline CString BuildPathFromBaseDirectory(const CString &rstrBaseDirectory, const CString &rstrRelativePath)
{
	if (rstrBaseDirectory.IsEmpty())
		return rstrRelativePath;
	return PathHelpers::AppendPathComponent(rstrBaseDirectory, rstrRelativePath);
}

inline CString BuildDefaultIniFilePath(const CString &rstrModulePath, const CString &rstrCurrentDirectory, const bool bModulePath)
{
	if (rstrModulePath.IsEmpty())
		return CString();
	const CString strIniFileName = ExtractBaseNameWithoutExtension(rstrModulePath) + _T(".ini");
	return bModulePath
		? BuildPathFromBaseDirectory(PathHelpers::GetDirectoryPath(rstrModulePath), strIniFileName)
		: BuildPathFromBaseDirectory(rstrCurrentDirectory, strIniFileName);
}

inline CString GetModuleFilePath()
{
	return PathHelpers::GetModuleFilePath(NULL);
}

inline CString GetCurrentDirectoryPath()
{
	return PathHelpers::GetCurrentDirectoryPath();
}

inline bool RequiresFileBackedProfileIo(const CString &rstrProfilePath)
{
	return !PathHelpers::IsShellSafePath(PathHelpers::StripExtendedLengthPrefix(rstrProfilePath));
}

/**
 * @brief Returns whether the logical profile path can be losslessly represented through the ANSI profile APIs.
 */
inline bool CanRoundTripAnsiProfilePath(const CString &rstrProfilePath)
{
#ifdef _UNICODE
	const CString strLogicalPath(PathHelpers::StripExtendedLengthPrefix(rstrProfilePath));
	if (strLogicalPath.IsEmpty())
		return true;

	BOOL bUsedDefaultChar = FALSE;
	const int iRequiredAnsiChars = ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, strLogicalPath, -1, NULL, 0, NULL, &bUsedDefaultChar);
	if (iRequiredAnsiChars <= 0 || bUsedDefaultChar != FALSE)
		return false;

	std::vector<char> ansiBuffer(static_cast<size_t>(iRequiredAnsiChars), '\0');
	if (::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, strLogicalPath, -1, &ansiBuffer[0], iRequiredAnsiChars, NULL, &bUsedDefaultChar) != iRequiredAnsiChars
		|| bUsedDefaultChar != FALSE)
	{
		return false;
	}

	const int iRequiredWideChars = ::MultiByteToWideChar(CP_ACP, 0, &ansiBuffer[0], -1, NULL, 0);
	if (iRequiredWideChars <= 0)
		return false;

	CString strRoundTrip;
	LPWSTR pszRoundTrip = strRoundTrip.GetBuffer(iRequiredWideChars - 1);
	const int iWrittenWideChars = ::MultiByteToWideChar(CP_ACP, 0, &ansiBuffer[0], -1, pszRoundTrip, iRequiredWideChars);
	strRoundTrip.ReleaseBuffer(iWrittenWideChars > 0 ? iWrittenWideChars - 1 : 0);
	return iWrittenWideChars == iRequiredWideChars && strRoundTrip == strLogicalPath;
#else
	return true;
#endif
}

/**
 * @brief Returns whether UTF-8 profile value I/O must bypass the ANSI profile APIs and use raw file access instead.
 */
inline bool RequiresFileBackedProfileUtf8Io(const CString &rstrProfilePath)
{
	return RequiresFileBackedProfileIo(rstrProfilePath) || !CanRoundTripAnsiProfilePath(rstrProfilePath);
}

inline CStringA EncodeUtf8(const CString &rstrValue)
{
#ifdef _UNICODE
	const int iRequiredChars = ::WideCharToMultiByte(CP_UTF8, 0, rstrValue, rstrValue.GetLength(), NULL, 0, NULL, NULL);
	if (iRequiredChars <= 0)
		return CStringA();

	CStringA strUtf8;
	LPSTR pszUtf8 = strUtf8.GetBuffer(iRequiredChars);
	const int iWrittenChars = ::WideCharToMultiByte(CP_UTF8, 0, rstrValue, rstrValue.GetLength(), pszUtf8, iRequiredChars, NULL, NULL);
	strUtf8.ReleaseBuffer(iWrittenChars > 0 ? iWrittenChars : 0);
	return strUtf8;
#else
	return CStringA(rstrValue);
#endif
}

inline CString DecodeUtf8(const CStringA &rstrUtf8)
{
#ifdef _UNICODE
	const int iRequiredChars = ::MultiByteToWideChar(CP_UTF8, 0, rstrUtf8, rstrUtf8.GetLength(), NULL, 0);
	if (iRequiredChars <= 0)
		return CString(rstrUtf8);

	CString strWide;
	LPWSTR pszWide = strWide.GetBuffer(iRequiredChars);
	const int iWrittenChars = ::MultiByteToWideChar(CP_UTF8, 0, rstrUtf8, rstrUtf8.GetLength(), pszWide, iRequiredChars);
	strWide.ReleaseBuffer(iWrittenChars > 0 ? iWrittenChars : 0);
	return strWide;
#else
	return CString(rstrUtf8);
#endif
}

namespace detail
{
struct ProfileLine
{
	CStringA strText;
	CStringA strLineEnding;
};

inline bool IsAsciiSpace(const char ch)
{
	return ch == ' ' || ch == '\t';
}

inline CStringA TrimAsciiWhitespace(const CStringA &rstrValue)
{
	int iStart = 0;
	int iEnd = rstrValue.GetLength();
	while (iStart < iEnd && IsAsciiSpace(rstrValue[iStart]))
		++iStart;
	while (iEnd > iStart && IsAsciiSpace(rstrValue[iEnd - 1]))
		--iEnd;
	return rstrValue.Mid(iStart, iEnd - iStart);
}

inline CStringA NarrowProfileToken(const CString &rstrValue)
{
	return CStringA(rstrValue);
}

inline CStringA BytesToCStringA(const std::vector<unsigned char> &rBytes)
{
	return rBytes.empty()
		? CStringA()
		: CStringA(reinterpret_cast<const char *>(rBytes.data()), static_cast<int>(rBytes.size()));
}

inline std::vector<unsigned char> CStringAToBytes(const CStringA &rstrValue)
{
	if (rstrValue.IsEmpty())
		return std::vector<unsigned char>();

	return std::vector<unsigned char>(
		reinterpret_cast<const unsigned char *>(static_cast<LPCSTR>(rstrValue)),
		reinterpret_cast<const unsigned char *>(static_cast<LPCSTR>(rstrValue)) + rstrValue.GetLength());
}

inline std::vector<ProfileLine> SplitProfileLines(const CStringA &rstrContent)
{
	std::vector<ProfileLine> lines;
	const int nLength = rstrContent.GetLength();
	int iLineStart = 0;
	for (int i = 0; i < nLength; ++i) {
		if (rstrContent[i] != '\r' && rstrContent[i] != '\n')
			continue;

		int iLineEnd = i;
		CStringA strLineEnding;
		if (rstrContent[i] == '\r' && i + 1 < nLength && rstrContent[i + 1] == '\n') {
			strLineEnding = "\r\n";
			++i;
		} else {
			const char chLineEnding = rstrContent[i];
			strLineEnding = CStringA(&chLineEnding, 1);
		}

		ProfileLine line = { rstrContent.Mid(iLineStart, iLineEnd - iLineStart), strLineEnding };
		lines.push_back(line);
		iLineStart = i + 1;
	}

	if (iLineStart < nLength || nLength == 0) {
		ProfileLine line = { rstrContent.Mid(iLineStart), CStringA() };
		lines.push_back(line);
	}

	return lines;
}

inline CStringA ChooseLineEnding(const std::vector<ProfileLine> &rLines)
{
	for (size_t i = 0; i < rLines.size(); ++i) {
		if (!rLines[i].strLineEnding.IsEmpty())
			return rLines[i].strLineEnding;
	}
	return CStringA("\r\n");
}

inline CStringA JoinProfileLines(const std::vector<ProfileLine> &rLines)
{
	CStringA strJoined;
	const CStringA strDefaultLineEnding = ChooseLineEnding(rLines);
	for (size_t i = 0; i < rLines.size(); ++i) {
		strJoined += rLines[i].strText;
		if (!rLines[i].strLineEnding.IsEmpty())
			strJoined += rLines[i].strLineEnding;
		else if (i + 1 < rLines.size())
			strJoined += strDefaultLineEnding;
	}
	return strJoined;
}

inline bool TryParseSectionName(const CStringA &rstrLine, CStringA &rstrSectionName)
{
	const CStringA strTrimmed = TrimAsciiWhitespace(rstrLine);
	if (strTrimmed.GetLength() < 3 || strTrimmed[0] != '[')
		return false;

	const int iCloseBracket = strTrimmed.Find(']');
	if (iCloseBracket <= 1)
		return false;

	rstrSectionName = TrimAsciiWhitespace(strTrimmed.Mid(1, iCloseBracket - 1));
	return !rstrSectionName.IsEmpty();
}

inline bool TryParseKeyValue(const CStringA &rstrLine, CStringA &rstrKeyName, int &riValueStart)
{
	riValueStart = -1;
	int iStart = 0;
	while (iStart < rstrLine.GetLength() && IsAsciiSpace(rstrLine[iStart]))
		++iStart;
	if (iStart >= rstrLine.GetLength() || rstrLine[iStart] == ';' || rstrLine[iStart] == '#')
		return false;

	const int iEquals = rstrLine.Find('=', iStart);
	if (iEquals <= iStart)
		return false;

	rstrKeyName = TrimAsciiWhitespace(rstrLine.Mid(iStart, iEquals - iStart));
	if (rstrKeyName.IsEmpty())
		return false;

	int iValueStart = iEquals + 1;
	while (iValueStart < rstrLine.GetLength() && IsAsciiSpace(rstrLine[iValueStart]))
		++iValueStart;
	riValueStart = iValueStart;
	return true;
}

inline int FindSectionStartIndex(const std::vector<ProfileLine> &rLines, const CStringA &rstrSectionName)
{
	for (size_t i = 0; i < rLines.size(); ++i) {
		CStringA strLineSectionName;
		if (TryParseSectionName(rLines[i].strText, strLineSectionName) && strLineSectionName.CompareNoCase(rstrSectionName) == 0)
			return static_cast<int>(i);
	}
	return -1;
}

inline int FindSectionInsertIndex(const std::vector<ProfileLine> &rLines, const int iSectionStart)
{
	if (iSectionStart < 0)
		return static_cast<int>(rLines.size());

	for (size_t i = static_cast<size_t>(iSectionStart + 1); i < rLines.size(); ++i) {
		CStringA strLineSectionName;
		if (TryParseSectionName(rLines[i].strText, strLineSectionName))
			return static_cast<int>(i);
	}

	return static_cast<int>(rLines.size());
}

inline CStringA ReadProfileValueRaw(const CString &rstrSection, const CString &rstrEntry, const CStringA &rstrDefaultValueRaw, const CString &rstrProfilePath)
{
	std::vector<unsigned char> fileBytes;
	if (!LongPathSeams::ReadAllBytes(rstrProfilePath, fileBytes))
		return rstrDefaultValueRaw;

	const CStringA strSectionName = NarrowProfileToken(rstrSection);
	const CStringA strEntryName = NarrowProfileToken(rstrEntry);
	const std::vector<ProfileLine> lines = SplitProfileLines(BytesToCStringA(fileBytes));
	bool bInTargetSection = false;
	for (size_t i = 0; i < lines.size(); ++i) {
		CStringA strLineSectionName;
		if (TryParseSectionName(lines[i].strText, strLineSectionName)) {
			bInTargetSection = (strLineSectionName.CompareNoCase(strSectionName) == 0);
			continue;
		}
		if (!bInTargetSection)
			continue;

		CStringA strLineEntryName;
		int iValueStart = -1;
		if (TryParseKeyValue(lines[i].strText, strLineEntryName, iValueStart)
			&& strLineEntryName.CompareNoCase(strEntryName) == 0)
		{
			return lines[i].strText.Mid(iValueStart);
		}
	}

	return rstrDefaultValueRaw;
}

inline CString DecodeProfileValueRaw(const CStringA &rstrValueRaw)
{
	return CString(rstrValueRaw);
}

inline CString DecodeProfileUtf8ValueRaw(const CStringA &rstrValueRaw)
{
	return DecodeUtf8(rstrValueRaw);
}

inline bool PersistProfileLines(const CString &rstrProfilePath, const std::vector<ProfileLine> &rLines)
{
	const CStringA strPersisted = JoinProfileLines(rLines);
	return LongPathSeams::WriteAllBytes(rstrProfilePath, CStringAToBytes(strPersisted));
}

inline void EnsureWritableLineEnding(ProfileLine &rLine, const CStringA &rstrLineEnding)
{
	if (rLine.strLineEnding.IsEmpty())
		rLine.strLineEnding = rstrLineEnding;
}

inline bool UpsertProfileValueRaw(const CString &rstrSection, const CString &rstrEntry, const CStringA &rstrValueRaw, const CString &rstrProfilePath)
{
	std::vector<unsigned char> fileBytes;
	const bool bLoaded = LongPathSeams::ReadAllBytes(rstrProfilePath, fileBytes);
	if (!bLoaded) {
		const DWORD dwLastError = ::GetLastError();
		if (dwLastError != ERROR_FILE_NOT_FOUND && dwLastError != ERROR_PATH_NOT_FOUND)
			return false;
	}

	std::vector<ProfileLine> lines = SplitProfileLines(BytesToCStringA(fileBytes));
	const CStringA strLineEnding = ChooseLineEnding(lines);
	const CStringA strSectionName = NarrowProfileToken(rstrSection);
	const CStringA strEntryName = NarrowProfileToken(rstrEntry);
	const CStringA strNewLine = strEntryName + "=" + rstrValueRaw;
	const int iSectionStart = FindSectionStartIndex(lines, strSectionName);
	if (iSectionStart >= 0) {
		const int iSectionEnd = FindSectionInsertIndex(lines, iSectionStart);
		for (int i = iSectionStart + 1; i < iSectionEnd; ++i) {
			CStringA strLineEntryName;
			int iValueStart = -1;
			if (TryParseKeyValue(lines[i].strText, strLineEntryName, iValueStart)
				&& strLineEntryName.CompareNoCase(strEntryName) == 0)
			{
				lines[i].strText = strNewLine;
				EnsureWritableLineEnding(lines[i], strLineEnding);
				return PersistProfileLines(rstrProfilePath, lines);
			}
		}

		ProfileLine line = { strNewLine, strLineEnding };
		lines.insert(lines.begin() + iSectionEnd, line);
		return PersistProfileLines(rstrProfilePath, lines);
	}

	if (!lines.empty() && !lines.back().strText.IsEmpty())
		lines.push_back(ProfileLine{ CStringA(), strLineEnding });
	lines.push_back(ProfileLine{ CStringA("[") + strSectionName + "]", strLineEnding });
	lines.push_back(ProfileLine{ strNewLine, CStringA() });
	return PersistProfileLines(rstrProfilePath, lines);
}

inline bool DeleteProfileKeyRaw(const CString &rstrSection, const CString &rstrEntry, const CString &rstrProfilePath)
{
	std::vector<unsigned char> fileBytes;
	if (!LongPathSeams::ReadAllBytes(rstrProfilePath, fileBytes)) {
		const DWORD dwLastError = ::GetLastError();
		return dwLastError == ERROR_FILE_NOT_FOUND || dwLastError == ERROR_PATH_NOT_FOUND;
	}

	std::vector<ProfileLine> lines = SplitProfileLines(BytesToCStringA(fileBytes));
	const CStringA strSectionName = NarrowProfileToken(rstrSection);
	const CStringA strEntryName = NarrowProfileToken(rstrEntry);
	const int iSectionStart = FindSectionStartIndex(lines, strSectionName);
	if (iSectionStart < 0)
		return true;

	const int iSectionEnd = FindSectionInsertIndex(lines, iSectionStart);
	bool bRemoved = false;
	for (int i = iSectionEnd - 1; i > iSectionStart; --i) {
		CStringA strLineEntryName;
		int iValueStart = -1;
		if (TryParseKeyValue(lines[i].strText, strLineEntryName, iValueStart)
			&& strLineEntryName.CompareNoCase(strEntryName) == 0)
		{
			lines.erase(lines.begin() + i);
			bRemoved = true;
		}
	}

	return !bRemoved || PersistProfileLines(rstrProfilePath, lines);
}
}

inline CString ReadProfileStringLongPath(const CString &rstrSection, const CString &rstrEntry, LPCTSTR pszDefaultValue, const CString &rstrProfilePath)
{
	return detail::DecodeProfileValueRaw(detail::ReadProfileValueRaw(rstrSection, rstrEntry, CStringA(pszDefaultValue != NULL ? pszDefaultValue : _T("")), rstrProfilePath));
}

inline CString ReadProfileUtf8StringLongPath(const CString &rstrSection, const CString &rstrEntry, LPCTSTR pszDefaultValue, const CString &rstrProfilePath)
{
	const CStringA strDefaultValueRaw = pszDefaultValue != NULL ? EncodeUtf8(CString(pszDefaultValue)) : CStringA();
	return detail::DecodeProfileUtf8ValueRaw(detail::ReadProfileValueRaw(rstrSection, rstrEntry, strDefaultValueRaw, rstrProfilePath));
}

inline bool WriteProfileStringLongPath(const CString &rstrSection, const CString &rstrEntry, LPCTSTR pszValue, const CString &rstrProfilePath)
{
	return detail::UpsertProfileValueRaw(rstrSection, rstrEntry, CStringA(pszValue != NULL ? pszValue : _T("")), rstrProfilePath);
}

inline bool WriteProfileUtf8StringLongPath(const CString &rstrSection, const CString &rstrEntry, LPCTSTR pszValue, const CString &rstrProfilePath)
{
	return detail::UpsertProfileValueRaw(rstrSection, rstrEntry, EncodeUtf8(CString(pszValue != NULL ? pszValue : _T(""))), rstrProfilePath);
}

inline bool DeleteProfileKeyLongPath(const CString &rstrSection, const CString &rstrEntry, const CString &rstrProfilePath)
{
	return detail::DeleteProfileKeyRaw(rstrSection, rstrEntry, rstrProfilePath);
}
}
