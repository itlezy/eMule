#pragma once

#include <atlstr.h>
#include <cstring>
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

namespace detail
{
enum class ProfileTextEncoding
{
	Empty,
	Ansi,
	Utf16Le,
	Unsupported
};

struct ProfileLineW
{
	CStringW strText;
	CStringW strLineEnding;
};

inline bool HasUtf16LeBom(const std::vector<unsigned char> &rBytes)
{
	return rBytes.size() >= 2 && rBytes[0] == 0xFFu && rBytes[1] == 0xFEu;
}

inline bool HasUtf16BeBom(const std::vector<unsigned char> &rBytes)
{
	return rBytes.size() >= 2 && rBytes[0] == 0xFEu && rBytes[1] == 0xFFu;
}

inline bool HasUtf8Bom(const std::vector<unsigned char> &rBytes)
{
	return rBytes.size() >= 3 && rBytes[0] == 0xEFu && rBytes[1] == 0xBBu && rBytes[2] == 0xBFu;
}

inline bool ContainsNulByte(const std::vector<unsigned char> &rBytes)
{
	for (size_t i = 0; i < rBytes.size(); ++i)
		if (rBytes[i] == 0)
			return true;
	return false;
}

inline ProfileTextEncoding DetectProfileTextEncoding(const std::vector<unsigned char> &rBytes)
{
	if (rBytes.empty())
		return ProfileTextEncoding::Empty;
	if (HasUtf16LeBom(rBytes))
		return ProfileTextEncoding::Utf16Le;
	if (HasUtf16BeBom(rBytes) || HasUtf8Bom(rBytes) || ContainsNulByte(rBytes))
		return ProfileTextEncoding::Unsupported;
	return ProfileTextEncoding::Ansi;
}

inline CStringW DecodeUtf16LeProfileText(const std::vector<unsigned char> &rBytes)
{
	if (!HasUtf16LeBom(rBytes))
		return CStringW();

	const size_t nPayloadBytes = rBytes.size() - 2u;
	const int iWideChars = static_cast<int>(nPayloadBytes / sizeof(wchar_t));
	return iWideChars > 0
		? CStringW(reinterpret_cast<const wchar_t *>(rBytes.data() + 2u), iWideChars)
		: CStringW();
}

inline std::vector<unsigned char> EncodeUtf16LeProfileText(const CStringW &rstrText)
{
	std::vector<unsigned char> bytes;
	bytes.reserve(2u + static_cast<size_t>(rstrText.GetLength()) * sizeof(wchar_t));
	bytes.push_back(0xFFu);
	bytes.push_back(0xFEu);
	const unsigned char *pTextBytes = reinterpret_cast<const unsigned char *>(static_cast<LPCWSTR>(rstrText));
	bytes.insert(bytes.end(), pTextBytes, pTextBytes + static_cast<size_t>(rstrText.GetLength()) * sizeof(wchar_t));
	return bytes;
}

inline bool TryDecodeAnsiExact(const CStringA &rstrAnsi, CStringW &rstrWide)
{
	rstrWide.Empty();
	if (rstrAnsi.IsEmpty())
		return true;

	const int iRequiredWideChars = ::MultiByteToWideChar(CP_ACP, 0, rstrAnsi, rstrAnsi.GetLength(), NULL, 0);
	if (iRequiredWideChars <= 0)
		return false;

	LPWSTR pszWide = rstrWide.GetBuffer(iRequiredWideChars);
	const int iWrittenWideChars = ::MultiByteToWideChar(CP_ACP, 0, rstrAnsi, rstrAnsi.GetLength(), pszWide, iRequiredWideChars);
	rstrWide.ReleaseBuffer(iWrittenWideChars > 0 ? iWrittenWideChars : 0);
	if (iWrittenWideChars != iRequiredWideChars)
		return false;

	BOOL bUsedDefaultChar = FALSE;
	const int iRequiredAnsiChars = ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, rstrWide, rstrWide.GetLength(), NULL, 0, NULL, &bUsedDefaultChar);
	if (iRequiredAnsiChars != rstrAnsi.GetLength() || bUsedDefaultChar != FALSE)
		return false;

	CStringA strRoundTrip;
	LPSTR pszRoundTrip = strRoundTrip.GetBuffer(iRequiredAnsiChars);
	const int iWrittenAnsiChars = ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, rstrWide, rstrWide.GetLength(), pszRoundTrip, iRequiredAnsiChars, NULL, &bUsedDefaultChar);
	strRoundTrip.ReleaseBuffer(iWrittenAnsiChars > 0 ? iWrittenAnsiChars : 0);
	return iWrittenAnsiChars == iRequiredAnsiChars && bUsedDefaultChar == FALSE && strRoundTrip == rstrAnsi;
}

inline bool TryDecodeUtf8Strict(const CStringA &rstrUtf8, CStringW &rstrWide)
{
	rstrWide.Empty();
	if (rstrUtf8.IsEmpty())
		return true;

	const int iRequiredWideChars = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, rstrUtf8, rstrUtf8.GetLength(), NULL, 0);
	if (iRequiredWideChars <= 0)
		return false;

	LPWSTR pszWide = rstrWide.GetBuffer(iRequiredWideChars);
	const int iWrittenWideChars = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, rstrUtf8, rstrUtf8.GetLength(), pszWide, iRequiredWideChars);
	rstrWide.ReleaseBuffer(iWrittenWideChars > 0 ? iWrittenWideChars : 0);
	return iWrittenWideChars == iRequiredWideChars;
}

inline bool ContainsNonAsciiByte(const CStringA &rstrValue)
{
	for (int i = 0; i < rstrValue.GetLength(); ++i)
		if (static_cast<unsigned char>(rstrValue[i]) >= 0x80u)
			return true;
	return false;
}

inline bool IsKnownUtf8SalvageValue(const CStringA &rstrSectionName, const CStringA &rstrEntryName)
{
	return rstrSectionName.CompareNoCase("eMule") == 0 && rstrEntryName.CompareNoCase("Nick") == 0;
}

inline bool IsWideSpace(const wchar_t ch)
{
	return ch == L' ' || ch == L'\t';
}

inline CStringW TrimWideWhitespace(const CStringW &rstrValue)
{
	int iStart = 0;
	int iEnd = rstrValue.GetLength();
	while (iStart < iEnd && IsWideSpace(rstrValue[iStart]))
		++iStart;
	while (iEnd > iStart && IsWideSpace(rstrValue[iEnd - 1]))
		--iEnd;
	return rstrValue.Mid(iStart, iEnd - iStart);
}

inline std::vector<ProfileLineW> SplitWideProfileLines(const CStringW &rstrContent)
{
	std::vector<ProfileLineW> lines;
	const int nLength = rstrContent.GetLength();
	int iLineStart = 0;
	for (int i = 0; i < nLength; ++i) {
		if (rstrContent[i] != L'\r' && rstrContent[i] != L'\n')
			continue;

		const int iLineEnd = i;
		CStringW strLineEnding;
		if (rstrContent[i] == L'\r' && i + 1 < nLength && rstrContent[i + 1] == L'\n') {
			strLineEnding = L"\r\n";
			++i;
		} else {
			const wchar_t chLineEnding = rstrContent[i];
			strLineEnding = CStringW(&chLineEnding, 1);
		}

		ProfileLineW line = { rstrContent.Mid(iLineStart, iLineEnd - iLineStart), strLineEnding };
		lines.push_back(line);
		iLineStart = i + 1;
	}

	if (iLineStart < nLength) {
		ProfileLineW line = { rstrContent.Mid(iLineStart), CStringW() };
		lines.push_back(line);
	}

	return lines;
}

inline CStringW ChooseWideLineEnding(const std::vector<ProfileLineW> &rLines)
{
	for (size_t i = 0; i < rLines.size(); ++i) {
		if (!rLines[i].strLineEnding.IsEmpty())
			return rLines[i].strLineEnding;
	}
	return CStringW(L"\r\n");
}

inline CStringW JoinWideProfileLines(const std::vector<ProfileLineW> &rLines)
{
	CStringW strJoined;
	const CStringW strDefaultLineEnding = ChooseWideLineEnding(rLines);
	for (size_t i = 0; i < rLines.size(); ++i) {
		strJoined += rLines[i].strText;
		if (!rLines[i].strLineEnding.IsEmpty())
			strJoined += rLines[i].strLineEnding;
		else if (i + 1 < rLines.size())
			strJoined += strDefaultLineEnding;
	}
	return strJoined;
}

inline bool TryParseWideSectionName(const CStringW &rstrLine, CStringW &rstrSectionName)
{
	const CStringW strTrimmed = TrimWideWhitespace(rstrLine);
	if (strTrimmed.GetLength() < 3 || strTrimmed[0] != L'[')
		return false;

	const int iCloseBracket = strTrimmed.Find(L']');
	if (iCloseBracket <= 1)
		return false;

	rstrSectionName = TrimWideWhitespace(strTrimmed.Mid(1, iCloseBracket - 1));
	return !rstrSectionName.IsEmpty();
}

inline bool TryParseWideKeyValue(const CStringW &rstrLine, CStringW &rstrKeyName, int &riValueStart)
{
	riValueStart = -1;
	int iStart = 0;
	while (iStart < rstrLine.GetLength() && IsWideSpace(rstrLine[iStart]))
		++iStart;
	if (iStart >= rstrLine.GetLength() || rstrLine[iStart] == L';' || rstrLine[iStart] == L'#')
		return false;

	const int iEquals = rstrLine.Find(L'=', iStart);
	if (iEquals <= iStart)
		return false;

	rstrKeyName = TrimWideWhitespace(rstrLine.Mid(iStart, iEquals - iStart));
	if (rstrKeyName.IsEmpty())
		return false;

	int iValueStart = iEquals + 1;
	while (iValueStart < rstrLine.GetLength() && IsWideSpace(rstrLine[iValueStart]))
		++iValueStart;
	riValueStart = iValueStart;
	return true;
}

inline int FindWideSectionStartIndex(const std::vector<ProfileLineW> &rLines, const CStringW &rstrSectionName)
{
	for (size_t i = 0; i < rLines.size(); ++i) {
		CStringW strLineSectionName;
		if (TryParseWideSectionName(rLines[i].strText, strLineSectionName) && strLineSectionName.CompareNoCase(rstrSectionName) == 0)
			return static_cast<int>(i);
	}
	return -1;
}

inline int FindWideSectionInsertIndex(const std::vector<ProfileLineW> &rLines, const int iSectionStart)
{
	if (iSectionStart < 0)
		return static_cast<int>(rLines.size());

	for (size_t i = static_cast<size_t>(iSectionStart + 1); i < rLines.size(); ++i) {
		CStringW strLineSectionName;
		if (TryParseWideSectionName(rLines[i].strText, strLineSectionName))
			return static_cast<int>(i);
	}

	return static_cast<int>(rLines.size());
}

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

inline bool DecodeLegacyProfileValue(const CStringA &rstrSectionName, const CStringA &rstrEntryName, const CStringA &rstrValueRaw, CStringW &rstrValue)
{
	if (IsKnownUtf8SalvageValue(rstrSectionName, rstrEntryName) && ContainsNonAsciiByte(rstrValueRaw) && TryDecodeUtf8Strict(rstrValueRaw, rstrValue))
		return true;
	return TryDecodeAnsiExact(rstrValueRaw, rstrValue);
}

inline bool DecodeLegacyProfileLine(const ProfileLine &rLine, const CStringA &rstrSectionName, CStringW &rstrLineText)
{
	CStringA strEntryName;
	int iValueStart = -1;
	if (TryParseKeyValue(rLine.strText, strEntryName, iValueStart)) {
		CStringW strPrefix;
		CStringW strValue;
		if (!TryDecodeAnsiExact(rLine.strText.Left(iValueStart), strPrefix)
			|| !DecodeLegacyProfileValue(rstrSectionName, strEntryName, rLine.strText.Mid(iValueStart), strValue))
		{
			return false;
		}
		rstrLineText = strPrefix + strValue;
		return true;
	}

	return TryDecodeAnsiExact(rLine.strText, rstrLineText);
}

inline CStringW DecodeLegacyProfileBytesToWideText(const std::vector<unsigned char> &rBytes)
{
	std::vector<ProfileLine> legacyLines = SplitProfileLines(BytesToCStringA(rBytes));
	std::vector<ProfileLineW> wideLines;
	CStringA strCurrentSection;
	for (size_t i = 0; i < legacyLines.size(); ++i) {
		CStringA strLineSectionName;
		if (TryParseSectionName(legacyLines[i].strText, strLineSectionName))
			strCurrentSection = strLineSectionName;

		CStringW strLineText;
		if (DecodeLegacyProfileLine(legacyLines[i], strCurrentSection, strLineText))
			wideLines.push_back(ProfileLineW{ strLineText, CStringW(legacyLines[i].strLineEnding) });
	}
	return JoinWideProfileLines(wideLines);
}

inline bool LoadProfileAsWideText(const CString &rstrProfilePath, CStringW &rstrText, bool &rbExists)
{
	rstrText.Empty();
	rbExists = false;

	std::vector<unsigned char> fileBytes;
	if (!LongPathSeams::ReadAllBytes(rstrProfilePath, fileBytes)) {
		const DWORD dwLastError = ::GetLastError();
		return dwLastError == ERROR_FILE_NOT_FOUND || dwLastError == ERROR_PATH_NOT_FOUND;
	}

	rbExists = true;
	switch (DetectProfileTextEncoding(fileBytes))
	{
		case ProfileTextEncoding::Empty:
			return true;
		case ProfileTextEncoding::Utf16Le:
			rstrText = DecodeUtf16LeProfileText(fileBytes);
			return true;
		case ProfileTextEncoding::Ansi:
			rstrText = DecodeLegacyProfileBytesToWideText(fileBytes);
			return true;
		case ProfileTextEncoding::Unsupported:
			return true;
	}
	return true;
}

inline bool PersistProfileWideText(const CString &rstrProfilePath, const CStringW &rstrText)
{
	return LongPathSeams::WriteAllBytes(rstrProfilePath, EncodeUtf16LeProfileText(rstrText));
}

inline CStringW ReadProfileValueWide(const CString &rstrSection, const CString &rstrEntry, const CStringW &rstrDefaultValue, const CString &rstrProfilePath)
{
	CStringW strText;
	bool bExists = false;
	if (!LoadProfileAsWideText(rstrProfilePath, strText, bExists) || !bExists)
		return rstrDefaultValue;

	const CStringW strSectionName(rstrSection);
	const CStringW strEntryName(rstrEntry);
	const std::vector<ProfileLineW> lines = SplitWideProfileLines(strText);
	bool bInTargetSection = false;
	for (size_t i = 0; i < lines.size(); ++i) {
		CStringW strLineSectionName;
		if (TryParseWideSectionName(lines[i].strText, strLineSectionName)) {
			bInTargetSection = (strLineSectionName.CompareNoCase(strSectionName) == 0);
			continue;
		}
		if (!bInTargetSection)
			continue;

		CStringW strLineEntryName;
		int iValueStart = -1;
		if (TryParseWideKeyValue(lines[i].strText, strLineEntryName, iValueStart)
			&& strLineEntryName.CompareNoCase(strEntryName) == 0)
		{
			return lines[i].strText.Mid(iValueStart);
		}
	}

	return rstrDefaultValue;
}

inline void EnsureWritableWideLineEnding(ProfileLineW &rLine, const CStringW &rstrLineEnding)
{
	if (rLine.strLineEnding.IsEmpty())
		rLine.strLineEnding = rstrLineEnding;
}

inline bool UpsertProfileValueWide(const CString &rstrSection, const CString &rstrEntry, LPCTSTR pszValue, const CString &rstrProfilePath)
{
	CStringW strText;
	bool bExists = false;
	if (!LoadProfileAsWideText(rstrProfilePath, strText, bExists))
		return false;
	(void)bExists;

	std::vector<ProfileLineW> lines = SplitWideProfileLines(strText);
	const CStringW strLineEnding = ChooseWideLineEnding(lines);
	const CStringW strSectionName(rstrSection);
	const CStringW strEntryName(rstrEntry);
	const CStringW strNewLine = strEntryName + L"=" + CStringW(pszValue != NULL ? pszValue : _T(""));
	const int iSectionStart = FindWideSectionStartIndex(lines, strSectionName);
	if (iSectionStart >= 0) {
		const int iSectionEnd = FindWideSectionInsertIndex(lines, iSectionStart);
		for (int i = iSectionStart + 1; i < iSectionEnd; ++i) {
			CStringW strLineEntryName;
			int iValueStart = -1;
			if (TryParseWideKeyValue(lines[i].strText, strLineEntryName, iValueStart)
				&& strLineEntryName.CompareNoCase(strEntryName) == 0)
			{
				lines[i].strText = strNewLine;
				EnsureWritableWideLineEnding(lines[i], strLineEnding);
				return PersistProfileWideText(rstrProfilePath, JoinWideProfileLines(lines));
			}
		}

		lines.insert(lines.begin() + iSectionEnd, ProfileLineW{ strNewLine, strLineEnding });
		return PersistProfileWideText(rstrProfilePath, JoinWideProfileLines(lines));
	}

	if (!lines.empty() && !lines.back().strText.IsEmpty())
		lines.push_back(ProfileLineW{ CStringW(), strLineEnding });
	lines.push_back(ProfileLineW{ CStringW(L"[") + strSectionName + L"]", strLineEnding });
	lines.push_back(ProfileLineW{ strNewLine, CStringW() });
	return PersistProfileWideText(rstrProfilePath, JoinWideProfileLines(lines));
}

inline bool DeleteProfileKeyWide(const CString &rstrSection, const CString &rstrEntry, const CString &rstrProfilePath)
{
	CStringW strText;
	bool bExists = false;
	if (!LoadProfileAsWideText(rstrProfilePath, strText, bExists))
		return false;
	if (!bExists)
		return true;

	std::vector<ProfileLineW> lines = SplitWideProfileLines(strText);
	const CStringW strSectionName(rstrSection);
	const CStringW strEntryName(rstrEntry);
	const int iSectionStart = FindWideSectionStartIndex(lines, strSectionName);
	if (iSectionStart < 0)
		return true;

	const int iSectionEnd = FindWideSectionInsertIndex(lines, iSectionStart);
	bool bRemoved = false;
	for (int i = iSectionEnd - 1; i > iSectionStart; --i) {
		CStringW strLineEntryName;
		int iValueStart = -1;
		if (TryParseWideKeyValue(lines[i].strText, strLineEntryName, iValueStart)
			&& strLineEntryName.CompareNoCase(strEntryName) == 0)
		{
			lines.erase(lines.begin() + i);
			bRemoved = true;
		}
	}

	return !bRemoved || PersistProfileWideText(rstrProfilePath, JoinWideProfileLines(lines));
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
	return CString(detail::ReadProfileValueWide(rstrSection, rstrEntry, CStringW(pszDefaultValue != NULL ? pszDefaultValue : _T("")), rstrProfilePath));
}

inline bool WriteProfileStringLongPath(const CString &rstrSection, const CString &rstrEntry, LPCTSTR pszValue, const CString &rstrProfilePath)
{
	return detail::UpsertProfileValueWide(rstrSection, rstrEntry, pszValue, rstrProfilePath);
}

inline bool DeleteProfileKeyLongPath(const CString &rstrSection, const CString &rstrEntry, const CString &rstrProfilePath)
{
	return detail::DeleteProfileKeyWide(rstrSection, rstrEntry, rstrProfilePath);
}

/**
 * @brief Converts a profile file to UTF-16LE with BOM, preserving ANSI values and salvaging known prior UTF-8 values once.
 */
inline bool NormalizeProfileFileToUtf16Le(const CString &rstrProfilePath)
{
	std::vector<unsigned char> fileBytes;
	if (!LongPathSeams::ReadAllBytes(rstrProfilePath, fileBytes)) {
		const DWORD dwLastError = ::GetLastError();
		if (dwLastError != ERROR_FILE_NOT_FOUND && dwLastError != ERROR_PATH_NOT_FOUND)
			return false;
		return detail::PersistProfileWideText(rstrProfilePath, CStringW());
	}

	if (detail::DetectProfileTextEncoding(fileBytes) == detail::ProfileTextEncoding::Utf16Le)
		return true;

	const CStringW strNormalized = detail::DetectProfileTextEncoding(fileBytes) == detail::ProfileTextEncoding::Ansi
		? detail::DecodeLegacyProfileBytesToWideText(fileBytes)
		: CStringW();
	return detail::PersistProfileWideText(rstrProfilePath, strNormalized);
}
}
