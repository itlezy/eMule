#pragma once

#include <atlstr.h>
#include <windows.h>

namespace Ini2Seams
{
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

inline CString EnsureTrailingSlash(const CString &rstrPath)
{
	if (rstrPath.IsEmpty())
		return rstrPath;
	if (rstrPath.Right(1) == _T("\\") || rstrPath.Right(1) == _T("/"))
		return rstrPath;
	return rstrPath + _T("\\");
}

inline CString ExtractDirectoryPath(const CString &rstrPath)
{
	int iSeparator = rstrPath.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrPath.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;
	if (iSeparator < 0)
		return CString();
	return rstrPath.Left(iSeparator + 1);
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
	return EnsureTrailingSlash(rstrBaseDirectory) + rstrRelativePath;
}

inline CString BuildDefaultIniFilePath(const CString &rstrModulePath, const CString &rstrCurrentDirectory, const bool bModulePath)
{
	if (rstrModulePath.IsEmpty())
		return CString();
	const CString strIniFileName = ExtractBaseNameWithoutExtension(rstrModulePath) + _T(".ini");
	return bModulePath
		? BuildPathFromBaseDirectory(ExtractDirectoryPath(rstrModulePath), strIniFileName)
		: BuildPathFromBaseDirectory(rstrCurrentDirectory, strIniFileName);
}

inline CString GetModuleFilePath()
{
	DWORD dwCapacity = MAX_PATH;
	CString strPath;
	for (;;) {
		LPTSTR pszBuffer = strPath.GetBuffer(dwCapacity);
		const DWORD dwLength = ::GetModuleFileName(NULL, pszBuffer, dwCapacity);
		if (dwLength == 0) {
			strPath.ReleaseBuffer(0);
			return CString();
		}
		if (dwLength < dwCapacity) {
			strPath.ReleaseBuffer(dwLength);
			return strPath;
		}
		strPath.ReleaseBuffer(dwCapacity);
		dwCapacity *= 2;
	}
}

inline CString GetCurrentDirectoryPath()
{
	DWORD dwCapacity = MAX_PATH;
	CString strPath;
	for (;;) {
		LPTSTR pszBuffer = strPath.GetBuffer(dwCapacity);
		const DWORD dwLength = ::GetCurrentDirectory(dwCapacity, pszBuffer);
		if (dwLength == 0) {
			strPath.ReleaseBuffer(0);
			return CString();
		}
		if (dwLength < dwCapacity) {
			strPath.ReleaseBuffer(dwLength);
			return strPath;
		}
		strPath.ReleaseBuffer(dwCapacity);
		dwCapacity = dwLength + 1;
	}
}
}
