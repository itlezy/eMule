#pragma once

#include <atlstr.h>

namespace LogFileSeams
{
inline CString BuildRotatedLogFilePath(const CString &rstrFilePath, const CString &rstrTimestamp)
{
	int iSeparator = rstrFilePath.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrFilePath.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;

	const int iNameStart = iSeparator + 1;
	int iNameEnd = rstrFilePath.GetLength();
	const int iDot = rstrFilePath.ReverseFind(_T('.'));
	if (iDot > iSeparator)
		iNameEnd = iDot;

	CString strRotatedPath(rstrFilePath.Left(iNameStart));
	strRotatedPath += rstrFilePath.Mid(iNameStart, iNameEnd - iNameStart);
	strRotatedPath += _T(" - ");
	strRotatedPath += rstrTimestamp;
	if (iDot > iSeparator)
		strRotatedPath += rstrFilePath.Mid(iDot);
	return strRotatedPath;
}
}
