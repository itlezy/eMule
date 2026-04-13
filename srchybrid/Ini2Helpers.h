#pragma once

#include <atlstr.h>
#include <windows.h>

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
}
