#pragma once

#include <atlstr.h>

namespace DownloadQueueOverviewSeams
{
inline CString GetPartMetOverviewDisplayName(const CString &rstrPartFilePath, const CString &rstrFullName, const bool bSingleTempDir)
{
	if (!bSingleTempDir)
		return rstrFullName;

	int iSeparator = rstrPartFilePath.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrPartFilePath.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;

	if (iSeparator < 0 || iSeparator + 1 >= rstrPartFilePath.GetLength())
		return rstrPartFilePath;

	return rstrPartFilePath.Mid(iSeparator + 1);
}
}
