#pragma once

#include <atlstr.h>
#include <tchar.h>
#include <vector>
#include <windows.h>
#include <ShlObj_core.h>

namespace PathHelperSeams
{
constexpr size_t kMaxDynamicPathChars = 32768u;

/**
 * @brief Returns true when the character is treated as a Win32 path separator.
 */
inline bool IsPathSeparator(const TCHAR ch)
{
	return ch == _T('\\') || ch == _T('/');
}

/**
 * @brief Normalizes forward slashes to backslashes before path composition or canonicalization.
 */
inline CString NormalizePathSeparators(const CString &rstrPath)
{
	CString strNormalized(rstrPath);
	strNormalized.Replace(_T('/'), _T('\\'));
	return strNormalized;
}

/**
 * @brief Grows the module-path buffer until `GetModuleFileName` returns the full path.
 */
template <typename GetModuleFileNameFn>
inline CString GetModuleFilePath(HMODULE hModule, GetModuleFileNameFn getModuleFileNameFn)
{
	std::vector<TCHAR> buffer(MAX_PATH, _T('\0'));
	while (buffer.size() < kMaxDynamicPathChars) {
		const DWORD dwCopied = getModuleFileNameFn(hModule, buffer.data(), static_cast<DWORD>(buffer.size()));
		if (dwCopied == 0)
			return CString();
		if (dwCopied < buffer.size()) {
			buffer[dwCopied] = _T('\0');
			return CString(buffer.data());
		}

		buffer.resize(buffer.size() * 2u, _T('\0'));
	}

	::SetLastError(ERROR_BUFFER_OVERFLOW);
	return CString();
}

/**
 * @brief Returns the module path through the real Win32 `GetModuleFileName` API.
 */
inline CString GetModuleFilePath(HMODULE hModule)
{
	return GetModuleFilePath(hModule, ::GetModuleFileName);
}

/**
 * @brief Queries a shell folder path into a dynamically sized buffer instead of a fixed `MAX_PATH` array.
 */
template <typename GetShellFolderPathFn>
inline CString GetShellFolderPath(int iCSIDL, GetShellFolderPathFn getShellFolderPathFn)
{
	std::vector<TCHAR> buffer(kMaxDynamicPathChars, _T('\0'));
	if (FAILED(getShellFolderPathFn(NULL, iCSIDL, NULL, SHGFP_TYPE_CURRENT, buffer.data())))
		return CString();
	buffer.back() = _T('\0');
	return CString(buffer.data());
}

/**
 * @brief Returns the shell folder path through the real `SHGetFolderPath` API.
 */
inline CString GetShellFolderPath(int iCSIDL)
{
	return GetShellFolderPath(
		iCSIDL,
		[](HWND hWnd, int iFolder, HANDLE hToken, DWORD dwFlags, LPTSTR pszPath) -> HRESULT {
			return ::SHGetFolderPath(hWnd, iFolder, hToken, dwFlags, pszPath);
		});
}

/**
 * @brief Appends a relative child path to a base path without relying on `PathCombine`.
 */
inline CString AppendPathComponent(const CString &rstrBasePath, LPCTSTR pszChildPath)
{
	if (pszChildPath == NULL || *pszChildPath == _T('\0'))
		return NormalizePathSeparators(rstrBasePath);
	if (rstrBasePath.IsEmpty())
		return NormalizePathSeparators(CString(pszChildPath));

	CString strCombined(NormalizePathSeparators(rstrBasePath));
	LPCTSTR pszSuffix = pszChildPath;
	while (IsPathSeparator(*pszSuffix))
		++pszSuffix;

	if (!strCombined.IsEmpty() && !IsPathSeparator(strCombined[strCombined.GetLength() - 1]))
		strCombined += _T('\\');
	strCombined += NormalizePathSeparators(CString(pszSuffix));
	return strCombined;
}

/**
 * @brief Returns the parent directory portion of a file path without using fixed buffers.
 */
inline CString GetDirectoryPath(const CString &rstrPath)
{
	const CString strNormalized(NormalizePathSeparators(rstrPath));
	const int iLastSlash = strNormalized.ReverseFind(_T('\\'));
	if (iLastSlash < 0)
		return CString();
	if (iLastSlash == 2 && strNormalized.GetLength() >= 3 && strNormalized[1] == _T(':'))
		return strNormalized.Left(iLastSlash + 1);
	return strNormalized.Left(iLastSlash);
}

struct ParsedPathRoot
{
	CString strPrefix;
	CString strRemainder;
	bool bAbsolute;
};

inline bool ReadNextPathSegment(const CString &rstrPath, int &iIndex, CString &rstrSegment)
{
	const int nLength = rstrPath.GetLength();
	while (iIndex < nLength && IsPathSeparator(rstrPath[iIndex]))
		++iIndex;
	if (iIndex >= nLength) {
		rstrSegment.Empty();
		return false;
	}

	const int iStart = iIndex;
	while (iIndex < nLength && !IsPathSeparator(rstrPath[iIndex]))
		++iIndex;
	rstrSegment = rstrPath.Mid(iStart, iIndex - iStart);
	return true;
}

inline bool TryParseUncRoot(const CString &rstrPath, const CString &rstrPrefix, ParsedPathRoot &rParsed)
{
	int iIndex = rstrPrefix.GetLength();
	CString strServer;
	CString strShare;
	if (!ReadNextPathSegment(rstrPath, iIndex, strServer) || !ReadNextPathSegment(rstrPath, iIndex, strShare))
		return false;

	rParsed.strPrefix.Format(_T("%s%s\\%s\\"), (LPCTSTR)rstrPrefix, (LPCTSTR)strServer, (LPCTSTR)strShare);
	rParsed.strRemainder = rstrPath.Mid(iIndex);
	rParsed.bAbsolute = true;
	return true;
}

inline ParsedPathRoot ParsePathRoot(const CString &rstrPath)
{
	const CString strNormalized(NormalizePathSeparators(rstrPath));
	ParsedPathRoot parsed = { CString(), strNormalized, false };

	if (strNormalized.Left(8).CompareNoCase(_T("\\\\?\\UNC\\")) == 0) {
		if (TryParseUncRoot(strNormalized, _T("\\\\?\\UNC\\"), parsed))
			return parsed;
		return parsed;
	}

	if (strNormalized.Left(4).CompareNoCase(_T("\\\\?\\")) == 0
		&& strNormalized.GetLength() >= 7
		&& strNormalized[5] == _T(':')
		&& IsPathSeparator(strNormalized[6]))
	{
		parsed.strPrefix = strNormalized.Left(7);
		parsed.strRemainder = strNormalized.Mid(7);
		parsed.bAbsolute = true;
		return parsed;
	}

	if (strNormalized.Left(2) == _T("\\\\")) {
		if (TryParseUncRoot(strNormalized, _T("\\\\"), parsed))
			return parsed;
		return parsed;
	}

	if (strNormalized.GetLength() >= 3
		&& ((strNormalized[0] >= _T('A') && strNormalized[0] <= _T('Z')) || (strNormalized[0] >= _T('a') && strNormalized[0] <= _T('z')))
		&& strNormalized[1] == _T(':')
		&& IsPathSeparator(strNormalized[2]))
	{
		parsed.strPrefix = strNormalized.Left(3);
		parsed.strRemainder = strNormalized.Mid(3);
		parsed.bAbsolute = true;
		return parsed;
	}

	if (strNormalized.GetLength() >= 2
		&& ((strNormalized[0] >= _T('A') && strNormalized[0] <= _T('Z')) || (strNormalized[0] >= _T('a') && strNormalized[0] <= _T('z')))
		&& strNormalized[1] == _T(':'))
	{
		parsed.strPrefix = strNormalized.Left(2);
		parsed.strRemainder = strNormalized.Mid(2);
		return parsed;
	}

	if (!strNormalized.IsEmpty() && IsPathSeparator(strNormalized[0])) {
		parsed.strPrefix = _T("\\");
		parsed.strRemainder = strNormalized.Mid(1);
		parsed.bAbsolute = true;
	}

	return parsed;
}

/**
 * @brief Lexically removes `.` and `..` segments without depending on `PathCanonicalize`.
 */
inline CString CanonicalizePath(const CString &rstrPath)
{
	if (rstrPath.IsEmpty())
		return CString();

	const ParsedPathRoot root = ParsePathRoot(rstrPath);
	std::vector<CString> segments;
	int iIndex = 0;
	CString strSegment;
	while (ReadNextPathSegment(root.strRemainder, iIndex, strSegment)) {
		if (strSegment.IsEmpty() || strSegment == _T("."))
			continue;

		if (strSegment == _T("..")) {
			if (!segments.empty() && segments.back() != _T("..")) {
				segments.pop_back();
				continue;
			}
			if (!root.bAbsolute)
				segments.push_back(strSegment);
			continue;
		}

		segments.push_back(strSegment);
	}

	CString strCanonical(root.strPrefix);
	for (size_t i = 0; i < segments.size(); ++i) {
		if (!strCanonical.IsEmpty()
			&& !IsPathSeparator(strCanonical[strCanonical.GetLength() - 1])
			&& !(strCanonical.GetLength() == 2 && strCanonical[1] == _T(':')))
		{
			strCanonical += _T('\\');
		}
		strCanonical += segments[i];
	}

	if (!strCanonical.IsEmpty())
		return strCanonical;
	return root.strPrefix.IsEmpty() ? CString(_T(".")) : root.strPrefix;
}

/**
 * @brief Formats a `res://` URL from the current module path without truncating overlong paths.
 */
template <typename GetModuleFileNameFn>
inline CString BuildModuleResourceBaseUrl(HMODULE hModule, GetModuleFileNameFn getModuleFileNameFn)
{
	const CString strModulePath(GetModuleFilePath(hModule, getModuleFileNameFn));
	if (strModulePath.IsEmpty())
		return CString();

	CString strResourceUrl;
	strResourceUrl.Format(_T("res://%s"), (LPCTSTR)strModulePath);
	return strResourceUrl;
}

/**
 * @brief Formats a `res://` URL from the real module path.
 */
inline CString BuildModuleResourceBaseUrl(HMODULE hModule)
{
	return BuildModuleResourceBaseUrl(hModule, ::GetModuleFileName);
}
}

#define EMULE_TEST_HAVE_PATH_HELPER_SEAMS 1
