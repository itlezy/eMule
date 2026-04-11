#pragma once

#include <atlstr.h>

namespace I18nSeams
{
inline CString ExtractLanguageDllBaseName(const CString &rstrFileName)
{
	int iSeparator = rstrFileName.ReverseFind(_T('\\'));
	const int iAltSeparator = rstrFileName.ReverseFind(_T('/'));
	if (iAltSeparator > iSeparator)
		iSeparator = iAltSeparator;

	const int iNameStart = iSeparator + 1;
	int iNameEnd = rstrFileName.GetLength();
	const int iDot = rstrFileName.ReverseFind(_T('.'));
	if (iDot > iSeparator)
		iNameEnd = iDot;

	return rstrFileName.Mid(iNameStart, iNameEnd - iNameStart);
}
}
