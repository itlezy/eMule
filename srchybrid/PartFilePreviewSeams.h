#pragma once

#include <atlstr.h>

namespace PartFilePreviewSeams
{
inline CString ExtractConfiguredVideoPlayerBaseName(const CString &rstrVideoPlayerPath)
{
	int iSeparator = rstrVideoPlayerPath.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrVideoPlayerPath.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;

	const int iNameStart = iSeparator + 1;
	int iNameEnd = rstrVideoPlayerPath.GetLength();
	const int iDot = rstrVideoPlayerPath.ReverseFind(_T('.'));
	if (iDot > iSeparator)
		iNameEnd = iDot;

	return rstrVideoPlayerPath.Mid(iNameStart, iNameEnd - iNameStart);
}
}
