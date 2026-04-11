#pragma once

#include <atlstr.h>

namespace PerfLogSeams
{
inline CString BuildMrtgSidecarPath(const CString &rstrConfiguredPath, LPCTSTR pszSuffixWithExtension)
{
	int iSeparator = rstrConfiguredPath.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrConfiguredPath.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;

	int iNameEnd = rstrConfiguredPath.GetLength();
	const int iDot = rstrConfiguredPath.ReverseFind(_T('.'));
	if (iDot > iSeparator)
		iNameEnd = iDot;

	CString strBasePath(rstrConfiguredPath.Left(iNameEnd));
	strBasePath += pszSuffixWithExtension;
	return strBasePath;
}
}
